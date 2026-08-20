#include "ChatManager.hpp"
#include "UrlUtils.hpp"
#include "Logging.hpp"

#include "UnityEngine/WaitForSecondsRealtime.hpp"

#include <algorithm>

namespace YouTubeLiveChat {

void ChatManager::Configure(const ModConfig& config) {
    config_ = config;
    auth_.Configure(config);
    apiClient_.Configure(config, &auth_);
}

void ChatManager::SetState(ConnectionState s, std::string detail) {
    state_ = s;
    statusDetail_ = detail;
    if (stateCallback_) stateCallback_(s, std::move(detail));
}

bool ChatManager::StartCoroutine(custom_types::Helpers::Coroutine coroutine) {
    if (!host_) {
        Log().error("ChatManager has no host MonoBehaviour; cannot start coroutine");
        return false;
    }
    System::Collections::IEnumerator* enumerator =
        custom_types::Helpers::CoroutineHelper::New(std::move(coroutine));
    host_->StartCoroutine(enumerator);
    return true;
}

void ChatManager::Connect() {
    wantsConnection_ = true;
    generation_++;
    if (!StartCoroutine(RunLoop(generation_))) {
        wantsConnection_ = false;
    }
}

void ChatManager::Disconnect() {
    wantsConnection_ = false;
    generation_++;  // invalidates any in-flight RunLoop's next iteration
    SetState(ConnectionState::Disconnected);
}

void ChatManager::BeginSignIn() {
    if (signingIn_) return;
    signingIn_ = true;
    if (!StartCoroutine(RunSignIn())) {
        signingIn_ = false;
    }
}

void ChatManager::SignOut() {
    auth_.Clear();
    config_.oauthRefreshToken.clear();
    config_.oauthAccountName.clear();
    if (signInCompleteCallback_) signInCompleteCallback_(false, "", "");
    Disconnect();
}

custom_types::Helpers::Coroutine ChatManager::WaitSeconds(float seconds) {
    // Realtime, not scaled: Beat Saber pauses/scales Time.timeScale during
    // gameplay transitions and we still want chat to keep ticking.
    co_yield UnityEngine::WaitForSecondsRealtime::New_ctor(seconds)->i___System__Collections__IEnumerator();
    co_return;
}

custom_types::Helpers::Coroutine ChatManager::RunSignIn() {
    SetState(ConnectionState::Connecting, "Asking Google for a sign-in code...");

    DeviceCodeResult device{};
    co_yield custom_types::Helpers::CoroutineHelper::New(
        auth_.RequestDeviceCode([&device](DeviceCodeResult r) { device = std::move(r); }));

    if (!device.ok) {
        signingIn_ = false;
        SetState(ConnectionState::AuthOrApiError, device.error);
        if (signInPromptCallback_) signInPromptCallback_("", "");
        if (signInCompleteCallback_) signInCompleteCallback_(false, "", device.error);
        co_return;
    }

    if (signInPromptCallback_) signInPromptCallback_(device.userCode, device.verificationUrl);
    SetState(ConnectionState::Connecting,
             "Go to " + device.verificationUrl + " and enter " + device.userCode);

    float interval = static_cast<float>(std::max(device.intervalSeconds, 5));
    float waited = 0.0f;
    const float deadline = static_cast<float>(device.expiresInSeconds);

    while (signingIn_ && waited < deadline) {
        co_yield custom_types::Helpers::CoroutineHelper::New(WaitSeconds(interval));
        waited += interval;

        TokenResult token{};
        co_yield custom_types::Helpers::CoroutineHelper::New(
            auth_.PollDeviceToken(device.deviceCode, [&token](TokenResult r) { token = std::move(r); }));

        if (token.ok) {
            signingIn_ = false;
            if (signInPromptCallback_) signInPromptCallback_("", "");
            if (signInCompleteCallback_) signInCompleteCallback_(true, token.refreshToken, "");
            SetState(ConnectionState::Disconnected, "Signed in to YouTube.");
            // A successful sign-in is the moment the user is waiting for --
            // go straight to chat instead of making them press Connect.
            Connect();
            co_return;
        }
        if (token.denied) {
            signingIn_ = false;
            if (signInPromptCallback_) signInPromptCallback_("", "");
            if (signInCompleteCallback_) signInCompleteCallback_(false, "", token.error);
            SetState(ConnectionState::NeedsSignIn, token.error);
            co_return;
        }
        if (token.slowDown) {
            interval += 5.0f;
        }
        if (!token.pending && !token.error.empty()) {
            signingIn_ = false;
            if (signInPromptCallback_) signInPromptCallback_("", "");
            if (signInCompleteCallback_) signInCompleteCallback_(false, "", token.error);
            SetState(ConnectionState::AuthOrApiError, token.error);
            co_return;
        }
    }

    signingIn_ = false;
    if (signInPromptCallback_) signInPromptCallback_("", "");
    if (signInCompleteCallback_) signInCompleteCallback_(false, "", "Sign-in timed out.");
    SetState(ConnectionState::NeedsSignIn, "Sign-in timed out. Try again.");
    co_return;
}

custom_types::Helpers::Coroutine ChatManager::RunLoop(uint32_t myGeneration) {
    auto stillCurrent = [this, myGeneration]() {
        return myGeneration == generation_ && wantsConnection_;
    };

    const bool oauth = config_.authMode == AuthMode::OAuth;
    // With OAuth we can ask "what am I streaming right now", so a video ID is
    // optional; with an API key there is nothing to look up without one.
    const bool useOwnBroadcast = oauth && config_.useOwnBroadcast;

    if (oauth) {
        if (config_.oauthClientId.empty() || config_.oauthClientSecret.empty()) {
            SetState(ConnectionState::InvalidConfiguration,
                     "Enter your OAuth client ID and secret, then sign in.");
            co_return;
        }
        if (!auth_.HasRefreshToken()) {
            SetState(ConnectionState::NeedsSignIn, "Press \"Sign in with YouTube\" to connect.");
            co_return;
        }
    } else if (config_.apiKey.empty()) {
        SetState(ConnectionState::InvalidConfiguration, "Set a YouTube Data API key in settings.");
        co_return;
    }

    std::string videoId;
    if (!useOwnBroadcast) {
        if (config_.videoIdOrUrl.empty()) {
            SetState(ConnectionState::InvalidConfiguration, "Set a video ID or URL in settings.");
            co_return;
        }
        auto parsed = UrlUtils::ExtractVideoId(config_.videoIdOrUrl);
        if (!parsed) {
            SetState(ConnectionState::InvalidConfiguration, "Couldn't find a video ID in that value.");
            co_return;
        }
        videoId = *parsed;
    }

    SetState(ConnectionState::Connecting);

    std::string liveChatId;
    int networkErrorBackoffSeconds = 3;

    while (stillCurrent()) {
        // ---- Step 1: find the active live chat ----
        if (liveChatId.empty()) {
            LiveChatLookupResult lookup{};
            if (useOwnBroadcast) {
                co_yield custom_types::Helpers::CoroutineHelper::New(
                    apiClient_.ResolveOwnLiveChatId([&lookup](LiveChatLookupResult r) { lookup = std::move(r); }));
            } else {
                co_yield custom_types::Helpers::CoroutineHelper::New(
                    apiClient_.ResolveLiveChatId(videoId, [&lookup](LiveChatLookupResult r) { lookup = std::move(r); }));
            }
            if (!stillCurrent()) co_return;

            if (lookup.authError) {
                SetState(oauth ? ConnectionState::NeedsSignIn : ConnectionState::AuthOrApiError,
                         lookup.errorMessage.empty()
                             ? "Authorization was rejected. Check your credentials."
                             : lookup.errorMessage);
                co_return;
            }
            if (lookup.quotaExceeded) {
                SetState(ConnectionState::RateLimited, "Daily YouTube API quota exceeded.");
                co_return;
            }
            if (!lookup.found) {
                if (lookup.errorMessage.empty() || lookup.errorMessage == "Video not found") {
                    SetState(ConnectionState::InvalidConfiguration,
                             "Video not found (deleted, private, or wrong ID).");
                    co_return;  // not worth retrying an ID that doesn't exist
                }
                // A transient lookup failure -- back off and try again.
                SetState(ConnectionState::NetworkError, lookup.errorMessage);
                if (!config_.autoReconnect) co_return;
                co_yield custom_types::Helpers::CoroutineHelper::New(
                    WaitSeconds(static_cast<float>(networkErrorBackoffSeconds)));
                if (!stillCurrent()) co_return;
                networkErrorBackoffSeconds = std::min(networkErrorBackoffSeconds * 2, 60);
                continue;
            }
            if (!lookup.isLive) {
                SetState(ConnectionState::StreamOffline,
                         useOwnBroadcast
                             ? "You're not live right now. Waiting..."
                             : (lookup.videoTitle.empty()
                                    ? "That stream isn't live right now."
                                    : ("\"" + lookup.videoTitle + "\" isn't live right now.")));
                co_yield custom_types::Helpers::CoroutineHelper::New(WaitSeconds(20.0f));
                if (!stillCurrent()) co_return;
                continue;  // re-check for the stream going live
            }

            liveChatId = lookup.liveChatId;
            networkErrorBackoffSeconds = 3;
            SetState(ConnectionState::Connecting,
                     lookup.videoTitle.empty() ? "Joining chat..." : ("Joined: " + lookup.videoTitle));
        }

        // ---- Step 2: poll liveChatMessages.list ----
        // The first page is the chat backlog; showing all of it at once would
        // flood the overlay with stale messages, so it is dropped and only
        // what arrives afterwards is displayed.
        bool firstPage = true;
        std::string pageToken;
        bool restartLookup = false;

        while (stillCurrent() && !restartLookup) {
            ChatPollResult result{};
            co_yield custom_types::Helpers::CoroutineHelper::New(
                apiClient_.FetchMessages(liveChatId, pageToken,
                                         [&result](ChatPollResult r) { result = std::move(r); }));
            if (!stillCurrent()) co_return;

            if (result.ok) {
                networkErrorBackoffSeconds = 3;
                if (state_ != ConnectionState::Connected) SetState(ConnectionState::Connected);
                if (!firstPage && !result.messages.empty() && messagesCallback_) {
                    messagesCallback_(result.messages);
                }
                firstPage = false;
                pageToken = result.nextPageToken;

                float waitSecs = std::max<float>(
                    config_.minPollIntervalSeconds,
                    static_cast<float>(result.pollingIntervalMillis) / 1000.0f);
                co_yield custom_types::Helpers::CoroutineHelper::New(WaitSeconds(waitSecs));
                continue;
            }

            // --- error handling ---
            if (result.chatEnded) {
                // Stream ended mid-poll: go back to step 1, which will either
                // pick up a new broadcast or park in "offline".
                liveChatId.clear();
                restartLookup = true;
                continue;
            }
            if (result.authError) {
                SetState(oauth ? ConnectionState::NeedsSignIn : ConnectionState::AuthOrApiError,
                         result.errorMessage.empty()
                             ? "Check that your credentials are valid and the YouTube Data API v3 is enabled."
                             : result.errorMessage);
                co_return;  // needs the user to fix something; don't hammer the API
            }
            if (result.quotaExceeded) {
                SetState(ConnectionState::RateLimited, "Daily YouTube API quota exceeded.");
                co_return;  // quota resets daily; a retry loop would just burn more
            }

            // Generic/network error.
            SetState(ConnectionState::NetworkError, result.errorMessage);
            if (!config_.autoReconnect) co_return;

            co_yield custom_types::Helpers::CoroutineHelper::New(
                WaitSeconds(static_cast<float>(networkErrorBackoffSeconds)));
            if (!stillCurrent()) co_return;
            networkErrorBackoffSeconds = std::min(networkErrorBackoffSeconds * 2, 60);
        }
    }
    co_return;
}

}  // namespace YouTubeLiveChat

#include "ChatManager.hpp"
#include "UrlUtils.hpp"

#include "UnityEngine/WaitForSecondsRealtime.hpp"
#include "custom-types/shared/coroutine.hpp"

#include "paper2_scotland2/shared/logger.hpp"

#include <algorithm>

static auto& log() {
    static auto logger = Paper::ConstLoggerContext("YTLiveChat.Manager");
    return logger;
}

namespace YouTubeLiveChat {

void ChatManager::Configure(const ModConfig& config) {
    config_ = config;
    if (!apiClient_) {
        apiClient_ = std::make_unique<YouTubeApiClient>(config_.apiKey);
    } else {
        apiClient_->SetApiKey(config_.apiKey);
    }
}

void ChatManager::SetState(ConnectionState s, std::string detail) {
    state_ = s;
    if (stateCallback_) stateCallback_(s, std::move(detail));
}

void ChatManager::Connect() {
    if (!host_) {
        log().error("ChatManager::Connect called with no host MonoBehaviour set");
        return;
    }
    wantsConnection_ = true;
    generation_++;
    uint32_t myGen = generation_;
    host_->StartCoroutine(
        reinterpret_cast<System::Collections::IEnumerator*>(
            custom_types::Helpers::CoroutineHelper::New(RunLoop(myGen))
        )
    );
}

void ChatManager::Disconnect() {
    wantsConnection_ = false;
    generation_++; // invalidates any in-flight RunLoop coroutine's next iteration
    SetState(ConnectionState::Disconnected);
}

custom_types::Helpers::Coroutine ChatManager::WaitSeconds(float seconds) {
    co_yield reinterpret_cast<System::Collections::IEnumerator*>(
        UnityEngine::WaitForSecondsRealtime::New_ctor(seconds));
    co_return;
}

custom_types::Helpers::Coroutine ChatManager::RunLoop(uint32_t myGeneration) {
    auto stillCurrent = [&]() { return myGeneration == generation_ && wantsConnection_; };

    if (config_.apiKey.empty() || config_.videoIdOrUrl.empty()) {
        SetState(ConnectionState::InvalidConfiguration, "Set an API key and a video ID/URL in settings.");
        co_return;
    }

    auto videoId = UrlUtils::ExtractVideoId(config_.videoIdOrUrl);
    if (!videoId) {
        SetState(ConnectionState::InvalidConfiguration, "Couldn't find a video ID in that value.");
        co_return;
    }

    SetState(ConnectionState::Connecting);

    std::string liveChatId;
    int networkErrorBackoffSeconds = 3;

    while (stillCurrent()) {
        // ---- Step 1: resolve the video -> activeLiveChatId ----
        if (liveChatId.empty()) {
            bool resolved = false;
            LiveChatLookupResult lookup{};
            co_yield reinterpret_cast<System::Collections::IEnumerator*>(
                custom_types::Helpers::CoroutineHelper::New(
                    apiClient_->ResolveLiveChatId(*videoId, [&](LiveChatLookupResult r) {
                        lookup = r;
                        resolved = true;
                    })
                )
            );
            if (!stillCurrent()) co_return;

            if (!lookup.found) {
                SetState(ConnectionState::InvalidConfiguration, "Video not found (deleted, private, or wrong ID).");
                co_return; // not worth auto-retrying a video ID that doesn't exist
            }
            if (!lookup.isLive) {
                SetState(ConnectionState::StreamOffline, lookup.videoTitle.empty() ? "" : ("\"" + lookup.videoTitle + "\" is not currently live."));
                co_yield reinterpret_cast<System::Collections::IEnumerator*>(
                    custom_types::Helpers::CoroutineHelper::New(WaitSeconds(15.0f)));
                if (!stillCurrent()) co_return;
                continue; // re-check for the stream going live
            }

            liveChatId = lookup.liveChatId;
            SetState(ConnectionState::Connecting, "Stream found, joining chat...");
        }

        // ---- Step 2: poll liveChatMessages.list ----
        std::string pageToken;
        bool pollFailedHard = false;

        while (stillCurrent() && !pollFailedHard) {
            ChatPollResult result{};
            bool done = false;
            co_yield reinterpret_cast<System::Collections::IEnumerator*>(
                custom_types::Helpers::CoroutineHelper::New(
                    apiClient_->FetchMessages(liveChatId, pageToken, [&](ChatPollResult r) {
                        result = r;
                        done = true;
                    })
                )
            );
            if (!stillCurrent()) co_return;

            if (result.ok) {
                networkErrorBackoffSeconds = 3; // reset backoff on success
                SetState(ConnectionState::Connected);
                if (!result.messages.empty() && messagesCallback_) {
                    messagesCallback_(result.messages);
                }
                pageToken = result.nextPageToken;

                float waitSecs = std::max<float>(
                    config_.minPollIntervalSeconds,
                    static_cast<float>(result.pollingIntervalMillis) / 1000.0f);
                co_yield reinterpret_cast<System::Collections::IEnumerator*>(
                    custom_types::Helpers::CoroutineHelper::New(WaitSeconds(waitSecs)));
                continue;
            }

            // --- error handling ---
            if (result.authError) {
                SetState(ConnectionState::AuthOrApiError, result.errorMessage.empty()
                    ? "Check that your API key is valid and the YouTube Data API v3 is enabled."
                    : result.errorMessage);
                co_return; // requires the user to fix the key; don't hammer the API
            }
            if (result.quotaExceeded) {
                SetState(ConnectionState::RateLimited, "Daily YouTube API quota exceeded.");
                if (!config_.autoReconnect) co_return;
                co_yield reinterpret_cast<System::Collections::IEnumerator*>(
                    custom_types::Helpers::CoroutineHelper::New(WaitSeconds(300.0f))); // quota resets daily; don't spin
                if (!stillCurrent()) co_return;
                co_return; // let the outer Connect() call (or a future manual retry) start fresh
            }
            if (result.errorMessage == "liveChatEnded") {
                liveChatId.clear(); // fall back to Step 1 to see if a new chat/stream picks up
                pollFailedHard = true;
                continue;
            }

            // Generic/network error.
            SetState(ConnectionState::NetworkError, result.errorMessage);
            if (!config_.autoReconnect) co_return;

            co_yield reinterpret_cast<System::Collections::IEnumerator*>(
                custom_types::Helpers::CoroutineHelper::New(WaitSeconds(static_cast<float>(networkErrorBackoffSeconds))));
            if (!stillCurrent()) co_return;
            networkErrorBackoffSeconds = std::min(networkErrorBackoffSeconds * 2, 60); // exponential, capped at 60s
        }
    }
    co_return;
}

}

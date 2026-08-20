#pragma once
#include <string>
#include <vector>
#include <functional>

#include "custom-types/shared/coroutine.hpp"
#include "ChatTypes.hpp"
#include "Config.hpp"
#include "YouTubeAuth.hpp"

// ---------------------------------------------------------------------------
// Read-only client for the official YouTube Data API v3:
//   GET /youtube/v3/videos?part=snippet,liveStreamingDetails&id=<videoId>
//   GET /youtube/v3/liveBroadcasts?part=snippet&mine=true&broadcastStatus=active
//   GET /youtube/v3/liveChat/messages?liveChatId=<id>&part=snippet,authorDetails
//
// Requests are authorized either with an API key (`key=` query parameter,
// public chat only) or with an OAuth bearer token (see YouTubeAuth). The
// liveBroadcasts endpoint is OAuth-only -- `mine=true` has no meaning for an
// anonymous key.
// ---------------------------------------------------------------------------

namespace YouTubeLiveChat {

struct LiveChatLookupResult {
    bool found = false;
    bool isLive = false;
    bool authError = false;
    bool quotaExceeded = false;
    std::string liveChatId;
    std::string channelTitle;
    std::string videoTitle;
    std::string errorMessage;
};

struct ChatPollResult {
    bool ok = false;
    long httpStatus = 0;
    bool quotaExceeded = false;
    bool authError = false;
    bool chatEnded = false;
    std::vector<ChatMessage> messages;
    std::string nextPageToken;
    int64_t pollingIntervalMillis = 5000;
    std::string errorMessage;
};

class YouTubeApiClient {
public:
    // auth may be null when running in API-key mode.
    void Configure(const ModConfig& config, YouTubeAuth* auth);

    // Resolves a video ID to its active live chat ID.
    custom_types::Helpers::Coroutine ResolveLiveChatId(
        std::string videoId,
        std::function<void(LiveChatLookupResult)> callback);

    // OAuth only: finds the signed-in account's currently active broadcast.
    custom_types::Helpers::Coroutine ResolveOwnLiveChatId(
        std::function<void(LiveChatLookupResult)> callback);

    // Fetches one page of chat messages. Pass the previous response's
    // nextPageToken back in on subsequent calls; pass "" for the first call.
    custom_types::Helpers::Coroutine FetchMessages(
        std::string liveChatId,
        std::string pageToken,
        std::function<void(ChatPollResult)> callback);

    bool UsingOAuth() const { return authMode_ == AuthMode::OAuth; }

private:
    AuthMode authMode_ = AuthMode::ApiKey;
    std::string apiKey_;
    YouTubeAuth* auth_ = nullptr;

    // Appends `key=` when in API-key mode; returns the bearer token (possibly
    // empty) to send in OAuth mode. Refreshes the access token if needed.
    custom_types::Helpers::Coroutine PrepareRequest(
        std::function<void(bool ok, std::string bearerToken, std::string error)> callback);

    std::string AuthorizedUrl(const std::string& baseUrl) const;
};

}  // namespace YouTubeLiveChat

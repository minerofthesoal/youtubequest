#pragma once
#include <string>
#include <vector>
#include <functional>
#include <optional>

#include "custom-types/shared/coroutine.hpp"
#include "ChatTypes.hpp"

// ---------------------------------------------------------------------------
// Talks to the *official* YouTube Data API v3 over HTTPS, read-only:
//   GET https://www.googleapis.com/youtube/v3/videos
//       ?part=snippet,liveStreamingDetails&id=<videoId>&key=<apiKey>
//   GET https://www.googleapis.com/youtube/v3/liveChat/messages
//       ?liveChatId=<id>&part=snippet,authorDetails&key=<apiKey>&pageToken=<token>
//
// Both endpoints accept a plain API key -- reading public live chat does NOT
// require OAuth (posting/deleting messages would, but this mod is read-only,
// so we deliberately don't implement an OAuth flow at all: it would be a
// large amount of extra attack surface and UX complexity on a device with no
// easy embedded browser, for zero benefit to "display chat in my headset").
// See README section "Why no OAuth" for the full reasoning.
//
// Networking is done via UnityEngine.Networking.UnityWebRequest (through
// codegen) rather than bundling a separate HTTP/TLS stack -- the game
// already links this, it's asynchronous under the hood, and it avoids
// pulling in libcurl/OpenSSL as new native dependencies just for this.
// ---------------------------------------------------------------------------

namespace YouTubeLiveChat {

struct LiveChatLookupResult {
    bool found = false;
    bool isLive = false;
    std::string liveChatId;
    std::string channelTitle;
    std::string videoTitle;
};

struct ChatPollResult {
    bool ok = false;
    long httpStatus = 0;
    bool quotaExceeded = false;
    bool authError = false;
    std::vector<ChatMessage> messages;
    std::string nextPageToken;
    int64_t pollingIntervalMillis = 5000;
    std::string errorMessage;
};

class YouTubeApiClient {
public:
    explicit YouTubeApiClient(std::string apiKey) : apiKey_(std::move(apiKey)) {}

    void SetApiKey(std::string apiKey) { apiKey_ = std::move(apiKey); }

    // Resolves a video ID to its active live chat ID. Call once when the user
    // connects; re-call if the stream drops (the chat can go inactive when
    // the broadcaster ends the stream).
    custom_types::Helpers::Coroutine ResolveLiveChatId(
        std::string videoId,
        std::function<void(LiveChatLookupResult)> callback);

    // Fetches one page of chat messages. Pass the previous response's
    // nextPageToken back in on subsequent calls; pass "" for the first call.
    custom_types::Helpers::Coroutine FetchMessages(
        std::string liveChatId,
        std::string pageToken,
        std::function<void(ChatPollResult)> callback);

private:
    std::string apiKey_;

    custom_types::Helpers::Coroutine SendGet(
        std::string url,
        std::function<void(bool success, long httpStatus, std::string body)> callback);
};

}

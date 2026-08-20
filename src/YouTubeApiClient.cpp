#include "YouTubeApiClient.hpp"
#include "Http.hpp"
#include "UrlUtils.hpp"
#include "Logging.hpp"

#include "rapidjson/document.h"

#include <cstdlib>

namespace {

constexpr const char* kVideosUrl = "https://www.googleapis.com/youtube/v3/videos";
constexpr const char* kBroadcastsUrl = "https://www.googleapis.com/youtube/v3/liveBroadcasts";
constexpr const char* kMessagesUrl = "https://www.googleapis.com/youtube/v3/liveChat/messages";

std::string GetStr(const rapidjson::Value& v, const char* key, const std::string& def = "") {
    if (v.HasMember(key) && v[key].IsString()) return v[key].GetString();
    return def;
}

// Pulls apart the standard Google API error envelope so the UI can say
// something more useful than the HTTP status.
struct ApiError {
    bool quotaExceeded = false;
    bool authError = false;
    bool chatEnded = false;
    std::string message;
};

ApiError ParseApiError(long status, const std::string& body) {
    ApiError out{};
    if (status == 401) out.authError = true;
    // 403 covers both "out of quota" and "this key/token isn't allowed"; the
    // reason string below is what actually tells them apart.
    if (status == 403) out.quotaExceeded = true;

    rapidjson::Document doc;
    if (doc.Parse(body.c_str()).HasParseError() || !doc.IsObject() ||
        !doc.HasMember("error") || !doc["error"].IsObject()) {
        if (out.message.empty()) out.message = "HTTP " + std::to_string(status);
        return out;
    }

    const auto& err = doc["error"];
    out.message = GetStr(err, "message", "Unknown API error");
    if (err.HasMember("errors") && err["errors"].IsArray() && !err["errors"].Empty()) {
        std::string reason = GetStr(err["errors"][0], "reason");
        if (reason == "quotaExceeded" || reason == "dailyLimitExceeded" || reason == "rateLimitExceeded") {
            out.quotaExceeded = true;
            out.authError = false;
        } else if (reason == "keyInvalid" || reason == "authError" || reason == "forbidden" ||
                   reason == "insufficientPermissions" || reason == "liveChatDisabled") {
            out.authError = true;
            out.quotaExceeded = false;
        } else if (reason == "liveChatNotFound" || reason == "liveChatEnded") {
            out.chatEnded = true;
            out.authError = false;
            out.quotaExceeded = false;
        }
    }
    return out;
}

void ParseAuthorDetails(const rapidjson::Value& item, YouTubeLiveChat::ChatMessage& msg) {
    if (!item.HasMember("authorDetails") || !item["authorDetails"].IsObject()) return;
    const auto& a = item["authorDetails"];
    msg.author.channelId = GetStr(a, "channelId");
    msg.author.displayName = GetStr(a, "displayName");
    msg.author.profileImageUrl = GetStr(a, "profileImageUrl");
    if (a.HasMember("isChatOwner") && a["isChatOwner"].IsBool()) msg.author.isChatOwner = a["isChatOwner"].GetBool();
    if (a.HasMember("isChatModerator") && a["isChatModerator"].IsBool()) msg.author.isChatModerator = a["isChatModerator"].GetBool();
    if (a.HasMember("isChatSponsor") && a["isChatSponsor"].IsBool()) msg.author.isChatSponsor = a["isChatSponsor"].GetBool();
    if (a.HasMember("isVerified") && a["isVerified"].IsBool()) msg.author.isVerified = a["isVerified"].GetBool();
}

// Returns false for events with nothing worth showing (tombstones, poll
// events, sponsor-only-mode toggles, ...).
bool ParseMessageBody(const rapidjson::Value& snip, const std::string& type,
                      YouTubeLiveChat::ChatMessage& msg) {
    using YouTubeLiveChat::ChatEventType;

    if (type == "textMessageEvent") {
        msg.type = ChatEventType::TextMessage;
        if (snip.HasMember("textMessageDetails") && snip["textMessageDetails"].IsObject())
            msg.displayText = GetStr(snip["textMessageDetails"], "messageText");
        return true;
    }
    if (type == "superChatEvent") {
        msg.type = ChatEventType::SuperChat;
        if (snip.HasMember("superChatDetails") && snip["superChatDetails"].IsObject()) {
            const auto& sc = snip["superChatDetails"];
            msg.amountDisplayString = GetStr(sc, "amountDisplayString");
            msg.currency = GetStr(sc, "currency");
            if (sc.HasMember("amountMicros") && sc["amountMicros"].IsString()) {
                msg.amountMicros = std::strtoll(sc["amountMicros"].GetString(), nullptr, 10);
            }
            if (sc.HasMember("tier") && sc["tier"].IsInt()) msg.tier = sc["tier"].GetInt();
            msg.displayText = GetStr(sc, "userComment");
        }
        return true;
    }
    if (type == "superStickerEvent") {
        msg.type = ChatEventType::SuperSticker;
        if (snip.HasMember("superStickerDetails") && snip["superStickerDetails"].IsObject()) {
            const auto& ss = snip["superStickerDetails"];
            msg.amountDisplayString = GetStr(ss, "amountDisplayString");
            msg.currency = GetStr(ss, "currency");
            if (ss.HasMember("tier") && ss["tier"].IsInt()) msg.tier = ss["tier"].GetInt();
        }
        // The sticker itself is an image asset with no text form in the API.
        msg.displayText = "[Super Sticker]";
        return true;
    }
    if (type == "newSponsorEvent") {
        msg.type = ChatEventType::NewMember;
        msg.displayText = "just became a member!";
        return true;
    }
    if (type == "memberMilestoneChatEvent") {
        msg.type = ChatEventType::MemberMilestone;
        std::string comment;
        int months = 0;
        if (snip.HasMember("memberMilestoneChatDetails") && snip["memberMilestoneChatDetails"].IsObject()) {
            const auto& d = snip["memberMilestoneChatDetails"];
            comment = GetStr(d, "userComment");
            if (d.HasMember("memberMonth") && d["memberMonth"].IsInt()) months = d["memberMonth"].GetInt();
        }
        std::string prefix = months > 0 ? ("member for " + std::to_string(months) + " months") : "member milestone";
        msg.displayText = comment.empty() ? prefix : (prefix + " - " + comment);
        return true;
    }
    if (type == "membershipGiftingEvent") {
        msg.type = ChatEventType::MembershipGifting;
        msg.displayText = "gifted memberships!";
        return true;
    }
    if (type == "giftMembershipReceivedEvent") {
        msg.type = ChatEventType::GiftMembershipReceived;
        msg.displayText = "received a gifted membership!";
        return true;
    }
    if (type == "messageDeletedEvent") {
        msg.type = ChatEventType::MessageDeleted;
        msg.displayText = "[message deleted by moderator]";
        return true;
    }
    if (type == "messageRetractedEvent") {
        msg.type = ChatEventType::MessageRetracted;
        msg.displayText = "[message retracted]";
        return true;
    }
    if (type == "userBannedEvent") {
        msg.type = ChatEventType::UserBanned;
        msg.displayText = "[user banned]";
        return true;
    }
    return false;
}

}  // namespace

namespace YouTubeLiveChat {

void YouTubeApiClient::Configure(const ModConfig& config, YouTubeAuth* auth) {
    authMode_ = config.authMode;
    apiKey_ = config.apiKey;
    auth_ = auth;
}

std::string YouTubeApiClient::AuthorizedUrl(const std::string& baseUrl) const {
    if (authMode_ == AuthMode::ApiKey) {
        return baseUrl + "&key=" + UrlUtils::UrlEncode(apiKey_);
    }
    return baseUrl;
}

custom_types::Helpers::Coroutine YouTubeApiClient::PrepareRequest(
    std::function<void(bool, std::string, std::string)> callback) {
    if (authMode_ == AuthMode::ApiKey) {
        if (apiKey_.empty()) {
            callback(false, "", "No API key set.");
        } else {
            callback(true, "", "");
        }
        co_return;
    }

    if (!auth_) {
        callback(false, "", "OAuth is selected but no auth state exists.");
        co_return;
    }

    bool ok = false;
    std::string error;
    co_yield custom_types::Helpers::CoroutineHelper::New(
        auth_->EnsureAccessToken([&ok, &error](bool success, std::string err) {
            ok = success;
            error = std::move(err);
        }));

    callback(ok, ok ? auth_->CachedAccessToken() : std::string(), error);
    co_return;
}

custom_types::Helpers::Coroutine YouTubeApiClient::ResolveLiveChatId(
    std::string videoId, std::function<void(LiveChatLookupResult)> callback) {
    bool ready = false;
    std::string bearer;
    std::string prepError;
    co_yield custom_types::Helpers::CoroutineHelper::New(
        PrepareRequest([&](bool ok, std::string token, std::string error) {
            ready = ok;
            bearer = std::move(token);
            prepError = std::move(error);
        }));

    if (!ready) {
        LiveChatLookupResult result{};
        result.authError = true;
        result.errorMessage = prepError;
        callback(result);
        co_return;
    }

    std::string url = AuthorizedUrl(std::string(kVideosUrl) +
                                    "?part=snippet,liveStreamingDetails&id=" + UrlUtils::UrlEncode(videoId));

    co_yield custom_types::Helpers::CoroutineHelper::New(
        Http::Get(url, bearer, [callback](Http::Response response) {
            LiveChatLookupResult result{};

            if (!response.completed) {
                result.errorMessage = response.error.empty() ? "Network error" : response.error;
                callback(result);
                return;
            }
            if (!response.ok) {
                ApiError err = ParseApiError(response.status, response.body);
                result.authError = err.authError;
                result.quotaExceeded = err.quotaExceeded;
                result.errorMessage = err.message;
                callback(result);
                return;
            }

            rapidjson::Document doc;
            if (doc.Parse(response.body.c_str()).HasParseError() || !doc.IsObject()) {
                result.errorMessage = "Malformed response body";
                callback(result);
                return;
            }
            if (!doc.HasMember("items") || !doc["items"].IsArray() || doc["items"].Empty()) {
                // Valid response, but no such video (deleted/private/wrong ID).
                result.errorMessage = "Video not found";
                callback(result);
                return;
            }

            const auto& item = doc["items"][0];
            result.found = true;
            if (item.HasMember("snippet") && item["snippet"].IsObject()) {
                result.videoTitle = GetStr(item["snippet"], "title");
                result.channelTitle = GetStr(item["snippet"], "channelTitle");
            }
            if (item.HasMember("liveStreamingDetails") && item["liveStreamingDetails"].IsObject()) {
                const auto& lsd = item["liveStreamingDetails"];
                if (lsd.HasMember("activeLiveChatId") && lsd["activeLiveChatId"].IsString()) {
                    result.isLive = true;
                    result.liveChatId = lsd["activeLiveChatId"].GetString();
                }
                // liveStreamingDetails without activeLiveChatId means the
                // stream ended or chat is off -- isLive stays false.
            }
            callback(result);
        }));
    co_return;
}

custom_types::Helpers::Coroutine YouTubeApiClient::ResolveOwnLiveChatId(
    std::function<void(LiveChatLookupResult)> callback) {
    if (authMode_ != AuthMode::OAuth) {
        LiveChatLookupResult result{};
        result.errorMessage = "Finding your own stream requires signing in with YouTube.";
        result.authError = true;
        callback(result);
        co_return;
    }

    bool ready = false;
    std::string bearer;
    std::string prepError;
    co_yield custom_types::Helpers::CoroutineHelper::New(
        PrepareRequest([&](bool ok, std::string token, std::string error) {
            ready = ok;
            bearer = std::move(token);
            prepError = std::move(error);
        }));

    if (!ready) {
        LiveChatLookupResult result{};
        result.authError = true;
        result.errorMessage = prepError;
        callback(result);
        co_return;
    }

    std::string url = std::string(kBroadcastsUrl) +
                      "?part=snippet,status&broadcastStatus=active&broadcastType=all&maxResults=5&mine=true";

    co_yield custom_types::Helpers::CoroutineHelper::New(
        Http::Get(url, bearer, [callback](Http::Response response) {
            LiveChatLookupResult result{};

            if (!response.completed) {
                result.errorMessage = response.error.empty() ? "Network error" : response.error;
                callback(result);
                return;
            }
            if (!response.ok) {
                ApiError err = ParseApiError(response.status, response.body);
                result.authError = err.authError;
                result.quotaExceeded = err.quotaExceeded;
                result.errorMessage = err.message;
                callback(result);
                return;
            }

            rapidjson::Document doc;
            if (doc.Parse(response.body.c_str()).HasParseError() || !doc.IsObject()) {
                result.errorMessage = "Malformed response body";
                callback(result);
                return;
            }
            if (!doc.HasMember("items") || !doc["items"].IsArray() || doc["items"].Empty()) {
                // Signed in fine, just not streaming right now.
                result.found = true;
                result.isLive = false;
                callback(result);
                return;
            }

            for (const auto& item : doc["items"].GetArray()) {
                if (!item.HasMember("snippet") || !item["snippet"].IsObject()) continue;
                const auto& snip = item["snippet"];
                std::string chatId = GetStr(snip, "liveChatId");
                if (chatId.empty()) continue;
                result.found = true;
                result.isLive = true;
                result.liveChatId = chatId;
                result.videoTitle = GetStr(snip, "title");
                result.channelTitle = GetStr(snip, "channelTitle");
                break;
            }
            if (!result.isLive) {
                result.found = true;  // the account exists, it just has no active chat
            }
            callback(result);
        }));
    co_return;
}

custom_types::Helpers::Coroutine YouTubeApiClient::FetchMessages(
    std::string liveChatId, std::string pageToken,
    std::function<void(ChatPollResult)> callback) {
    bool ready = false;
    std::string bearer;
    std::string prepError;
    co_yield custom_types::Helpers::CoroutineHelper::New(
        PrepareRequest([&](bool ok, std::string token, std::string error) {
            ready = ok;
            bearer = std::move(token);
            prepError = std::move(error);
        }));

    if (!ready) {
        ChatPollResult result{};
        result.authError = true;
        result.errorMessage = prepError;
        callback(result);
        co_return;
    }

    std::string url = AuthorizedUrl(std::string(kMessagesUrl) +
                                    "?liveChatId=" + UrlUtils::UrlEncode(liveChatId) +
                                    "&part=snippet,authorDetails&maxResults=200");
    if (!pageToken.empty()) {
        url += "&pageToken=" + UrlUtils::UrlEncode(pageToken);
    }

    co_yield custom_types::Helpers::CoroutineHelper::New(
        Http::Get(url, bearer, [callback](Http::Response response) {
            ChatPollResult result{};
            result.httpStatus = response.status;

            if (!response.completed) {
                result.errorMessage = response.error.empty() ? "Network error" : response.error;
                callback(result);
                return;
            }
            if (!response.ok) {
                ApiError err = ParseApiError(response.status, response.body);
                result.authError = err.authError;
                result.quotaExceeded = err.quotaExceeded;
                result.chatEnded = err.chatEnded;
                result.errorMessage = err.message;
                callback(result);
                return;
            }

            rapidjson::Document doc;
            if (doc.Parse(response.body.c_str()).HasParseError() || !doc.IsObject()) {
                result.errorMessage = "Malformed response body";
                callback(result);
                return;
            }

            result.ok = true;
            result.nextPageToken = GetStr(doc, "nextPageToken");
            if (doc.HasMember("pollingIntervalMillis")) {
                if (doc["pollingIntervalMillis"].IsInt64()) {
                    result.pollingIntervalMillis = doc["pollingIntervalMillis"].GetInt64();
                } else if (doc["pollingIntervalMillis"].IsInt()) {
                    result.pollingIntervalMillis = doc["pollingIntervalMillis"].GetInt();
                }
            }

            if (doc.HasMember("items") && doc["items"].IsArray()) {
                for (const auto& item : doc["items"].GetArray()) {
                    if (!item.HasMember("snippet") || !item["snippet"].IsObject()) continue;
                    const auto& snip = item["snippet"];

                    ChatMessage msg{};
                    msg.id = GetStr(item, "id");
                    ParseAuthorDetails(item, msg);
                    if (!ParseMessageBody(snip, GetStr(snip, "type"), msg)) {
                        continue;  // nothing displayable for this event type
                    }
                    result.messages.push_back(std::move(msg));
                }
            }

            callback(result);
        }));
    co_return;
}

}  // namespace YouTubeLiveChat

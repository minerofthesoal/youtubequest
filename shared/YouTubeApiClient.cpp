#include "YouTubeApiClient.hpp"

#include "UnityEngine/Networking/UnityWebRequest.hpp"
#include "UnityEngine/Networking/DownloadHandler.hpp"
#include "custom-types/shared/coroutine.hpp"
#include "custom-types/shared/macros.hpp"

#include "rapidjson/document.h"

#include "paper2_scotland2/shared/logger.hpp"

#include <sstream>
#include <iomanip>
#include <cctype>

using namespace UnityEngine::Networking;

// -----------------------------------------------------------------------
// Logger tag. paper2_scotland2 is the standard Scotland2 logging shim; swap
// for whatever logging package your qpm.json ends up resolving if you use
// a different one (e.g. plain `Paper::Logger`).
// -----------------------------------------------------------------------
static const Logger& log() {
    static auto logger = Paper::ConstLoggerContext<"YTLiveChat.Api">();
    return logger;
}

namespace {

std::string UrlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << std::uppercase << int(c) << std::nouppercase;
        }
    }
    return escaped.str();
}

std::string GetStr(const rapidjson::Value& v, const char* key, const std::string& def = "") {
    if (v.HasMember(key) && v[key].IsString()) return v[key].GetString();
    return def;
}

}

namespace YouTubeLiveChat {

custom_types::Helpers::Coroutine YouTubeApiClient::SendGet(
    std::string url,
    std::function<void(bool success, long httpStatus, std::string body)> callback) {

    auto* request = UnityWebRequest::Get(url);
    request->set_timeout(15); // seconds; bail out rather than hang a poll cycle forever

    auto* op = request->SendWebRequest();
    co_yield reinterpret_cast<System::Collections::IEnumerator*>(op);

    // --- Result check ---------------------------------------------------
    // Modern Unity (2020.2+) exposes UnityWebRequest::Result via get_result().
    // If your generated codegen for this Beat Saber build predates that, swap
    // this block for: bool ok = !request->get_isNetworkError() && !request->get_isHttpError();
    bool ok = request->get_result() == UnityWebRequest::Result::Success;
    long status = static_cast<long>(request->get_responseCode());
    std::string body;
    if (request->get_downloadHandler()) {
        body = std::string(request->get_downloadHandler()->GetText());
    }

    if (!ok) {
        log().warn("HTTP request failed ({}): {}", status, request->get_error() ? std::string(request->get_error()) : "");
    }

    callback(ok, status, body);
    request->Dispose();
    co_return;
}

custom_types::Helpers::Coroutine YouTubeApiClient::ResolveLiveChatId(
    std::string videoId,
    std::function<void(LiveChatLookupResult)> callback) {

    std::string url = "https://www.googleapis.com/youtube/v3/videos"
                       "?part=snippet,liveStreamingDetails&id=" + UrlEncode(videoId) +
                       "&key=" + UrlEncode(apiKey_);

    co_yield reinterpret_cast<System::Collections::IEnumerator*>(
        custom_types::Helpers::CoroutineHelper::New(
            SendGet(url, [callback](bool success, long status, std::string body) {
                LiveChatLookupResult result{};

                if (!success) {
                    callback(result); // found=false; caller maps status separately if needed
                    return;
                }

                rapidjson::Document doc;
                if (doc.Parse(body.c_str()).HasParseError() || !doc.IsObject()) {
                    callback(result);
                    return;
                }

                if (!doc.HasMember("items") || !doc["items"].IsArray() || doc["items"].Empty()) {
                    // Valid response, but no such video (deleted/private/wrong ID).
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
                    // If liveStreamingDetails exists but activeLiveChatId doesn't,
                    // the stream has ended or chat is disabled -- isLive stays false.
                }

                callback(result);
            })
        )
    );
    co_return;
}

custom_types::Helpers::Coroutine YouTubeApiClient::FetchMessages(
    std::string liveChatId,
    std::string pageToken,
    std::function<void(ChatPollResult)> callback) {

    std::string url = "https://www.googleapis.com/youtube/v3/liveChat/messages"
                       "?liveChatId=" + UrlEncode(liveChatId) +
                       "&part=snippet,authorDetails" +
                       "&key=" + UrlEncode(apiKey_);
    if (!pageToken.empty()) {
        url += "&pageToken=" + UrlEncode(pageToken);
    }

    co_yield reinterpret_cast<System::Collections::IEnumerator*>(
        custom_types::Helpers::CoroutineHelper::New(
            SendGet(url, [callback](bool success, long status, std::string body) {
                ChatPollResult result{};
                result.httpStatus = status;

                if (!success) {
                    result.ok = false;
                    if (status == 403) {
                        // Google returns 403 both for quota exhaustion and for a
                        // disabled/misconfigured API key; disambiguate below via
                        // the error reason if present, default to quota.
                        result.quotaExceeded = true;
                    } else if (status == 401) {
                        result.authError = true;
                    }

                    rapidjson::Document errDoc;
                    if (!errDoc.Parse(body.c_str()).HasParseError() && errDoc.IsObject() &&
                        errDoc.HasMember("error") && errDoc["error"].IsObject()) {
                        const auto& err = errDoc["error"];
                        result.errorMessage = GetStr(err, "message", "Unknown API error");
                        if (err.HasMember("errors") && err["errors"].IsArray() && !err["errors"].Empty()) {
                            std::string reason = GetStr(err["errors"][0], "reason");
                            if (reason == "quotaExceeded" || reason == "dailyLimitExceeded") {
                                result.quotaExceeded = true;
                                result.authError = false;
                            } else if (reason == "keyInvalid" || reason == "forbidden" || reason == "authError") {
                                result.authError = true;
                                result.quotaExceeded = false;
                            } else if (reason == "liveChatNotFound" || reason == "liveChatEnded") {
                                // Stream ended mid-poll; caller should treat this as
                                // "go back to resolving the video" rather than a hard error.
                                result.errorMessage = "liveChatEnded";
                            }
                        }
                    }
                    callback(result);
                    return;
                }

                rapidjson::Document doc;
                if (doc.Parse(body.c_str()).HasParseError() || !doc.IsObject()) {
                    result.ok = false;
                    result.errorMessage = "Malformed response body";
                    callback(result);
                    return;
                }

                result.ok = true;
                result.nextPageToken = GetStr(doc, "nextPageToken");
                if (doc.HasMember("pollingIntervalMillis") && doc["pollingIntervalMillis"].IsInt64()) {
                    result.pollingIntervalMillis = doc["pollingIntervalMillis"].GetInt64();
                } else if (doc.HasMember("pollingIntervalMillis") && doc["pollingIntervalMillis"].IsInt()) {
                    result.pollingIntervalMillis = doc["pollingIntervalMillis"].GetInt();
                }

                if (doc.HasMember("items") && doc["items"].IsArray()) {
                    for (const auto& item : doc["items"].GetArray()) {
                        ChatMessage msg{};
                        msg.id = GetStr(item, "id");

                        if (!item.HasMember("snippet") || !item["snippet"].IsObject()) continue;
                        const auto& snip = item["snippet"];
                        std::string type = GetStr(snip, "type");

                        if (item.HasMember("authorDetails") && item["authorDetails"].IsObject()) {
                            const auto& a = item["authorDetails"];
                            msg.author.channelId = GetStr(a, "channelId");
                            msg.author.displayName = GetStr(a, "displayName");
                            msg.author.profileImageUrl = GetStr(a, "profileImageUrl");
                            if (a.HasMember("isChatOwner") && a["isChatOwner"].IsBool()) msg.author.isChatOwner = a["isChatOwner"].GetBool();
                            if (a.HasMember("isChatModerator") && a["isChatModerator"].IsBool()) msg.author.isChatModerator = a["isChatModerator"].GetBool();
                            if (a.HasMember("isChatSponsor") && a["isChatSponsor"].IsBool()) msg.author.isChatSponsor = a["isChatSponsor"].GetBool();
                            if (a.HasMember("isVerified") && a["isVerified"].IsBool()) msg.author.isVerified = a["isVerified"].GetBool();
                        }

                        if (type == "textMessageEvent") {
                            msg.type = ChatEventType::TextMessage;
                            if (snip.HasMember("textMessageDetails") && snip["textMessageDetails"].IsObject())
                                msg.displayText = GetStr(snip["textMessageDetails"], "messageText");
                        } else if (type == "superChatEvent") {
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
                        } else if (type == "superStickerEvent") {
                            msg.type = ChatEventType::SuperSticker;
                            if (snip.HasMember("superStickerDetails") && snip["superStickerDetails"].IsObject()) {
                                const auto& ss = snip["superStickerDetails"];
                                msg.amountDisplayString = GetStr(ss, "amountDisplayString");
                                msg.currency = GetStr(ss, "currency");
                                if (ss.HasMember("tier") && ss["tier"].IsInt()) msg.tier = ss["tier"].GetInt();
                                msg.displayText = "[Super Sticker]"; // sticker is an image asset; no text body via API
                            }
                        } else if (type == "newSponsorEvent") {
                            msg.type = ChatEventType::NewMember;
                            msg.displayText = msg.author.displayName + " just became a member!";
                        } else if (type == "memberMilestoneChatEvent") {
                            msg.type = ChatEventType::MemberMilestone;
                            std::string extra;
                            if (snip.HasMember("memberMilestoneChatDetails") && snip["memberMilestoneChatDetails"].IsObject())
                                extra = GetStr(snip["memberMilestoneChatDetails"], "userComment");
                            msg.displayText = extra.empty() ? "Member milestone" : extra;
                        } else if (type == "membershipGiftingEvent") {
                            msg.type = ChatEventType::MembershipGifting;
                            msg.displayText = msg.author.displayName + " gifted memberships!";
                        } else if (type == "giftMembershipReceivedEvent") {
                            msg.type = ChatEventType::GiftMembershipReceived;
                            msg.displayText = msg.author.displayName + " received a gifted membership!";
                        } else if (type == "messageDeletedEvent") {
                            msg.type = ChatEventType::MessageDeleted;
                            msg.displayText = "[message deleted by moderator]";
                        } else if (type == "messageRetractedEvent") {
                            msg.type = ChatEventType::MessageRetracted;
                            msg.displayText = "[message retracted]";
                        } else if (type == "userBannedEvent") {
                            msg.type = ChatEventType::UserBanned;
                            msg.displayText = "[user banned]";
                        } else if (type == "tombstone") {
                            msg.type = ChatEventType::Tombstone;
                            continue; // nothing displayable
                        } else {
                            msg.type = ChatEventType::Unsupported;
                            continue; // pollEvent*, sponsorOnlyMode*, chatEndedEvent, fanFundingEvent (legacy) -- skip
                        }

                        if (snip.HasMember("publishedAt") && snip["publishedAt"].IsString()) {
                            // ISO-8601 -- parsed elsewhere if you need real ordering beyond
                            // API arrival order; arrival order is already chronological.
                        }

                        result.messages.push_back(std::move(msg));
                    }
                }

                callback(result);
            })
        )
    );
    co_return;
}

}

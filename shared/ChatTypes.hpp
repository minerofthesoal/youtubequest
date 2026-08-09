#pragma once
#include <string>
#include <cstdint>

namespace YouTubeLiveChat {

// Mirrors the real snippet.type values documented for the liveChatMessage
// resource. Only a subset is rendered in the overlay; the rest are parsed
// so we don't silently choke on unknown events, but are dropped after
// logging (see ChatManager::HandleMessage).
enum class ChatEventType {
    TextMessage,          // textMessageEvent
    SuperChat,             // superChatEvent
    SuperSticker,           // superStickerEvent
    NewMember,             // newSponsorEvent  (channel membership, NOT a plain subscribe)
    MemberMilestone,        // memberMilestoneChatEvent
    MembershipGifting,      // membershipGiftingEvent
    GiftMembershipReceived, // giftMembershipReceivedEvent
    MessageDeleted,          // messageDeletedEvent
    MessageRetracted,        // messageRetractedEvent
    UserBanned,               // userBannedEvent
    Tombstone,                 // tombstone
    Unsupported                 // anything else (polls, chatEndedEvent, sponsor-only mode, ...)
};

struct ChatAuthor {
    std::string channelId;
    std::string displayName;
    std::string profileImageUrl;
    bool isChatOwner = false;
    bool isChatModerator = false;
    bool isChatSponsor = false;   // "member" badge
    bool isVerified = false;
};

struct ChatMessage {
    std::string id;
    ChatEventType type = ChatEventType::Unsupported;
    ChatAuthor author;
    std::string displayText;      // pre-rendered display text (message body, or a
                                    // synthesized line like "was a member for 6 months")
    // Super Chat / Super Sticker only:
    std::string amountDisplayString; // e.g. "$5.00", already formatted by the API
    int64_t amountMicros = 0;
    std::string currency;
    int32_t tier = 0;              // 1 (lowest) .. N, from superChatDetails.tier

    int64_t publishedAtUnixMillis = 0;
    uint64_t receivedAtFrameTimeMillis = 0; // local monotonic time, for expiry only
};

enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    StreamOffline,
    AuthOrApiError,
    RateLimited,
    InvalidConfiguration,
    NetworkError
};

inline const char* ToString(ConnectionState s) {
    switch (s) {
        case ConnectionState::Disconnected:        return "Disconnected";
        case ConnectionState::Connecting:            return "Connecting...";
        case ConnectionState::Connected:              return "Connected";
        case ConnectionState::StreamOffline:           return "Stream offline";
        case ConnectionState::AuthOrApiError:           return "API / auth error";
        case ConnectionState::RateLimited:               return "Rate limited";
        case ConnectionState::InvalidConfiguration:       return "Invalid configuration";
        case ConnectionState::NetworkError:                return "Network error";
    }
    return "Unknown";
}

}

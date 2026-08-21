#pragma once
#include <string>
#include <cstdint>

namespace YouTubeLiveChat {

enum class PanelPreset {
    Custom,
    TopLeft,
    TopCenter,
    TopRight,
    MiddleLeft,
    MiddleRight,
    BottomLeft,
    BottomRight
};

const char* PanelPresetToString(PanelPreset p);
PanelPreset PanelPresetFromString(const std::string& s);

// How we authenticate against the YouTube Data API.
//   ApiKey -- a plain browser/server API key. Read-only access to *public*
//             live chat, no user consent step. Cannot see your own
//             members-only chat and cannot auto-detect your broadcast.
//   OAuth  -- OAuth 2.0 device flow ("TV and Limited Input device" client).
//             Lets the mod read chat as *you*, including your own
//             members-only stream, and auto-find your active broadcast
//             without pasting a video ID.
enum class AuthMode {
    ApiKey,
    OAuth
};

const char* AuthModeToString(AuthMode m);
AuthMode AuthModeFromString(const std::string& s);

struct ModConfig {
    // --- Core ---
    bool enabled = true;
    std::string videoIdOrUrl;
    bool autoReconnect = true;
    bool debugLogging = false;

    // --- Auth ---
    AuthMode authMode = AuthMode::ApiKey;
    std::string apiKey;             // YouTube Data API v3 key (AuthMode::ApiKey)
    std::string oauthClientId;      // OAuth client id     (AuthMode::OAuth)
    std::string oauthClientSecret;  // OAuth client secret (AuthMode::OAuth)
    std::string oauthRefreshToken;  // filled in by the device flow, persisted
    std::string oauthAccountName;   // display-only, so the UI can show who is signed in
    // With OAuth, prefer liveBroadcasts.list(mine=true) over the video ID so
    // you never have to type anything to watch your own stream's chat.
    bool useOwnBroadcast = true;

    // --- Panel placement ---
    PanelPreset preset = PanelPreset::MiddleRight;
    // true  -> the panel is placed relative to your head using the preset
    //          above and softly follows you.
    // false -> the panel stays where you last dragged it by its handle
    //          (panelPos*/panelRot* below).
    bool followHead = true;
    float customX = 0.6f;
    float customY = 1.4f;
    float customZ = 2.0f;
    float horizontalOffset = 0.0f;
    float verticalOffset = 0.0f;
    // Metres out from your head along the preset's direction. Kept modest by
    // default: a chat panel parked a couple of metres straight ahead sits in
    // the middle of the play space and reads as being in the way.
    float distance = 1.6f;
    float width = 0.9f;
    float height = 1.1f;
    float scale = 1.0f;
    float opacity = 0.85f;

    // Free placement (only used when followHead == false).
    float panelPosX = 0.0f;
    float panelPosY = 1.6f;
    float panelPosZ = 2.2f;
    float panelRotX = 0.0f;
    float panelRotY = 0.0f;
    float panelRotZ = 0.0f;

    // --- Message behavior ---
    int32_t maxVisibleMessages = 12;
    float messageDurationSeconds = 45.0f; // 0 = never auto-expire, only trims by maxVisibleMessages

    bool showRegularMessages = true;
    bool showSuperChats = true;
    bool showSuperStickers = true;
    bool showMembershipEvents = true;   // newSponsorEvent, memberMilestoneChatEvent, gifting
    bool showUsernames = true;
    bool showProfilePictures = true;
    bool highlightSuperChats = true;
    bool highlightMemberships = true;
    bool notificationSounds = false;

    // --- Networking ---
    // Floor on polling interval regardless of what the API's pollingIntervalMillis
    // suggests, so a misbehaving/aggressive suggestion can never hammer quota.
    float minPollIntervalSeconds = 2.0f;

    static ModConfig Load();
    void Save() const;
};

}  // namespace YouTubeLiveChat

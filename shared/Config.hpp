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

struct ModConfig {
    // --- Core ---
    bool enabled = true;
    std::string apiKey;                 // YouTube Data API v3 key, user-supplied, stored locally only
    std::string videoIdOrUrl;
    bool autoReconnect = true;
    bool debugLogging = false;

    // --- Panel placement ---
    PanelPreset preset = PanelPreset::TopRight;
    float customX = 0.6f;
    float customY = 1.4f;
    float customZ = 2.0f;
    float horizontalOffset = 0.0f;
    float verticalOffset = 0.0f;
    float distance = 2.0f;
    float width = 0.9f;
    float height = 1.1f;
    float scale = 1.0f;
    float opacity = 0.85f;

    // --- Message behavior ---
    int32_t maxVisibleMessages = 20;
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

}

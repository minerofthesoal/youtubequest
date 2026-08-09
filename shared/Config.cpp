#include "Config.hpp"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/error/en.h"

#include <fstream>
#include <sstream>
#include <sys/stat.h>

// NOTE: this mirrors the folder convention most Quest BS mods use for their
// own JSON config (as opposed to game save data). If your build environment
// already has `config-utils` set up and you'd rather use its observable
// Config<T> wrapper (auto-persists on field write, integrates with BSML's
// two-way binding more smoothly), swap this file for that -- the ModConfig
// struct above is deliberately plain-old-data so either approach works.
namespace {
    constexpr const char* kConfigDir = "/sdcard/ModData/com.beatgames.beatsaber/Configs";
    constexpr const char* kConfigPath = "/sdcard/ModData/com.beatgames.beatsaber/Configs/YouTubeLiveChat.json";

    void EnsureDir() {
        mkdir("/sdcard/ModData", 0770);
        mkdir("/sdcard/ModData/com.beatgames.beatsaber", 0770);
        mkdir(kConfigDir, 0770);
    }
}

namespace YouTubeLiveChat {

const char* PanelPresetToString(PanelPreset p) {
    switch (p) {
        case PanelPreset::Custom:       return "Custom";
        case PanelPreset::TopLeft:      return "TopLeft";
        case PanelPreset::TopCenter:    return "TopCenter";
        case PanelPreset::TopRight:     return "TopRight";
        case PanelPreset::MiddleLeft:   return "MiddleLeft";
        case PanelPreset::MiddleRight:  return "MiddleRight";
        case PanelPreset::BottomLeft:   return "BottomLeft";
        case PanelPreset::BottomRight:  return "BottomRight";
    }
    return "Custom";
}

PanelPreset PanelPresetFromString(const std::string& s) {
    if (s == "TopLeft") return PanelPreset::TopLeft;
    if (s == "TopCenter") return PanelPreset::TopCenter;
    if (s == "TopRight") return PanelPreset::TopRight;
    if (s == "MiddleLeft") return PanelPreset::MiddleLeft;
    if (s == "MiddleRight") return PanelPreset::MiddleRight;
    if (s == "BottomLeft") return PanelPreset::BottomLeft;
    if (s == "BottomRight") return PanelPreset::BottomRight;
    return PanelPreset::Custom;
}

static std::string ReadFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

#define GET_BOOL(field) if (doc.HasMember(#field) && doc[#field].IsBool()) cfg.field = doc[#field].GetBool();
#define GET_FLOAT(field) if (doc.HasMember(#field) && doc[#field].IsNumber()) cfg.field = doc[#field].GetFloat();
#define GET_INT(field) if (doc.HasMember(#field) && doc[#field].IsInt()) cfg.field = doc[#field].GetInt();
#define GET_STR(field) if (doc.HasMember(#field) && doc[#field].IsString()) cfg.field = doc[#field].GetString();

ModConfig ModConfig::Load() {
    ModConfig cfg{};
    std::string text = ReadFile(kConfigPath);
    if (text.empty()) {
        return cfg; // first run -- defaults, then Save() will create the file
    }

    rapidjson::Document doc;
    if (doc.Parse(text.c_str()).HasParseError() || !doc.IsObject()) {
        // Corrupt config: fall back to defaults rather than crashing the game.
        return cfg;
    }

    GET_BOOL(enabled)
    GET_STR(apiKey)
    GET_STR(videoIdOrUrl)
    GET_BOOL(autoReconnect)
    GET_BOOL(debugLogging)

    if (doc.HasMember("preset") && doc["preset"].IsString()) {
        cfg.preset = PanelPresetFromString(doc["preset"].GetString());
    }
    GET_FLOAT(customX)
    GET_FLOAT(customY)
    GET_FLOAT(customZ)
    GET_FLOAT(horizontalOffset)
    GET_FLOAT(verticalOffset)
    GET_FLOAT(distance)
    GET_FLOAT(width)
    GET_FLOAT(height)
    GET_FLOAT(scale)
    GET_FLOAT(opacity)

    GET_INT(maxVisibleMessages)
    GET_FLOAT(messageDurationSeconds)
    GET_BOOL(showRegularMessages)
    GET_BOOL(showSuperChats)
    GET_BOOL(showSuperStickers)
    GET_BOOL(showMembershipEvents)
    GET_BOOL(showUsernames)
    GET_BOOL(showProfilePictures)
    GET_BOOL(highlightSuperChats)
    GET_BOOL(highlightMemberships)
    GET_BOOL(notificationSounds)
    GET_FLOAT(minPollIntervalSeconds)

    return cfg;
}

#undef GET_BOOL
#undef GET_FLOAT
#undef GET_INT
#undef GET_STR

void ModConfig::Save() const {
    EnsureDir();

    rapidjson::Document doc;
    doc.SetObject();
    auto& alloc = doc.GetAllocator();

    doc.AddMember("enabled", enabled, alloc);
    doc.AddMember("apiKey", rapidjson::Value(apiKey.c_str(), alloc), alloc);
    doc.AddMember("videoIdOrUrl", rapidjson::Value(videoIdOrUrl.c_str(), alloc), alloc);
    doc.AddMember("autoReconnect", autoReconnect, alloc);
    doc.AddMember("debugLogging", debugLogging, alloc);

    doc.AddMember("preset", rapidjson::Value(PanelPresetToString(preset), alloc), alloc);
    doc.AddMember("customX", customX, alloc);
    doc.AddMember("customY", customY, alloc);
    doc.AddMember("customZ", customZ, alloc);
    doc.AddMember("horizontalOffset", horizontalOffset, alloc);
    doc.AddMember("verticalOffset", verticalOffset, alloc);
    doc.AddMember("distance", distance, alloc);
    doc.AddMember("width", width, alloc);
    doc.AddMember("height", height, alloc);
    doc.AddMember("scale", scale, alloc);
    doc.AddMember("opacity", opacity, alloc);

    doc.AddMember("maxVisibleMessages", maxVisibleMessages, alloc);
    doc.AddMember("messageDurationSeconds", messageDurationSeconds, alloc);
    doc.AddMember("showRegularMessages", showRegularMessages, alloc);
    doc.AddMember("showSuperChats", showSuperChats, alloc);
    doc.AddMember("showSuperStickers", showSuperStickers, alloc);
    doc.AddMember("showMembershipEvents", showMembershipEvents, alloc);
    doc.AddMember("showUsernames", showUsernames, alloc);
    doc.AddMember("showProfilePictures", showProfilePictures, alloc);
    doc.AddMember("highlightSuperChats", highlightSuperChats, alloc);
    doc.AddMember("highlightMemberships", highlightMemberships, alloc);
    doc.AddMember("notificationSounds", notificationSounds, alloc);
    doc.AddMember("minPollIntervalSeconds", minPollIntervalSeconds, alloc);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    std::ofstream f(kConfigPath, std::ios::binary | std::ios::trunc);
    if (f) {
        f << buffer.GetString();
    }
}

}

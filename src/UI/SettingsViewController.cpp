#include "UI/SettingsViewController.hpp"
#include "ModState.hpp"
#include "UI/ChatOverlayController.hpp"
#include "Logging.hpp"

#include "bsml/shared/BSML-Lite.hpp"

#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/RectTransform.hpp"
#include "UnityEngine/Vector2.hpp"
#include "TMPro/TextAlignmentOptions.hpp"

#include <array>
#include <span>
#include <string_view>

using namespace UnityEngine;
using namespace YouTubeLiveChat;
using namespace YouTubeLiveChat::UI;

DEFINE_TYPE(YouTubeLiveChat::UI, SettingsViewController);

namespace {

// Not constexpr: BSML's CreateDropdown takes a mutable std::span, so these
// need to live in non-const storage.
std::array<std::string_view, 2> kAuthModeNames = {
    "API key (public chat)",
    "Sign in with YouTube",
};

std::array<std::string_view, 8> kPresetNames = {
    "Custom", "Top left", "Top center", "Top right",
    "Middle left", "Middle right", "Bottom left", "Bottom right",
};

PanelPreset IndexToPreset(size_t i) {
    switch (i) {
        case 1: return PanelPreset::TopLeft;
        case 2: return PanelPreset::TopCenter;
        case 3: return PanelPreset::TopRight;
        case 4: return PanelPreset::MiddleLeft;
        case 5: return PanelPreset::MiddleRight;
        case 6: return PanelPreset::BottomLeft;
        case 7: return PanelPreset::BottomRight;
        default: return PanelPreset::Custom;
    }
}

std::string_view PresetToName(PanelPreset p) {
    switch (p) {
        case PanelPreset::TopLeft: return kPresetNames[1];
        case PanelPreset::TopCenter: return kPresetNames[2];
        case PanelPreset::TopRight: return kPresetNames[3];
        case PanelPreset::MiddleLeft: return kPresetNames[4];
        case PanelPreset::MiddleRight: return kPresetNames[5];
        case PanelPreset::BottomLeft: return kPresetNames[6];
        case PanelPreset::BottomRight: return kPresetNames[7];
        default: return kPresetNames[0];
    }
}

std::string Mask(std::string const& secret) {
    if (secret.empty()) return "(not set)";
    if (secret.size() <= 6) return "******";
    return secret.substr(0, 4) + std::string(6, '*') + secret.substr(secret.size() - 2);
}

std::string ToStd(StringW s) {
    if (!s) return {};
    return static_cast<std::string>(s);
}

}  // namespace

void SettingsViewController::Edit(std::function<void(ModConfig&)> const& mutate) {
    ModConfig cfg = ModState::Config();
    mutate(cfg);
    ModState::ApplyConfig(cfg);
}

void SettingsViewController::DidActivate(bool firstActivation, bool addedToHierarchy,
                                         bool screenSystemEnabling) {
    ModState::SettingsPtr() = this;
    Log().info("Settings DidActivate (firstActivation={})", firstActivation);
    if (firstActivation) {
        // A managed exception from any one control would otherwise abandon the
        // rest of the screen and leave it half-built with no explanation --
        // which is indistinguishable, from the headset, from "the mod is
        // broken". Log it instead so the cause is in the Scotland2 log.
        try {
            BuildUI();
        } catch (std::exception const& e) {
            Log().error("Settings UI failed to build: {}", e.what());
        }
    }
    RefreshFromConfig();
    // Qualified: this controller inherits UnityEngine::Object, whose own
    // ToString member would otherwise hide the ConnectionState overload.
    auto& manager = ModState::Manager();
    std::string line = YouTubeLiveChat::ToString(manager.State());
    if (!manager.StatusDetail().empty()) line += " - " + manager.StatusDetail();
    SetStatus(line);
}

void SettingsViewController::DidDeactivate(bool removedFromHierarchy, bool screenSystemDisabling) {
    if (ModState::SettingsPtr() == this) {
        ModState::SettingsPtr() = nullptr;
    }
}

void SettingsViewController::BuildUI() {
    // One scrollable container parented straight to the view controller.
    //
    // The previous layout nested three of these inside a HorizontalLayoutGroup
    // to get columns, and the settings screen came out completely blank: the
    // horizontal group carries a ContentSizeFitter set to PreferredSize while
    // also being stretched to its parent, and the scroll views inside report
    // no preferred height, so the whole row collapsed to zero height and every
    // control was laid out inside nothing. CreateScrollableSettingsContainer
    // anchors itself to fill its parent (see BSML's Layout.cpp), so given the
    // view controller's own full-size RectTransform it just works -- one
    // column that scrolls, which is what every other Quest mod does.
    UnityEngine::GameObject* container = BSML::Lite::CreateScrollableSettingsContainer(get_transform());
    if (!container) {
        Log().error("Settings: CreateScrollableSettingsContainer returned null");
        return;
    }

    UnityEngine::Transform* parent = container->get_transform();
    BuildConnectionSection(parent);
    BuildPlacementSection(parent);
    BuildMessageSection(parent);
    Log().info("Settings UI built");
}

void SettingsViewController::BuildConnectionSection(Transform* parent) {
    BSML::Lite::CreateText(parent, StringW("<b>Connection</b>"), 4.5f);

    statusText_ = BSML::Lite::CreateText(parent, StringW("Disconnected"), 3.0f);
    statusText_->set_enableWordWrapping(true);

    ModConfig const& cfg = ModState::Config();

    BSML::Lite::CreateDropdown(
        parent, StringW("Sign-in method"),
        StringW(std::string(kAuthModeNames[cfg.authMode == AuthMode::OAuth ? 1 : 0])),
        std::span<std::string_view>(kAuthModeNames),
        [this](StringW value) {
            std::string picked = ToStd(value);
            Edit([&picked](ModConfig& c) {
                c.authMode = (picked == std::string(kAuthModeNames[1])) ? AuthMode::OAuth : AuthMode::ApiKey;
            });
            RefreshAuthStatus();
        });

    BSML::Lite::CreateStringSetting(
        parent, StringW("API key"), StringW(cfg.apiKey), [this](StringW value) {
            std::string v = ToStd(value);
            Edit([&v](ModConfig& c) { c.apiKey = v; });
            RefreshAuthStatus();
        });

    BSML::Lite::CreateStringSetting(
        parent, StringW("OAuth client ID"), StringW(cfg.oauthClientId), [this](StringW value) {
            std::string v = ToStd(value);
            Edit([&v](ModConfig& c) { c.oauthClientId = v; });
            RefreshAuthStatus();
        });

    BSML::Lite::CreateStringSetting(
        parent, StringW("OAuth client secret"), StringW(cfg.oauthClientSecret), [this](StringW value) {
            std::string v = ToStd(value);
            Edit([&v](ModConfig& c) { c.oauthClientSecret = v; });
            RefreshAuthStatus();
        });

    authStatusText_ = BSML::Lite::CreateText(parent, StringW(""), 3.0f);
    authStatusText_->set_enableWordWrapping(true);

    signInButton_ = BSML::Lite::CreateUIButton(parent, StringW("Sign in with YouTube"), [this]() {
        ModState::Manager().BeginSignIn();
    });

    BSML::Lite::CreateUIButton(parent, StringW("Sign out"), [this]() {
        ModState::Manager().SignOut();
        Edit([](ModConfig& c) {
            c.oauthRefreshToken.clear();
            c.oauthAccountName.clear();
        });
        RefreshAuthStatus();
    });

    BSML::Lite::CreateToggle(
        parent, StringW("Use my own live stream"), cfg.useOwnBroadcast, [this](bool value) {
            Edit([value](ModConfig& c) { c.useOwnBroadcast = value; });
        });

    BSML::Lite::CreateStringSetting(
        parent, StringW("Video ID or URL"), StringW(cfg.videoIdOrUrl), [this](StringW value) {
            std::string v = ToStd(value);
            Edit([&v](ModConfig& c) { c.videoIdOrUrl = v; });
        });

    BSML::Lite::CreateUIButton(parent, StringW("Connect"), [this]() {
        ModState::Manager().Connect();
    });
    BSML::Lite::CreateUIButton(parent, StringW("Disconnect"), [this]() {
        ModState::Manager().Disconnect();
    });

    BSML::Lite::CreateToggle(parent, StringW("Auto-reconnect"), cfg.autoReconnect, [this](bool value) {
        Edit([value](ModConfig& c) { c.autoReconnect = value; });
    });
    BSML::Lite::CreateToggle(parent, StringW("Debug logging"), cfg.debugLogging, [this](bool value) {
        Edit([value](ModConfig& c) { c.debugLogging = value; });
    });
}

void SettingsViewController::BuildPlacementSection(Transform* parent) {
    BSML::Lite::CreateText(parent, StringW("\n<b>Panel position</b>"), 4.5f);

    ModConfig const& cfg = ModState::Config();

    BSML::Lite::CreateToggle(parent, StringW("Show overlay"), cfg.enabled, [this](bool value) {
        Edit([value](ModConfig& c) { c.enabled = value; });
    });

    BSML::Lite::CreateToggle(
        parent, StringW("Follow my head"), cfg.followHead, [this](bool value) {
            Edit([value](ModConfig& c) { c.followHead = value; });
        });

    BSML::Lite::CreateToggle(
        parent, StringW("Position with red saber"),
        ModState::OverlayPtr() && ModState::OverlayPtr()->SaberPlacementActive(),
        [this](bool value) {
            if (!ModState::OverlayPtr()) return;
            ModState::OverlayPtr()->SetSaberPlacement(value);
        });

    BSML::Lite::CreateText(
        parent,
        StringW("<size=80%><color=#909090>Turn on, point where you want the panel, turn off to "
                "drop it there. In a level it follows the red saber; here in the menu it "
                "follows your left controller. Pause mid-song for the same button.</color></size>"),
        3.0f)
        ->set_enableWordWrapping(true);

    BSML::Lite::CreateDropdown(
        parent, StringW("Position preset"), StringW(std::string(PresetToName(cfg.preset))),
        std::span<std::string_view>(kPresetNames),
        [this](StringW value) {
            std::string picked = ToStd(value);
            Edit([&picked](ModConfig& c) {
                for (size_t i = 0; i < kPresetNames.size(); i++) {
                    if (picked == std::string(kPresetNames[i])) {
                        c.preset = IndexToPreset(i);
                        return;
                    }
                }
            });
        });

    BSML::Lite::CreateSliderSetting(parent, StringW("Distance"), 0.1f, cfg.distance, 0.5f, 4.0f,
                                    [this](float value) {
                                        Edit([value](ModConfig& c) { c.distance = value; });
                                    });
    BSML::Lite::CreateSliderSetting(parent, StringW("Horizontal offset"), 0.05f, cfg.horizontalOffset,
                                    -2.0f, 2.0f, [this](float value) {
                                        Edit([value](ModConfig& c) { c.horizontalOffset = value; });
                                    });
    BSML::Lite::CreateSliderSetting(parent, StringW("Vertical offset"), 0.05f, cfg.verticalOffset,
                                    -2.0f, 2.0f, [this](float value) {
                                        Edit([value](ModConfig& c) { c.verticalOffset = value; });
                                    });
    BSML::Lite::CreateSliderSetting(parent, StringW("Width"), 0.05f, cfg.width, 0.3f, 2.5f,
                                    [this](float value) {
                                        Edit([value](ModConfig& c) { c.width = value; });
                                    });
    BSML::Lite::CreateSliderSetting(parent, StringW("Height"), 0.05f, cfg.height, 0.3f, 2.5f,
                                    [this](float value) {
                                        Edit([value](ModConfig& c) { c.height = value; });
                                    });
    BSML::Lite::CreateSliderSetting(parent, StringW("Scale"), 0.05f, cfg.scale, 0.3f, 2.0f,
                                    [this](float value) {
                                        Edit([value](ModConfig& c) { c.scale = value; });
                                    });
    BSML::Lite::CreateSliderSetting(parent, StringW("Opacity"), 0.05f, cfg.opacity, 0.1f, 1.0f,
                                    [this](float value) {
                                        Edit([value](ModConfig& c) { c.opacity = value; });
                                    });
}

void SettingsViewController::BuildMessageSection(Transform* parent) {
    BSML::Lite::CreateText(parent, StringW("\n<b>Messages</b>"), 4.5f);

    ModConfig const& cfg = ModState::Config();

    BSML::Lite::CreateSliderSetting(parent, StringW("Max visible"), 1.0f,
                                    static_cast<float>(cfg.maxVisibleMessages), 3.0f, 30.0f,
                                    [this](float value) {
                                        Edit([value](ModConfig& c) {
                                            c.maxVisibleMessages = static_cast<int32_t>(value);
                                        });
                                    });
    BSML::Lite::CreateSliderSetting(parent, StringW("Hide after (s, 0 = never)"), 5.0f,
                                    cfg.messageDurationSeconds, 0.0f, 180.0f, [this](float value) {
                                        Edit([value](ModConfig& c) { c.messageDurationSeconds = value; });
                                    });

    BSML::Lite::CreateToggle(parent, StringW("Regular messages"), cfg.showRegularMessages,
                             [this](bool v) { Edit([v](ModConfig& c) { c.showRegularMessages = v; }); });
    BSML::Lite::CreateToggle(parent, StringW("Super Chats"), cfg.showSuperChats,
                             [this](bool v) { Edit([v](ModConfig& c) { c.showSuperChats = v; }); });
    BSML::Lite::CreateToggle(parent, StringW("Super Stickers"), cfg.showSuperStickers,
                             [this](bool v) { Edit([v](ModConfig& c) { c.showSuperStickers = v; }); });
    BSML::Lite::CreateToggle(parent, StringW("Memberships"), cfg.showMembershipEvents,
                             [this](bool v) { Edit([v](ModConfig& c) { c.showMembershipEvents = v; }); });
    BSML::Lite::CreateToggle(parent, StringW("Usernames"), cfg.showUsernames,
                             [this](bool v) { Edit([v](ModConfig& c) { c.showUsernames = v; }); });
    BSML::Lite::CreateToggle(parent, StringW("Profile pictures"), cfg.showProfilePictures,
                             [this](bool v) { Edit([v](ModConfig& c) { c.showProfilePictures = v; }); });
    BSML::Lite::CreateToggle(parent, StringW("Highlight Super Chats"), cfg.highlightSuperChats,
                             [this](bool v) { Edit([v](ModConfig& c) { c.highlightSuperChats = v; }); });
    BSML::Lite::CreateToggle(parent, StringW("Highlight memberships"), cfg.highlightMemberships,
                             [this](bool v) { Edit([v](ModConfig& c) { c.highlightMemberships = v; }); });
    BSML::Lite::CreateToggle(parent, StringW("Notification sound"), cfg.notificationSounds,
                             [this](bool v) { Edit([v](ModConfig& c) { c.notificationSounds = v; }); });
}

void SettingsViewController::SetStatus(std::string const& text) {
    statusLine_ = text;
    if (statusText_) {
        statusText_->set_text(StringW("<color=#B0B0B0>" + text + "</color>"));
    }
}

void SettingsViewController::SetSignInPrompt(std::string const& userCode,
                                             std::string const& verificationUrl) {
    if (userCode.empty()) {
        promptLine_.clear();
        RefreshAuthStatus();
        return;
    }
    promptLine_ = "On your phone or PC open <b>" + verificationUrl +
                  "</b> and enter <color=#FFD24D><b>" + userCode + "</b></color>";
    if (authStatusText_) {
        authStatusText_->set_text(StringW(promptLine_));
    }
}

void SettingsViewController::RefreshAuthStatus() {
    if (!authStatusText_) return;
    if (!promptLine_.empty()) {
        authStatusText_->set_text(StringW(promptLine_));
        return;
    }

    ModConfig const& cfg = ModState::Config();
    std::string line;
    if (cfg.authMode == AuthMode::OAuth) {
        if (cfg.oauthClientId.empty() || cfg.oauthClientSecret.empty()) {
            line = "<color=#D9A066>Add an OAuth client ID and secret (Google Cloud console -> "
                   "Credentials -> TVs and Limited Input devices).</color>";
        } else if (cfg.oauthRefreshToken.empty()) {
            line = "<color=#D9A066>Not signed in. Press \"Sign in with YouTube\".</color>";
        } else {
            line = "<color=#5BE07E>Signed in.</color> Client " + Mask(cfg.oauthClientId);
        }
    } else {
        line = cfg.apiKey.empty()
                   ? "<color=#D9A066>No API key set.</color>"
                   : ("Using API key " + Mask(cfg.apiKey) + " (public chat only).");
    }
    authStatusText_->set_text(StringW(line));
}

void SettingsViewController::RefreshFromConfig() {
    RefreshAuthStatus();
}

#include "SettingsViewController.hpp"
#include "BSMLLayouts.hpp"
#include "ModState.hpp"
#include "ChatOverlayController.hpp"

#include "System/Collections/Generic/List_1.hpp"
#include "paper2_scotland2/shared/logger.hpp"

using namespace YouTubeLiveChat;
using namespace YouTubeLiveChat::UI;

static auto& log() {
    static auto logger = Paper::ConstLoggerContext("YTLiveChat.Settings");
    return logger;
}

DEFINE_TYPE(YouTubeLiveChat::UI, SettingsViewController);

namespace {
    const char* kPresetNames[] = {
        "Custom", "Top Left", "Top Center", "Top Right",
        "Middle Left", "Middle Right", "Bottom Left", "Bottom Right"
    };
    constexpr int kPresetCount = 8;

    PanelPreset IndexToPreset(int i) {
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

    int PresetToIndex(PanelPreset p) {
        switch (p) {
            case PanelPreset::TopLeft: return 1;
            case PanelPreset::TopCenter: return 2;
            case PanelPreset::TopRight: return 3;
            case PanelPreset::MiddleLeft: return 4;
            case PanelPreset::MiddleRight: return 5;
            case PanelPreset::BottomLeft: return 6;
            case PanelPreset::BottomRight: return 7;
            default: return 0;
        }
    }
}

void SettingsViewController::ctor() {
    INVOKE_CTOR();
    ApiKey = StringW("");
    VideoIdOrUrl = StringW("");
    StatusText = StringW("Disconnected");
    PresetChoices = System::Collections::Generic::List_1<StringW>::New_ctor();
    for (int i = 0; i < kPresetCount; i++) PresetChoices->Add(StringW(kPresetNames[i]));
}

void SettingsViewController::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if (firstActivation) {
        // VERIFY: exact parse/construct call + signature for your restored
        // `bsml` package -- shown here is the shape used by most current
        // Quest-BSML mods (parse the XML against this transform, binding to
        // `this` as the host for value=/on-click= lookups).
        BSML::parse_and_construct(UI::kSettingsMenuXml, get_transform(), this);
    }

    RefreshFromConfig(ModState::Config());
}

void SettingsViewController::RefreshFromConfig(const ModConfig& cfg) {
    ApiKey = StringW(cfg.apiKey);
    VideoIdOrUrl = StringW(cfg.videoIdOrUrl);
    Enabled = cfg.enabled;
    AutoReconnect = cfg.autoReconnect;
    DebugLogging = cfg.debugLogging;

    PresetIndex = PresetToIndex(cfg.preset);
    HorizontalOffset = cfg.horizontalOffset;
    VerticalOffset = cfg.verticalOffset;
    Distance = cfg.distance;
    Width = cfg.width;
    Height = cfg.height;
    Scale = cfg.scale;
    Opacity = cfg.opacity;

    MaxVisibleMessages = cfg.maxVisibleMessages;
    MessageDurationSeconds = cfg.messageDurationSeconds;
    ShowRegularMessages = cfg.showRegularMessages;
    ShowSuperChats = cfg.showSuperChats;
    ShowSuperStickers = cfg.showSuperStickers;
    ShowMembershipEvents = cfg.showMembershipEvents;
    ShowUsernames = cfg.showUsernames;
    ShowProfilePictures = cfg.showProfilePictures;
    HighlightSuperChats = cfg.highlightSuperChats;
    HighlightMemberships = cfg.highlightMemberships;
    NotificationSounds = cfg.notificationSounds;
}

ModConfig SettingsViewController::BuildConfigFromFields() {
    ModConfig cfg = ModState::Config(); // start from existing, so unmapped fields (e.g. minPollIntervalSeconds) survive
    cfg.apiKey = std::string(ApiKey);
    cfg.videoIdOrUrl = std::string(VideoIdOrUrl);
    cfg.enabled = Enabled;
    cfg.autoReconnect = AutoReconnect;
    cfg.debugLogging = DebugLogging;

    cfg.preset = IndexToPreset(PresetIndex);
    cfg.horizontalOffset = HorizontalOffset;
    cfg.verticalOffset = VerticalOffset;
    cfg.distance = Distance;
    cfg.width = Width;
    cfg.height = Height;
    cfg.scale = Scale;
    cfg.opacity = Opacity;

    cfg.maxVisibleMessages = MaxVisibleMessages;
    cfg.messageDurationSeconds = MessageDurationSeconds;
    cfg.showRegularMessages = ShowRegularMessages;
    cfg.showSuperChats = ShowSuperChats;
    cfg.showSuperStickers = ShowSuperStickers;
    cfg.showMembershipEvents = ShowMembershipEvents;
    cfg.showUsernames = ShowUsernames;
    cfg.showProfilePictures = ShowProfilePictures;
    cfg.highlightSuperChats = HighlightSuperChats;
    cfg.highlightMemberships = HighlightMemberships;
    cfg.notificationSounds = NotificationSounds;
    return cfg;
}

void SettingsViewController::SaveAndApply() {
    ModConfig cfg = BuildConfigFromFields();
    cfg.Save();
    ModState::Config() = cfg;
    ModState::Manager().Configure(cfg);
    if (ModState::OverlayPtr()) {
        ModState::OverlayPtr()->ApplyConfig(cfg);
    }
}

void SettingsViewController::SetStatus(const std::string& text) {
    StatusText = StringW(text);
}

void SettingsViewController::ConnectClicked() {
    SaveAndApply();
    ModState::Manager().Connect();
}

void SettingsViewController::DisconnectClicked() {
    ModState::Manager().Disconnect();
    SetStatus("Disconnected");
}

void SettingsViewController::OnApiKeyChanged() { SaveAndApply(); }
void SettingsViewController::OnVideoChanged() { SaveAndApply(); }

void SettingsViewController::OnEnabledChanged() {
    SaveAndApply();
    if (ModState::OverlayPtr()) {
        ModState::OverlayPtr()->SetVisible(Enabled);
    }
}

void SettingsViewController::OnPresetChanged() { SaveAndApply(); }

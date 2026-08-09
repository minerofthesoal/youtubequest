#pragma once
#include "custom-types/shared/macros.hpp"
#include "custom-types/shared/types.hpp"
#include "HMUI/ViewController.hpp"
#include "System/Collections/Generic/List_1.hpp"
#include "bsml/shared/BSML.hpp" // VERIFY: actual include path/namespace in your restored `bsml` package

#include "Config.hpp"

// VERIFY: on Quest-BSML, `value="FieldName"` binds directly to a public field
// or a get_/set_ method pair on this host by name via il2cpp reflection --
// there is no [UIValue]/[UIAction] attribute step like PC BSML. That means
// every field referenced from BSMLLayouts.hpp's `value="..."` MUST exist
// below with a matching name, and every `on-click="..."` MUST be a
// DECLARE_INSTANCE_METHOD with that exact name. Double check the exact
// binding rules against whichever bsml release your qpm.json resolves --
// this has been a moving target across Quest-BSML versions.
DECLARE_CLASS_CODEGEN(YouTubeLiveChat::UI, SettingsViewController, HMUI::ViewController,

public:
    DECLARE_INSTANCE_METHOD(void, ctor);
    DECLARE_INSTANCE_METHOD(void, DidActivate, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling);

    // --- BSML-bound fields (see BSMLLayouts.hpp `value="..."`) ---
    DECLARE_INSTANCE_FIELD(StringW, ApiKey);
    DECLARE_INSTANCE_FIELD(StringW, VideoIdOrUrl);
    DECLARE_INSTANCE_FIELD(bool, Enabled);
    DECLARE_INSTANCE_FIELD(bool, AutoReconnect);
    DECLARE_INSTANCE_FIELD(bool, DebugLogging);

    DECLARE_INSTANCE_FIELD(int, PresetIndex);
    DECLARE_INSTANCE_FIELD(System::Collections::Generic::List_1<StringW>*, PresetChoices);
    DECLARE_INSTANCE_FIELD(float, HorizontalOffset);
    DECLARE_INSTANCE_FIELD(float, VerticalOffset);
    DECLARE_INSTANCE_FIELD(float, Distance);
    DECLARE_INSTANCE_FIELD(float, Width);
    DECLARE_INSTANCE_FIELD(float, Height);
    DECLARE_INSTANCE_FIELD(float, Scale);
    DECLARE_INSTANCE_FIELD(float, Opacity);

    DECLARE_INSTANCE_FIELD(int, MaxVisibleMessages);
    DECLARE_INSTANCE_FIELD(float, MessageDurationSeconds);
    DECLARE_INSTANCE_FIELD(bool, ShowRegularMessages);
    DECLARE_INSTANCE_FIELD(bool, ShowSuperChats);
    DECLARE_INSTANCE_FIELD(bool, ShowSuperStickers);
    DECLARE_INSTANCE_FIELD(bool, ShowMembershipEvents);
    DECLARE_INSTANCE_FIELD(bool, ShowUsernames);
    DECLARE_INSTANCE_FIELD(bool, ShowProfilePictures);
    DECLARE_INSTANCE_FIELD(bool, HighlightSuperChats);
    DECLARE_INSTANCE_FIELD(bool, HighlightMemberships);
    DECLARE_INSTANCE_FIELD(bool, NotificationSounds);

    DECLARE_INSTANCE_FIELD(StringW, StatusText);

    // --- on-click handlers ---
    DECLARE_INSTANCE_METHOD(void, ConnectClicked);
    DECLARE_INSTANCE_METHOD(void, DisconnectClicked);

    // --- on-change handlers that need extra logic beyond "just save" ---
    DECLARE_INSTANCE_METHOD(void, OnApiKeyChanged);
    DECLARE_INSTANCE_METHOD(void, OnVideoChanged);
    DECLARE_INSTANCE_METHOD(void, OnEnabledChanged);
    DECLARE_INSTANCE_METHOD(void, OnPresetChanged);

    void RefreshFromConfig(const YouTubeLiveChat::ModConfig& cfg);
    void SetStatus(const std::string& text);

private:
    YouTubeLiveChat::ModConfig BuildConfigFromFields();
    void SaveAndApply();
)

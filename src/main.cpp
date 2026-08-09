#include "scotland2/shared/modloader.h"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "custom-types/shared/register.hpp"
#include "paper2_scotland2/shared/logger.hpp"

#include "UI/SettingsViewController.hpp"
#include "UI/ChatOverlayController.hpp"
#include "Hooks/MainFlowCoordinatorHooks.hpp"
#include "ModState.hpp"

// VERIFY: exact BSML settings-menu registration call for your restored
// `bsml` package -- shown below in the shape most current Quest-BSML mods
// use (register a codegen'd ViewController type + display name, torn down
// automatically on mod disable). Some bsml versions want an instance,
// others a type; check Quest-BSML's own example mod for the current form.
#include "bsml/shared/BSML.hpp"

static modloader::ModInfo modInfo{MOD_ID, VERSION, 0};

static const Logger& log() {
    static auto logger = Paper::ConstLoggerContext<MOD_ID>();
    return logger;
}

extern "C" void setup(CModInfo* info) {
    info->id = MOD_ID;
    info->version = VERSION;
    info->version_long = 0;
    modInfo.assign_from(*info);

    log().info("YouTube Live Chat: setup()");
}

extern "C" void load() {
    log().info("YouTube Live Chat: load()");

    custom_types::Register::AutoRegister();

    YouTubeLiveChat::Hooks::InstallHooks();

    BSML::Init();

    // VERIFY: different Quest-BSML releases have exposed this registration
    // call in different shapes -- e.g. RegisterSettingsMenu<T>(name),
    // RegisterSettingsMenu(name, T::New_ctor), or a BSMLSettings::instance()
    // ->AddSettingsMenu(...) singleton call. The line below is the current
    // common form; if it doesn't compile against your restored `bsml`
    // package, check that package's own example mod for the exact call --
    // SettingsViewController itself (ctor/DidActivate/parse_and_construct)
    // is correct regardless of which registration API wires it in.
    BSML::Register::RegisterSettingsMenu<YouTubeLiveChat::UI::SettingsViewController*>("YouTube Live Chat");

    // Auto-connect-on-launch (if a key + video were already saved from a
    // previous session) happens in ChatOverlayController::Awake(), once a
    // MonoBehaviour host actually exists to run the polling coroutine on --
    // see MainFlowCoordinatorHooks.cpp for when that fires.
}

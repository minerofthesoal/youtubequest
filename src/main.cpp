#include "scotland2/shared/modloader.h"
#include "scotland2/shared/loader.hpp"

#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "custom-types/shared/register.hpp"
#include "bsml/shared/BSML.hpp"

#include "Logging.hpp"
#include "ModState.hpp"
#include "Hooks/Hooks.hpp"
#include "UI/SettingsViewController.hpp"

static modloader::ModInfo modInfo{MOD_ID, VERSION, 0};

// The build compiles with -fvisibility=hidden, so these two have to be marked
// default-visibility explicitly. Without that the symbols never make it into
// the .so's dynamic symbol table, Scotland2's dlsym finds nothing, and the mod
// loads "successfully" while doing absolutely nothing -- with only a
// "No late_load function on mod" line buried in the log to show for it.
#define YTLC_EXPORT extern "C" __attribute__((visibility("default")))

YTLC_EXPORT void setup(CModInfo* info) {
    info->id = MOD_ID;
    info->version = VERSION;
    info->version_long = 0;
    modInfo.assign(*info);

    YouTubeLiveChat::Log().info("YouTube Live Chat: setup()");
}

YTLC_EXPORT void late_load() {
    YouTubeLiveChat::Log().info("YouTube Live Chat: late_load()");

    // Registers every DECLARE_CLASS_CODEGEN type in this library with il2cpp.
    // Must happen before anything tries to AddComponent one of them.
    custom_types::Register::AutoRegister();

    YouTubeLiveChat::Hooks::InstallHooks();

    BSML::Init();
    BSML::Register::RegisterSettingsMenu<YouTubeLiveChat::UI::SettingsViewController*>(
        "YouTube Live Chat");

    // Loading the config here (rather than lazily on first use) means a
    // corrupt or missing file is reported in the log at startup instead of
    // halfway through a level.
    YouTubeLiveChat::ModState::Config();
    YouTubeLiveChat::ModState::InstallManagerCallbacks();
}

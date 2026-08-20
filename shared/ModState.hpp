#pragma once
#include "Config.hpp"
#include "ChatManager.hpp"

namespace YouTubeLiveChat {

namespace UI {
class ChatOverlayController;
class SettingsViewController;
}  // namespace UI

// Process-wide singletons. A Beat Saber mod only ever has one of these
// concerns active at a time, so Meyers singletons here are simpler and safer
// than threading a context object through every hook -- there is exactly one
// YouTube connection and one overlay for the lifetime of the game process.
class ModState {
public:
    static ModConfig& Config();
    static ChatManager& Manager();

    // Set by ChatOverlayController::Awake/OnDestroy; null when no overlay
    // exists yet (e.g. before the first scene loads it in).
    static UI::ChatOverlayController*& OverlayPtr();

    // Set by SettingsViewController while the settings screen is open.
    static UI::SettingsViewController*& SettingsPtr();

    // Persists the current config and pushes it to everything that cares.
    static void ApplyConfig(const ModConfig& config);

    // Wires ChatManager's callbacks to whichever UI objects currently exist.
    // Registered once at load; the fan-out re-checks OverlayPtr()/SettingsPtr()
    // on every event, so UI that comes and goes with scene changes never has
    // to re-subscribe (and can never leave a dangling callback behind).
    static void InstallManagerCallbacks();
};

}  // namespace YouTubeLiveChat

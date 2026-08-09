#pragma once
#include "Config.hpp"
#include "ChatManager.hpp"

namespace YouTubeLiveChat {

namespace UI { class ChatOverlayController; }

// Process-wide singletons. A Beat Saber mod only ever has one of these
// concerns active at a time, so Meyers singletons here are simpler and
// safer than threading a context object through every hook -- there is
// exactly one YouTube connection and one overlay for the lifetime of the
// game process.
class ModState {
public:
    static ModConfig& Config();
    static ChatManager& Manager();

    // Set by ChatOverlayController::ctor/OnDestroy; null when no overlay
    // exists yet (e.g. before the first scene loads it in).
    static UI::ChatOverlayController*& OverlayPtr();
};

}

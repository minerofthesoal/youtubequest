#include "ModState.hpp"

namespace YouTubeLiveChat {

ModConfig& ModState::Config() {
    static ModConfig instance = ModConfig::Load();
    return instance;
}

ChatManager& ModState::Manager() {
    static ChatManager instance;
    return instance;
}

UI::ChatOverlayController*& ModState::OverlayPtr() {
    static UI::ChatOverlayController* instance = nullptr;
    return instance;
}

}

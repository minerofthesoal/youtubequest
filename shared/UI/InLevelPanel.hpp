#pragma once
#include "UnityEngine/Vector3.hpp"
#include "bsml/shared/BSML/FloatingScreen/FloatingScreen.hpp"
#include "HMUI/CurvedTextMeshPro.hpp"
#include "UnityEngine/UI/Button.hpp"
#include "UnityEngine/Object.hpp"  // SafePtr static_assert needs Object complete

#include "beatsaber-hook/shared/utils/typedefs-wrappers.hpp"

namespace YouTubeLiveChat::UI {

// A small control panel shown while a song is paused.
//
// The BSML settings menu lives in the main-menu flow coordinator, so it is
// unreachable once a level starts -- which is exactly when you are most likely
// to want to hide the chat, or reconnect after the stream dropped. This panel
// gives you those controls without leaving the song: it is created on demand
// when the pause menu opens, placed in front of you, and hidden again on
// resume.
class InLevelPanel {
public:
    static InLevelPanel& Instance();

    // Shown/hidden by the PauseMenuManager hooks.
    void Show();
    void Hide();
    void Refresh();

private:
    SafePtrUnity<BSML::FloatingScreen> screen_;
    SafePtrUnity<HMUI::CurvedTextMeshPro> statusText_;
    SafePtrUnity<UnityEngine::UI::Button> saberButton_;
    bool built_ = false;

    void Build();
    UnityEngine::Vector3 PlacementInFrontOfHead() const;
};

}  // namespace YouTubeLiveChat::UI

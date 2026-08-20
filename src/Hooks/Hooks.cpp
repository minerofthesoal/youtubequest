#include "Hooks/Hooks.hpp"
#include "ModState.hpp"
#include "Logging.hpp"
#include "UI/ChatOverlayController.hpp"
#include "UI/InLevelPanel.hpp"

#include "beatsaber-hook/shared/utils/hooking.hpp"

#include "GlobalNamespace/MainFlowCoordinator.hpp"
#include "GlobalNamespace/PauseMenuManager.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Object.hpp"

using namespace GlobalNamespace;

MAKE_HOOK_MATCH(YTLC_MainFlowCoordinator_DidActivate, &MainFlowCoordinator::DidActivate, void,
                MainFlowCoordinator* self, bool firstActivation, bool addedToHierarchy,
                bool screenSystemEnabling) {
    YTLC_MainFlowCoordinator_DidActivate(self, firstActivation, addedToHierarchy, screenSystemEnabling);

    if (!firstActivation) return;
    if (YouTubeLiveChat::ModState::OverlayPtr()) return;

    YouTubeLiveChat::Log().info("Spawning YouTube Live Chat overlay");
    auto* go = UnityEngine::GameObject::New_ctor(StringW("YTLC_OverlayRoot"));
    UnityEngine::Object::DontDestroyOnLoad(go);
    go->AddComponent<YouTubeLiveChat::UI::ChatOverlayController*>();
}

MAKE_HOOK_MATCH(YTLC_PauseMenuManager_ShowMenu, &PauseMenuManager::ShowMenu, void,
                PauseMenuManager* self) {
    YTLC_PauseMenuManager_ShowMenu(self);
    YouTubeLiveChat::UI::InLevelPanel::Instance().Show();
}

MAKE_HOOK_MATCH(YTLC_PauseMenuManager_StartResumeAnimation, &PauseMenuManager::StartResumeAnimation,
                void, PauseMenuManager* self) {
    YouTubeLiveChat::UI::InLevelPanel::Instance().Hide();
    YTLC_PauseMenuManager_StartResumeAnimation(self);
}

MAKE_HOOK_MATCH(YTLC_PauseMenuManager_MenuButtonPressed, &PauseMenuManager::MenuButtonPressed, void,
                PauseMenuManager* self) {
    // Leaving the level back to the menu: the pause panel has no business
    // hanging in the air over the main menu.
    YouTubeLiveChat::UI::InLevelPanel::Instance().Hide();
    YTLC_PauseMenuManager_MenuButtonPressed(self);
}

namespace YouTubeLiveChat::Hooks {

void InstallHooks() {
    INSTALL_HOOK(Log(), YTLC_MainFlowCoordinator_DidActivate);
    INSTALL_HOOK(Log(), YTLC_PauseMenuManager_ShowMenu);
    INSTALL_HOOK(Log(), YTLC_PauseMenuManager_StartResumeAnimation);
    INSTALL_HOOK(Log(), YTLC_PauseMenuManager_MenuButtonPressed);
}

}  // namespace YouTubeLiveChat::Hooks

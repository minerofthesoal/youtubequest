#include "MainFlowCoordinatorHooks.hpp"
#include "ModState.hpp"
#include "UI/ChatOverlayController.hpp"

#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "GlobalNamespace/MainFlowCoordinator.hpp"
#include "UnityEngine/GameObject.hpp"

#include "paper2_scotland2/shared/logger.hpp"

using namespace GlobalNamespace;

static const Logger& log() {
    static auto logger = Paper::ConstLoggerContext<"YTLiveChat.Hooks">();
    return logger;
}

// VERIFY: MainFlowCoordinator::DidActivate's parameter list against your
// generated codegen for Beat Saber 1.40.8 -- flow coordinators in recent BS
// versions have used this (bool, bool, bool) shape for a long time, but it
// is exactly the kind of thing that only the real generated header for your
// exact build can confirm. If the signature differs, this hook simply won't
// match at compile time, which is a safe failure mode (build error, not a
// silent runtime mismatch).
MAKE_HOOK_MATCH(MainFlowCoordinator_DidActivate, &MainFlowCoordinator::DidActivate, void,
                MainFlowCoordinator* self, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    MainFlowCoordinator_DidActivate(self, firstActivation, addedToHierarchy, screenSystemEnabling);

    if (firstActivation && !YouTubeLiveChat::ModState::OverlayPtr()) {
        log().info("Spawning YouTube Live Chat overlay root");
        auto* go = UnityEngine::GameObject::New_ctor("YTLC_OverlayRoot");
        go->AddComponent<YouTubeLiveChat::UI::ChatOverlayController*>();
    }
}

namespace YouTubeLiveChat::Hooks {

void InstallHooks() {
    INSTALL_HOOK(log(), MainFlowCoordinator_DidActivate);
}

}

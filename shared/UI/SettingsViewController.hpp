#pragma once
#include "custom-types/shared/macros.hpp"
#include "custom-types/shared/types.hpp"

#include "HMUI/ViewController.hpp"
#include "HMUI/CurvedTextMeshPro.hpp"
#include "UnityEngine/UI/Button.hpp"

#include "beatsaber-hook/shared/utils/typedefs-wrappers.hpp"

#include <functional>
#include <string>

#include "Config.hpp"

// The settings screen, registered with BSML under Settings -> Mods.
//
// Built imperatively with BSML-Lite rather than from a BSML XML document on
// purpose: XML binds `value="Foo"` / `on-click="Bar"` to the host object by
// *name* through il2cpp reflection, so a typo or a renamed field is a silent
// runtime no-op. Building the same controls in C++ makes every binding a
// direct lambda the compiler checks.
DECLARE_CLASS_CODEGEN(YouTubeLiveChat::UI, SettingsViewController, HMUI::ViewController) {
   public:
    DECLARE_DEFAULT_CTOR();

    // DidActivate/DidDeactivate are *virtual* on HMUI::ViewController, so they
    // need OVERRIDE_METHOD_MATCH rather than INSTANCE_METHOD: the latter would
    // declare a brand-new C# method that the base class's virtual dispatch
    // never calls, leaving the settings screen permanently blank.
    DECLARE_OVERRIDE_METHOD_MATCH(void, DidActivate, &HMUI::ViewController::DidActivate,
                                  bool firstActivation, bool addedToHierarchy,
                                  bool screenSystemEnabling);
    DECLARE_OVERRIDE_METHOD_MATCH(void, DidDeactivate, &HMUI::ViewController::DidDeactivate,
                                  bool removedFromHierarchy, bool screenSystemDisabling);

    void SetStatus(std::string const& text);
    void SetSignInPrompt(std::string const& userCode, std::string const& verificationUrl);
    // Re-reads the shared config and updates the labels this controller owns.
    void RefreshFromConfig();

   private:
    SafePtrUnity<HMUI::CurvedTextMeshPro> statusText_;
    SafePtrUnity<HMUI::CurvedTextMeshPro> authStatusText_;
    SafePtrUnity<UnityEngine::UI::Button> signInButton_;

    std::string statusLine_;
    std::string promptLine_;

    void BuildUI();
    void BuildConnectionSection(UnityEngine::Transform * parent);
    void BuildPlacementSection(UnityEngine::Transform * parent);
    void BuildMessageSection(UnityEngine::Transform * parent);
    void RefreshAuthStatus();

    // Mutates a copy of the shared config, persists it, and pushes it out.
    void Edit(std::function<void(YouTubeLiveChat::ModConfig&)> const& mutate);
};

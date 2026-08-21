#include "UI/InLevelPanel.hpp"
#include "UI/ChatOverlayController.hpp"
#include "ModState.hpp"
#include "Logging.hpp"

#include "bsml/shared/BSML-Lite.hpp"

#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Object.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Vector2.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "TMPro/TextAlignmentOptions.hpp"

using namespace UnityEngine;

namespace YouTubeLiveChat::UI {

InLevelPanel& InLevelPanel::Instance() {
    static InLevelPanel instance;
    return instance;
}

Vector3 InLevelPanel::PlacementInFrontOfHead() const {
    Camera* camera = Camera::get_main();
    if (!camera) return Vector3(0.0f, 1.2f, 1.6f);
    Transform* head = camera->get_transform();
    // Below eye level and a little closer than the pause menu itself, so it
    // sits under the pause buttons rather than fighting them for space.
    return head->TransformPoint(Vector3(0.0f, -0.45f, 1.4f));
}

void InLevelPanel::Build() {
    if (built_) return;

    screen_ = BSML::Lite::CreateFloatingScreen(Vector2(64.0f, 46.0f),
                                               Vector3(0.0f, 1.2f, 1.6f),
                                               Vector3(0.0f, 0.0f, 0.0f),
                                               /*curvatureRadius*/ 0.0f,
                                               /*hasBackground*/ true,
                                               /*createHandle*/ false);
    if (!screen_) {
        Log().error("Failed to create the in-level panel");
        return;
    }

    GameObject* go = screen_->get_gameObject();
    go->set_name(StringW("YTLC_InLevelPanel"));
    // Survives the pause/resume cycle and the level teardown, so the panel is
    // built once per session instead of once per pause.
    Object::DontDestroyOnLoad(go);

    Transform* root = screen_->get_transform();

    BSML::Lite::CreateText(root, StringW("<b>YouTube Live Chat</b>"), 3.6f,
                           Vector2(0.0f, 18.0f), Vector2(60.0f, 6.0f))
        ->set_alignment(TMPro::TextAlignmentOptions::Center);

    statusText_ = BSML::Lite::CreateText(root, StringW(""), 3.0f,
                                         Vector2(0.0f, 11.0f), Vector2(60.0f, 8.0f));
    statusText_->set_alignment(TMPro::TextAlignmentOptions::Center);
    statusText_->set_enableWordWrapping(true);

    BSML::Lite::CreateUIButton(root, StringW("Show / hide"), Vector2(-13.0f, 1.0f),
                               Vector2(22.0f, 8.0f), []() {
                                   ModConfig cfg = ModState::Config();
                                   cfg.enabled = !cfg.enabled;
                                   ModState::ApplyConfig(cfg);
                                   Instance().Refresh();
                               });

    BSML::Lite::CreateUIButton(root, StringW("Reconnect"), Vector2(13.0f, 1.0f),
                               Vector2(22.0f, 8.0f), []() {
                                   ModState::Manager().Connect();
                                   Instance().Refresh();
                               });

    BSML::Lite::CreateUIButton(root, StringW("Clear"), Vector2(-13.0f, -9.0f),
                               Vector2(22.0f, 8.0f), []() {
                                   if (ModState::OverlayPtr()) ModState::OverlayPtr()->ClearMessages();
                               });

    BSML::Lite::CreateUIButton(root, StringW("Disconnect"), Vector2(13.0f, -9.0f),
                               Vector2(22.0f, 8.0f), []() {
                                   ModState::Manager().Disconnect();
                                   Instance().Refresh();
                               });

    // Placing the panel is the one thing you can only really do in a level,
    // since that is where the sabers are.
    saberButton_ = BSML::Lite::CreateUIButton(root, StringW("Move with red saber"),
                                              Vector2(0.0f, -19.0f), Vector2(50.0f, 8.0f), []() {
                                                  auto* overlay = ModState::OverlayPtr();
                                                  if (!overlay) return;
                                                  overlay->SetSaberPlacement(!overlay->SaberPlacementActive());
                                                  Instance().Refresh();
                                              });

    built_ = true;
    go->SetActive(false);
}

void InLevelPanel::Show() {
    Build();
    if (!screen_) return;

    Transform* transform = screen_->get_transform();
    Camera* camera = Camera::get_main();
    transform->set_position(PlacementInFrontOfHead());
    if (camera) {
        Vector3 euler = camera->get_transform()->get_rotation().get_eulerAngles();
        // Match the player's yaw only: inheriting pitch and roll would leave
        // the panel tilted at whatever angle their head happened to be at.
        transform->set_rotation(Quaternion::Euler(20.0f, euler.y, 0.0f));
    }

    screen_->get_gameObject()->SetActive(true);
    Refresh();
}

void InLevelPanel::Hide() {
    if (!screen_) return;
    screen_->get_gameObject()->SetActive(false);
}

void InLevelPanel::Refresh() {
    if (!statusText_) return;
    auto& manager = ModState::Manager();
    std::string line = ToString(manager.State());
    if (!manager.StatusDetail().empty()) line += " - " + manager.StatusDetail();
    if (!ModState::Config().enabled) line += "  (overlay hidden)";

    auto* overlay = ModState::OverlayPtr();
    const bool placing = overlay && overlay->SaberPlacementActive();
    if (placing) line = "Point the red saber where you want the panel";
    if (saberButton_) {
        BSML::Lite::SetButtonText(saberButton_.ptr(),
                                  StringW(placing ? "Drop it here" : "Move with red saber"));
    }

    statusText_->set_text(StringW("<color=#B0B0B0>" + line + "</color>"));
}

}  // namespace YouTubeLiveChat::UI

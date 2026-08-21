#include "UI/ChatOverlayController.hpp"
#include "ModState.hpp"
#include "Logging.hpp"

#include "bsml/shared/BSML-Lite.hpp"

#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Object.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/RectTransform.hpp"
#include "UnityEngine/Vector2.hpp"
#include "UnityEngine/Vector3.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Time.hpp"
#include "TMPro/TextAlignmentOptions.hpp"
#include "GlobalNamespace/SaberManager.hpp"
#include "GlobalNamespace/Saber.hpp"
#include "GlobalNamespace/SaberType.hpp"
#include "GlobalNamespace/VRController.hpp"
#include "UnityEngine/XR/XRNode.hpp"

#include "beatsaber-hook/shared/utils/typedefs.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>

using namespace UnityEngine;
using namespace YouTubeLiveChat;
using namespace YouTubeLiveChat::UI;

DEFINE_TYPE(YouTubeLiveChat::UI, ChatOverlayController);

namespace {

uint64_t NowMillis() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

// Head-relative *direction* for each preset, scaled by the configured
// distance. Each is roughly unit length, so `distance` means what it says --
// metres from your head to the panel.
//
// The z component is per-preset rather than always "straight ahead": the side
// presets used to sit the full distance forward *and* offset sideways, which
// put the panel out in front of the player in the play space instead of off
// to the side of it. Middle-left/right are now mostly lateral with just
// enough forward lean to stay in peripheral vision.
Vector3 PresetBaseOffset(PanelPreset p) {
    switch (p) {
        case PanelPreset::TopLeft:     return Vector3(-0.48f, 0.32f, 0.82f);
        case PanelPreset::TopCenter:   return Vector3(0.0f, 0.40f, 0.92f);
        case PanelPreset::TopRight:    return Vector3(0.48f, 0.32f, 0.82f);
        case PanelPreset::MiddleLeft:  return Vector3(-0.88f, 0.0f, 0.48f);
        case PanelPreset::MiddleRight: return Vector3(0.88f, 0.0f, 0.48f);
        case PanelPreset::BottomLeft:  return Vector3(-0.48f, -0.34f, 0.82f);
        case PanelPreset::BottomRight: return Vector3(0.48f, -0.34f, 0.82f);
        case PanelPreset::Custom:
        default:                       return Vector3(0.0f, 0.0f, 0.0f);
    }
}

float Distance(Vector3 const& a, Vector3 const& b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// cordl value types don't get C++ arithmetic operators, so the vector maths
// here is written out rather than going through Vector3::op_Subtraction.
Vector3 Subtract(Vector3 const& a, Vector3 const& b) {
    return Vector3(a.x - b.x, a.y - b.y, a.z - b.z);
}

// World-space point at the tip of the red saber, or of the left controller
// when there are no sabers (i.e. anywhere outside a level).
std::optional<Vector3> RedSaberTip() {
    auto* saberManager = Object::FindObjectOfType<GlobalNamespace::SaberManager*>();
    if (saberManager) {
        // Left saber is SaberA, the red one.
        auto saber = saberManager->get_leftSaber();
        if (saber) return saber->get_saberBladeTopPos();
    }

    auto controllers = Object::FindObjectsOfType<GlobalNamespace::VRController*>();
    for (auto controller : controllers) {
        if (!controller) continue;
        if (controller->get_node().value__ != UnityEngine::XR::XRNode::LeftHand.value__) continue;
        // No blade to measure from out here, so project a saber's length
        // forward from the controller to keep the gesture feeling the same.
        Transform* t = controller->get_transform();
        Vector3 pos = t->get_position();
        Vector3 fwd = t->get_forward();
        return Vector3(pos.x + fwd.x * 0.9f, pos.y + fwd.y * 0.9f, pos.z + fwd.z * 0.9f);
    }
    return std::nullopt;
}

}  // namespace

float ChatOverlayController::ScreenWidthUnits() const {
    return std::max(20.0f, config_.width * kScreenUnitsPerMeter);
}

float ChatOverlayController::ScreenHeightUnits() const {
    return std::max(20.0f, config_.height * kScreenUnitsPerMeter);
}

void ChatOverlayController::Awake() {
    ModState::OverlayPtr() = this;
    config_ = ModState::Config();

    Object::DontDestroyOnLoad(get_gameObject());

    avatarCache_ = std::make_unique<AvatarCache>(this);

    Build();

    auto& manager = ModState::Manager();
    // This component is the coroutine host for the whole mod: it is
    // DontDestroyOnLoad, so a poll or a sign-in in flight is not cancelled
    // when a level loads. The callbacks themselves are wired once at
    // late_load by ModState::InstallManagerCallbacks.
    manager.SetHost(this);
    manager.Configure(config_);

    ApplyConfig(config_);

    // Auto-connect if the mod already has everything it needs, so a stream
    // that was working last session just comes back on its own.
    const bool oauthReady = config_.authMode == AuthMode::OAuth && !config_.oauthRefreshToken.empty();
    const bool apiKeyReady = config_.authMode == AuthMode::ApiKey && !config_.apiKey.empty() &&
                             !config_.videoIdOrUrl.empty();
    if (config_.enabled && (oauthReady || apiKeyReady)) {
        manager.Connect();
    } else {
        SetStatus(config_.enabled ? "Not configured - open Settings > YouTube Live Chat"
                                  : "Overlay disabled");
    }
}

void ChatOverlayController::Build() {
    if (built_) return;

    screen_ = BSML::Lite::CreateFloatingScreen(
        Vector2(ScreenWidthUnits(), ScreenHeightUnits()),
        Vector3(0.0f, 1.6f, 2.2f),
        Vector3(0.0f, 0.0f, 0.0f),
        /*curvatureRadius*/ 0.0f,
        /*hasBackground*/ true,
        /*createHandle*/ true);

    if (!screen_) {
        YouTubeLiveChat::Log().error("Failed to create the chat FloatingScreen");
        return;
    }

    UnityEngine::GameObject* screenGo = screen_->get_gameObject();
    screenGo->set_name(StringW("YTLC_ChatOverlay"));
    Object::DontDestroyOnLoad(screenGo);

    // One CanvasGroup drives the whole panel's opacity, and keeps it from
    // eating pointer input while you are playing.
    canvasGroup_ = screenGo->AddComponent<CanvasGroup*>();

    UnityEngine::Transform* root = screen_->get_transform();

    statusText_ = BSML::Lite::CreateText(root, StringW("YouTube Live Chat"), 3.4f,
                                         Vector2(0.0f, (ScreenHeightUnits() / 2.0f) - 4.0f),
                                         Vector2(ScreenWidthUnits() - 4.0f, 6.0f));
    statusText_->set_alignment(TMPro::TextAlignmentOptions::Center);

    promptText_ = BSML::Lite::CreateText(root, StringW(""), 4.2f,
                                         Vector2(0.0f, 0.0f),
                                         Vector2(ScreenWidthUnits() - 4.0f, 20.0f));
    promptText_->set_alignment(TMPro::TextAlignmentOptions::Center);
    promptText_->set_richText(true);
    promptText_->get_gameObject()->SetActive(false);

    audioSource_ = screenGo->AddComponent<AudioSource*>();
    audioSource_->set_spatialBlend(0.0f);  // 2D beep, not positional
    audioSource_->set_playOnAwake(false);

    BuildRowPool();
    built_ = true;
}

void ChatOverlayController::BuildRowPool() {
    rows_.reserve(kMaxPoolSize);
    freeList_.reserve(kMaxPoolSize);
    UnityEngine::Transform* root = screen_->get_transform();
    for (size_t i = 0; i < kMaxPoolSize; i++) {
        rows_.push_back(MessageRow::Create(root, ScreenWidthUnits() - 4.0f, kRowHeight));
        freeList_.push_back(i);
    }
}

void ChatOverlayController::ApplyConfig(ModConfig const& cfg) {
    config_ = cfg;
    if (!built_ || !screen_) return;

    screen_->set_ScreenSize(Vector2(ScreenWidthUnits(), ScreenHeightUnits()));

    // cfg.scale is a multiplier the player sets, NOT the transform scale.
    // BSML creates a FloatingScreen at localScale 0.02 because its
    // RectTransform is sized in 1/50 m screen units; assigning cfg.scale
    // straight into localScale (as this used to) replaced 0.02 with 1.0 and
    // made the panel fifty times too big -- a ~45 x 55 m wall centred a metre
    // or two in front of the player, i.e. one they are standing inside. That
    // is why moving the panel appeared to do nothing at all: the sliders were
    // working, but shifting the centre of a 45 m panel by 30 cm is invisible.
    const float scale = kFloatingScreenScale * std::clamp(cfg.scale, 0.1f, 4.0f);
    screen_->get_transform()->set_localScale(Vector3(scale, scale, scale));

    // The grab handle only makes sense in free-placement mode -- while the
    // panel is head-following, dragging it would just snap back next frame.
    screen_->set_ShowHandle(!cfg.followHead);

    if (canvasGroup_) {
        canvasGroup_->set_alpha(std::clamp(cfg.opacity, 0.05f, 1.0f));
        canvasGroup_->set_blocksRaycasts(!cfg.followHead);
    }

    if (statusText_) {
        statusText_->get_rectTransform()->set_anchoredPosition(
            Vector2(0.0f, (ScreenHeightUnits() / 2.0f) - 4.0f));
        statusText_->get_rectTransform()->set_sizeDelta(Vector2(ScreenWidthUnits() - 4.0f, 6.0f));
    }

    for (auto& row : rows_) {
        if (row.root) row.root->set_sizeDelta(Vector2(0.0f, kRowHeight));
    }

    // Trim active rows if maxVisibleMessages shrank.
    while (static_cast<int32_t>(activeOrder_.size()) > cfg.maxVisibleMessages) {
        RecycleOldest();
    }

    RelayoutRows();
    ApplyPlacement(true);
    SetVisible(cfg.enabled);
    RefreshHeaderText();
}

void ChatOverlayController::SetVisible(bool visible) {
    if (screen_) screen_->get_gameObject()->SetActive(visible);
}

void ChatOverlayController::ApplyPlacement(bool immediate) {
    if (!screen_) return;
    UnityEngine::Transform* screenTransform = screen_->get_transform();

    if (!config_.followHead) {
        if (immediate) {
            Vector3 saved(config_.panelPosX, config_.panelPosY, config_.panelPosZ);
            screenTransform->set_position(saved);
            screenTransform->set_rotation(
                Quaternion::Euler(config_.panelRotX, config_.panelRotY, config_.panelRotZ));
            // Seed the drag tracker so the very first frame doesn't look like
            // the user just moved the panel.
            lastPlacementPos_ = saved;
            placementDirty_ = false;
            placementSaveTimer_ = 0.0f;
        }
        return;
    }

    UnityEngine::Camera* camera = Camera::get_main();
    if (!camera) return;
    UnityEngine::Transform* head = camera->get_transform();

    Vector3 base = PresetBaseOffset(config_.preset);
    const bool custom = config_.preset == PanelPreset::Custom;
    Vector3 localOffset(
        (custom ? config_.customX : base.x * config_.distance) + config_.horizontalOffset,
        (custom ? config_.customY : base.y * config_.distance) + config_.verticalOffset,
        custom ? config_.customZ : base.z * config_.distance);

    Vector3 targetPos = head->TransformPoint(localOffset);
    Quaternion targetRot = head->get_rotation();

    if (immediate) {
        screenTransform->set_position(targetPos);
        screenTransform->set_rotation(targetRot);
        return;
    }

    // Gentle follow rather than a rigid 1:1 head-lock -- a panel welded to
    // head rotation every frame reads as uncomfortably "swimmy" in VR, and is
    // actively distracting while you are trying to hit notes.
    float t = 1.0f - std::exp(-Time::get_deltaTime() * 6.0f);
    screenTransform->set_position(Vector3::Lerp(screenTransform->get_position(), targetPos, t));
    screenTransform->set_rotation(Quaternion::Slerp(screenTransform->get_rotation(), targetRot, t));
}

void ChatOverlayController::TrackFreePlacement() {
    if (config_.followHead || !screen_) return;

    UnityEngine::Transform* screenTransform = screen_->get_transform();
    Vector3 pos = screenTransform->get_position();

    // Debounce on "has it moved since last frame", not on "does it differ from
    // the saved value": the saved value only catches up once we write it, so
    // comparing against it would restart the timer forever and never save.
    if (Distance(pos, lastPlacementPos_) > 0.001f) {
        lastPlacementPos_ = pos;
        placementDirty_ = true;
        placementSaveTimer_ = 0.0f;
        return;
    }

    if (!placementDirty_) return;

    // Persist a couple of seconds after the panel stops moving, so dragging it
    // around doesn't rewrite the config file every frame.
    placementSaveTimer_ += Time::get_deltaTime();
    if (placementSaveTimer_ < 2.0f) return;

    placementDirty_ = false;
    placementSaveTimer_ = 0.0f;

    Vector3 euler = screenTransform->get_rotation().get_eulerAngles();
    ModConfig cfg = ModState::Config();
    cfg.panelPosX = pos.x;
    cfg.panelPosY = pos.y;
    cfg.panelPosZ = pos.z;
    cfg.panelRotX = euler.x;
    cfg.panelRotY = euler.y;
    cfg.panelRotZ = euler.z;
    config_ = cfg;
    ModState::Config() = cfg;
    cfg.Save();
}

void ChatOverlayController::Update() {
    if (!built_ || !screen_) return;
    if (!config_.enabled) {
        // Polling keeps running while the overlay is hidden (so re-enabling it
        // is instant), but the buffer must not grow without bound in the
        // meantime.
        incoming_.clear();
        return;
    }

    if (saberPlacement_) {
        if (!UpdateSaberPlacement()) {
            SetStatus("No saber or controller found to position with");
        }
    } else {
        ApplyPlacement(false);
        TrackFreePlacement();
    }

    if (!incoming_.empty()) {
        std::vector<ChatMessage> batch;
        batch.swap(incoming_);
        bool any = false;
        for (auto const& msg : batch) {
            if (!PassesFilter(msg)) continue;
            ApplyOneMessage(msg);
            any = true;
        }
        if (any) RelayoutRows();
    }

    ExpireOldMessages();
}

bool ChatOverlayController::PassesFilter(ChatMessage const& msg) const {
    switch (msg.type) {
        case ChatEventType::System:       return true;
        case ChatEventType::TextMessage:  return config_.showRegularMessages;
        case ChatEventType::SuperChat:    return config_.showSuperChats;
        case ChatEventType::SuperSticker: return config_.showSuperStickers;
        case ChatEventType::NewMember:
        case ChatEventType::MemberMilestone:
        case ChatEventType::MembershipGifting:
        case ChatEventType::GiftMembershipReceived:
            return config_.showMembershipEvents;
        case ChatEventType::MessageDeleted:
        case ChatEventType::MessageRetracted:
        case ChatEventType::UserBanned:
            // Moderation notices ride along with regular chat visibility.
            return config_.showRegularMessages;
        default:
            return false;
    }
}

void ChatOverlayController::RecycleOldest() {
    if (activeOrder_.empty()) return;
    size_t idx = activeOrder_.front();
    activeOrder_.pop_front();
    rows_[idx].SetVisible(false);
    freeList_.push_back(idx);
}

void ChatOverlayController::ApplyOneMessage(ChatMessage const& msg) {
    const int32_t maxRows = std::clamp<int32_t>(config_.maxVisibleMessages, 1,
                                                static_cast<int32_t>(kMaxPoolSize));
    while (static_cast<int32_t>(activeOrder_.size()) >= maxRows) {
        RecycleOldest();
    }

    if (freeList_.empty()) return;  // pool exhausted; should not happen given the trim above
    size_t idx = freeList_.back();
    freeList_.pop_back();

    auto& row = rows_[idx];
    row.ApplyMessage(msg, config_);
    activeOrder_.push_back(idx);

    if (config_.showProfilePictures && !msg.author.profileImageUrl.empty() && avatarCache_) {
        // Capture the pool index, not a reference to the row: by the time the
        // download finishes this row may already have been recycled for a
        // different message, and the generation check below catches that.
        uint64_t stamp = row.assignedAtMillis;
        avatarCache_->GetOrFetch(msg.author.profileImageUrl,
                                 [this, idx, stamp](UnityEngine::Texture2D* texture) {
                                     if (!texture || idx >= rows_.size()) return;
                                     auto& target = rows_[idx];
                                     if (!target.active || target.assignedAtMillis != stamp) return;
                                     if (target.avatar) target.avatar->set_texture(texture);
                                 });
    }

    if (config_.notificationSounds &&
        (msg.type == ChatEventType::SuperChat || msg.type == ChatEventType::SuperSticker ||
         msg.type == ChatEventType::NewMember)) {
        PlayNotificationBeep();
    }
}

void ChatOverlayController::RelayoutRows() {
    // Rows are anchored to the top edge with a top pivot, so anchoredPosition.y
    // is measured downwards from the top of the panel -- the header occupies
    // the first kHeaderHeight units.
    float y = -kHeaderHeight;
    for (size_t idx : activeOrder_) {
        auto& row = rows_[idx];
        if (!row.root) continue;
        row.root->set_anchoredPosition(Vector2(0.0f, y));
        y -= kRowHeight;
    }
}

void ChatOverlayController::ExpireOldMessages() {
    if (config_.messageDurationSeconds <= 0.0f) return;

    const uint64_t cutoff = static_cast<uint64_t>(config_.messageDurationSeconds * 1000.0f);
    const uint64_t now = NowMillis();
    bool changed = false;

    // Only the front can expire: rows are assigned in arrival order, so once
    // the oldest is still young enough, nothing behind it has expired either.
    while (!activeOrder_.empty()) {
        size_t idx = activeOrder_.front();
        if (now - rows_[idx].assignedAtMillis <= cutoff) break;
        RecycleOldest();
        changed = true;
    }
    if (changed) RelayoutRows();
}

void ChatOverlayController::SetSaberPlacement(bool active) {
    if (saberPlacement_ == active) return;
    saberPlacement_ = active;

    if (active) {
        // The handle would fight the saber for control of the same transform.
        if (screen_) screen_->set_ShowHandle(false);
        SetStatus("Point the red saber where you want the chat panel");
        return;
    }

    // Dropping it here: switch to fixed placement so the panel stays put
    // instead of springing back to the head-relative preset next frame.
    CommitCurrentPlacement();
    SetStatus("Chat panel placed");
}

void ChatOverlayController::CommitCurrentPlacement() {
    if (!screen_) return;
    UnityEngine::Transform* screenTransform = screen_->get_transform();
    Vector3 pos = screenTransform->get_position();
    Vector3 euler = screenTransform->get_rotation().get_eulerAngles();

    ModConfig cfg = ModState::Config();
    cfg.followHead = false;
    cfg.panelPosX = pos.x;
    cfg.panelPosY = pos.y;
    cfg.panelPosZ = pos.z;
    cfg.panelRotX = euler.x;
    cfg.panelRotY = euler.y;
    cfg.panelRotZ = euler.z;
    ModState::ApplyConfig(cfg);

    lastPlacementPos_ = pos;
    placementDirty_ = false;
    placementSaveTimer_ = 0.0f;
}

bool ChatOverlayController::UpdateSaberPlacement() {
    if (!screen_) return false;

    auto tip = RedSaberTip();
    if (!tip) return false;

    UnityEngine::Camera* camera = Camera::get_main();
    UnityEngine::Transform* screenTransform = screen_->get_transform();
    screenTransform->set_position(*tip);

    if (camera) {
        // Face the panel away from the head, so its front is towards you.
        Vector3 away = Subtract(*tip, camera->get_transform()->get_position());
        if (Distance(away, Vector3(0.0f, 0.0f, 0.0f)) > 0.01f) {
            screenTransform->set_rotation(Quaternion::LookRotation(away, Vector3::get_up()));
        }
    }
    return true;
}

void ChatOverlayController::PushMessages(std::vector<ChatMessage> const& messages) {
    incoming_.insert(incoming_.end(), messages.begin(), messages.end());
}

void ChatOverlayController::PushSystemMessage(std::string const& text) {
    ChatMessage msg{};
    msg.type = ChatEventType::System;
    msg.displayText = text;
    incoming_.push_back(std::move(msg));
}

void ChatOverlayController::ClearMessages() {
    while (!activeOrder_.empty()) RecycleOldest();
    incoming_.clear();
}

void ChatOverlayController::SetStatus(std::string const& text) {
    statusLine_ = text;
    RefreshHeaderText();
}

void ChatOverlayController::SetSignInPrompt(std::string const& userCode,
                                            std::string const& verificationUrl) {
    if (userCode.empty()) {
        promptLine_.clear();
    } else {
        promptLine_ = "<color=#FFFFFF>Go to <b>" + verificationUrl +
                      "</b>\nand enter code</color>\n<color=#FFD24D><b>" + userCode + "</b></color>";
    }
    if (promptText_) {
        promptText_->set_text(StringW(promptLine_));
        promptText_->get_gameObject()->SetActive(!promptLine_.empty());
    }
}

void ChatOverlayController::RefreshHeaderText() {
    if (!statusText_) return;
    std::string header = statusLine_.empty() ? "YouTube Live Chat" : statusLine_;
    statusText_->set_text(StringW("<color=#B0B0B0>" + header + "</color>"));
}

void ChatOverlayController::PlayNotificationBeep() {
    if (!audioSource_) return;

    if (!beepClip_) {
        // Generated procedurally so the .qmod doesn't have to ship an audio
        // asset (and so there is no asset-loading path to go wrong).
        const int sampleRate = 22050;
        const float durationSec = 0.12f;
        const int sampleCount = static_cast<int>(sampleRate * durationSec);

        std::vector<float> samples(static_cast<size_t>(sampleCount));
        for (int i = 0; i < sampleCount; i++) {
            float t = static_cast<float>(i) / static_cast<float>(sampleRate);
            // Linear fade-out, which avoids the click a hard cut would make.
            float envelope = 1.0f - (static_cast<float>(i) / static_cast<float>(sampleCount));
            samples[static_cast<size_t>(i)] =
                std::sin(2.0f * 3.14159265f * 880.0f * t) * 0.25f * envelope;
        }

        auto clip = AudioClip::Create(StringW("YTLC_Beep"), sampleCount, 1, sampleRate, false);
        if (!clip) return;
        clip->SetData(ArrayW<float>(samples), 0);
        beepClip_ = clip;
    }

    audioSource_->PlayOneShot(beepClip_.ptr(), 0.6f);
}

void ChatOverlayController::OnDestroy() {
    if (ModState::OverlayPtr() == this) {
        ModState::OverlayPtr() = nullptr;
    }
    ModState::Manager().SetHost(nullptr);
}

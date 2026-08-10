#include "ChatOverlayController.hpp"
#include "ModState.hpp"

#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Object.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/Vector2.hpp"
#include "UnityEngine/Vector3.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Color.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/UI/RectMask2D.hpp"
#include "UnityEngine/AudioClip.hpp"
#include "custom-types/shared/macros.hpp"

#include "paper2_scotland2/shared/logger.hpp"

#include <cmath>
#include <chrono>

using namespace UnityEngine;
using namespace YouTubeLiveChat;
using namespace YouTubeLiveChat::UI;

static auto& log() {
    static auto logger = Paper::ConstLoggerContext("YTLiveChat.Overlay");
    return logger;
}

DEFINE_TYPE(YouTubeLiveChat::UI, ChatOverlayController);

namespace {
    uint64_t NowMillis() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    // Approximate corner offsets (meters, at 1m distance) for each preset,
    // scaled by actual distance in RepositionCanvasRelativeToHead. This is a
    // simple head-relative approximation, not a true FOV-edge calculation --
    // good enough for a chat panel that just needs to sit out of the way in
    // a consistent spot, not to exactly hug the render frustum edge.
    Vector3 PresetBaseOffset(PanelPreset p) {
        switch (p) {
            case PanelPreset::TopLeft:      return Vector3(-0.45f, 0.30f, 0);
            case PanelPreset::TopCenter:    return Vector3(0.0f,   0.35f, 0);
            case PanelPreset::TopRight:     return Vector3(0.45f,  0.30f, 0);
            case PanelPreset::MiddleLeft:   return Vector3(-0.55f, 0.0f,  0);
            case PanelPreset::MiddleRight:  return Vector3(0.55f,  0.0f,  0);
            case PanelPreset::BottomLeft:   return Vector3(-0.45f, -0.30f, 0);
            case PanelPreset::BottomRight:  return Vector3(0.45f,  -0.30f, 0);
            case PanelPreset::Custom:
            default:                        return Vector3(0, 0, 0);
        }
    }
}

void ChatOverlayController::ctor() {
    INVOKE_CTOR();
}

void ChatOverlayController::Awake() {
    ModState::OverlayPtr() = this;
    Object::DontDestroyOnLoad(get_gameObject());

    config_ = ModState::Config();
    avatarCache_ = std::make_unique<AvatarCache>(this);

    BuildCanvas();
    BuildRowPool();

    audioSource_ = get_gameObject()->AddComponent<AudioSource*>();
    audioSource_->set_spatialBlend(0.0f); // 2D beep, not positional

    ModState::Manager().SetHost(this);
    ModState::Manager().OnMessagesReceived([this](const std::vector<ChatMessage>& msgs) {
        std::lock_guard<std::mutex> lk(incomingMutex_); // see header: precautionary, not expected to contend
        incomingBuffer_.insert(incomingBuffer_.end(), msgs.begin(), msgs.end());
    });
    ModState::Manager().Configure(config_);

    SetVisible(config_.enabled);

    if (config_.enabled && !config_.apiKey.empty() && !config_.videoIdOrUrl.empty()) {
        ModState::Manager().Connect();
    }
}

void ChatOverlayController::BuildCanvas() {
    auto* go = GameObject::New_ctor("YTLC_Canvas");
    go->get_transform()->SetParent(get_transform(), false);

    canvas_ = go->AddComponent<Canvas*>();
    canvas_->set_renderMode(RenderMode::WorldSpace);

    canvasRoot_ = go->GetComponent<RectTransform*>();
    canvasRoot_->set_sizeDelta(Vector2(1000, 1200)); // fixed UI-unit canvas; world size comes from localScale in ApplyConfig
    go->AddComponent<UnityEngine::UI::RectMask2D*>(); // clip rows to panel bounds
}

void ChatOverlayController::BuildRowPool() {
    rows_.reserve(kMaxPoolSize);
    for (size_t i = 0; i < kMaxPoolSize; i++) {
        rows_.push_back(MessageRow::Create(canvasRoot_, 1000.0f, 70.0f));
        freeList_.push_back(i);
    }
}

void ChatOverlayController::ApplyConfig(const ModConfig& cfg) {
    config_ = cfg;

    float worldWidth = cfg.width * cfg.scale;
    float worldHeight = cfg.height * cfg.scale;
    canvasRoot_->get_transform()->set_localScale(Vector3(worldWidth / 1000.0f, worldHeight / 1200.0f, 1.0f));

    for (auto& row : rows_) {
        if (row.background) {
            Color c = row.background->get_color();
            c.a = cfg.opacity;
            row.background->set_color(c);
        }
    }

    // Trim active rows down if maxVisibleMessages shrank.
    while ((int)activeOrder_.size() > cfg.maxVisibleMessages) {
        size_t idx = activeOrder_.front();
        activeOrder_.pop_front();
        rows_[idx].SetVisible(false);
        freeList_.push_back(idx);
    }
    RelayoutRows();

    SetVisible(cfg.enabled);
}

void ChatOverlayController::SetVisible(bool visible) {
    if (canvas_) canvas_->get_gameObject()->SetActive(visible);
}

void ChatOverlayController::RepositionCanvasRelativeToHead() {
    auto* cam = Camera::get_main();
    if (!cam) return;

    Transform* head = cam->get_transform();
    Vector3 baseOffset = PresetBaseOffset(config_.preset);

    Vector3 localOffset(
        (config_.preset == PanelPreset::Custom ? config_.customX : baseOffset.x) + config_.horizontalOffset,
        (config_.preset == PanelPreset::Custom ? config_.customY : baseOffset.y) + config_.verticalOffset,
        config_.preset == PanelPreset::Custom ? config_.customZ : config_.distance
    );

    Vector3 targetPos = head->TransformPoint(localOffset);
    Quaternion targetRot = head->get_rotation();

    // Gentle follow rather than a rigid 1:1 head-lock -- a panel that's
    // perfectly welded to head rotation every frame reads as uncomfortably
    // "swimmy" in VR. A short lerp lets it settle into place instead.
    float t = 1.0f - std::exp(-Time::get_deltaTime() * 8.0f);
    get_transform()->set_position(Vector3::Lerp(get_transform()->get_position(), targetPos, t));
    get_transform()->set_rotation(Quaternion::Slerp(get_transform()->get_rotation(), targetRot, t));
}

void ChatOverlayController::Update() {
    if (!config_.enabled) return;

    RepositionCanvasRelativeToHead();

    std::vector<ChatMessage> batch;
    {
        std::lock_guard<std::mutex> lk(incomingMutex_);
        if (!incomingBuffer_.empty()) {
            batch.swap(incomingBuffer_);
        }
    }
    for (auto& msg : batch) {
        if (PassesFilter(msg)) {
            ApplyOneMessage(msg);
        }
    }
    if (!batch.empty()) {
        RelayoutRows();
    }

    // Expiry: only ever need to check the front (oldest) -- messages expire
    // in FIFO order, so once the front isn't expired, nothing behind it is
    // either. No per-frame scan of the whole active set.
    if (config_.messageDurationSeconds > 0.0f) {
        bool changed = false;
        uint64_t cutoffMillis = static_cast<uint64_t>(config_.messageDurationSeconds * 1000.0f);
        while (!activeOrder_.empty()) {
            size_t idx = activeOrder_.front();
            if (NowMillis() - rows_[idx].assignedAtMillis > cutoffMillis) {
                rows_[idx].SetVisible(false);
                freeList_.push_back(idx);
                activeOrder_.pop_front();
                changed = true;
            } else {
                break;
            }
        }
        if (changed) RelayoutRows();
    }
}

bool ChatOverlayController::PassesFilter(const ChatMessage& msg) const {
    switch (msg.type) {
        case ChatEventType::TextMessage: return config_.showRegularMessages;
        case ChatEventType::SuperChat: return config_.showSuperChats;
        case ChatEventType::SuperSticker: return config_.showSuperStickers;
        case ChatEventType::NewMember:
        case ChatEventType::MemberMilestone:
        case ChatEventType::MembershipGifting:
        case ChatEventType::GiftMembershipReceived:
            return config_.showMembershipEvents;
        case ChatEventType::MessageDeleted:
        case ChatEventType::MessageRetracted:
        case ChatEventType::UserBanned:
            return config_.showRegularMessages; // moderation notices ride along with regular chat visibility
        default:
            return false;
    }
}

void ChatOverlayController::RecycleOldestIfNeeded() {
    if ((int)activeOrder_.size() >= config_.maxVisibleMessages && !activeOrder_.empty()) {
        size_t idx = activeOrder_.front();
        activeOrder_.pop_front();
        rows_[idx].SetVisible(false);
        freeList_.push_back(idx);
    }
}

void ChatOverlayController::ApplyOneMessage(const ChatMessage& msg) {
    RecycleOldestIfNeeded();

    size_t idx;
    if (!freeList_.empty()) {
        idx = freeList_.back();
        freeList_.pop_back();
    } else if (!activeOrder_.empty()) {
        idx = activeOrder_.front();
        activeOrder_.pop_front();
    } else {
        return; // pool misconfigured to 0 -- shouldn't happen
    }

    auto& row = rows_[idx];
    row.assignedAtMillis = NowMillis();
    row.ApplyMessage(msg, config_.showUsernames, config_.showProfilePictures,
                      config_.highlightSuperChats, config_.highlightMemberships);

    if (config_.showProfilePictures && !msg.author.profileImageUrl.empty()) {
        avatarCache_->GetOrFetch(msg.author.profileImageUrl, [&row](UnityEngine::Sprite* sprite) {
            if (sprite && row.avatar) row.avatar->set_texture(sprite->get_texture());
        });
    }

    activeOrder_.push_back(idx);

    if (config_.notificationSounds &&
        (msg.type == ChatEventType::SuperChat || msg.type == ChatEventType::SuperSticker ||
         msg.type == ChatEventType::NewMember)) {
        PlayNotificationBeep();
    }
}

void ChatOverlayController::RelayoutRows() {
    constexpr float rowHeight = 74.0f; // UI units, matches MessageRow::Create's rowHeight + small gap
    float y = -4.0f;
    for (size_t idx : activeOrder_) {
        auto& row = rows_[idx];
        row.root->set_anchorMin(Vector2(0, 1));
        row.root->set_anchorMax(Vector2(1, 1));
        row.root->set_pivot(Vector2(0.5f, 1));
        row.root->set_anchoredPosition(Vector2(0, y));
        y -= rowHeight;
    }
}

void ChatOverlayController::PlayNotificationBeep() {
    if (!audioSource_) return;

    if (!beepClip_) {
        // Procedurally generate a short sine-wave beep so we don't need to
        // bundle/reference an external audio asset for this.
        const int sampleRate = 22050;
        const float durationSec = 0.12f;
        const int sampleCount = static_cast<int>(sampleRate * durationSec);
        beepClip_ = AudioClip::Create("YTLC_Beep", sampleCount, 1, sampleRate, false);

        std::vector<float> samples(sampleCount);
        for (int i = 0; i < sampleCount; i++) {
            float t = static_cast<float>(i) / sampleRate;
            float envelope = 1.0f - (static_cast<float>(i) / sampleCount); // simple linear fade-out, avoids a click
            samples[i] = std::sin(2.0f * 3.14159265f * 880.0f * t) * 0.25f * envelope;
        }
        // VERIFY: SetData's exact overload/array marshalling on your codegen
        // (ArrayW<float> vs a raw pointer + length) -- shown here in intent,
        // adjust to match the generated signature.
        beepClip_->SetData(samples.data(), samples.size(), 0);
    }

    audioSource_->PlayOneShot(beepClip_, 0.6f);
}

void ChatOverlayController::OnDestroy() {
    if (ModState::OverlayPtr() == this) {
        ModState::OverlayPtr() = nullptr;
    }
}

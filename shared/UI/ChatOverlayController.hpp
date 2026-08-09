#pragma once
#include "custom-types/shared/macros.hpp"
#include "custom-types/shared/types.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/RectTransform.hpp"
#include "UnityEngine/Canvas.hpp"
#include "UnityEngine/AudioSource.hpp"

#include <deque>
#include <vector>
#include <mutex>
#include <memory>

#include "Config.hpp"
#include "ChatTypes.hpp"
#include "MessageRow.hpp"
#include "AvatarCache.hpp"

// Deliberately NOT a BSML view -- BSML view controllers belong to Beat
// Saber's flow-coordinator/menu system and are not meant to render as a
// persistent HUD element during active gameplay. This is a plain
// DontDestroyOnLoad MonoBehaviour with its own world-space Canvas instead,
// which is the same pattern most Quest "HUD overlay" mods use (e.g. FPS
// counters, clocks) for exactly this reason.
DECLARE_CLASS_CODEGEN(YouTubeLiveChat::UI, ChatOverlayController, UnityEngine::MonoBehaviour,

public:
    DECLARE_INSTANCE_METHOD(void, ctor);
    DECLARE_INSTANCE_METHOD(void, Awake);
    DECLARE_INSTANCE_METHOD(void, Update);
    DECLARE_INSTANCE_METHOD(void, OnDestroy);

    void ApplyConfig(const YouTubeLiveChat::ModConfig& cfg);
    void SetVisible(bool visible);
    void PushMessages(const std::vector<YouTubeLiveChat::ChatMessage>& messages); // thread-safe-ish entry point; see .cpp

private:
    static constexpr size_t kMaxPoolSize = 50;

    UnityEngine::RectTransform* canvasRoot_ = nullptr;
    UnityEngine::Canvas* canvas_ = nullptr;
    UnityEngine::AudioSource* audioSource_ = nullptr;
    UnityEngine::AudioClip* beepClip_ = nullptr;

    std::vector<YouTubeLiveChat::UI::MessageRow> rows_;
    std::deque<size_t> activeOrder_;
    std::vector<size_t> freeList_;

    YouTubeLiveChat::ModConfig config_{};
    std::unique_ptr<YouTubeLiveChat::UI::AvatarCache> avatarCache_;

    std::mutex incomingMutex_;
    std::vector<YouTubeLiveChat::ChatMessage> incomingBuffer_; // filled by ChatManager's callback (main-thread, via coroutine), drained in Update

    void BuildCanvas();
    void BuildRowPool();
    void RepositionCanvasRelativeToHead();
    void RelayoutRows();
    void RecycleOldestIfNeeded();
    void ApplyOneMessage(const YouTubeLiveChat::ChatMessage& msg);
    bool PassesFilter(const YouTubeLiveChat::ChatMessage& msg) const;
    void PlayNotificationBeep();
)

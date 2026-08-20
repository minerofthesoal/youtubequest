#pragma once
#include "custom-types/shared/macros.hpp"
#include "custom-types/shared/types.hpp"

#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/CanvasGroup.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/AudioClip.hpp"
#include "UnityEngine/Object.hpp"  // SafePtr static_assert needs Object complete
#include "HMUI/CurvedTextMeshPro.hpp"
#include "bsml/shared/BSML/FloatingScreen/FloatingScreen.hpp"

#include "beatsaber-hook/shared/utils/typedefs-wrappers.hpp"

#include <deque>
#include <vector>
#include <memory>
#include <string>

#include "Config.hpp"
#include "ChatTypes.hpp"
#include "UI/MessageRow.hpp"
#include "UI/AvatarCache.hpp"

// The overlay is a BSML FloatingScreen rather than a BSML *view controller*:
// view controllers belong to Beat Saber's menu flow-coordinator system and are
// torn down when you start a song, which is precisely when you still want to
// see chat. A FloatingScreen is a standalone HMUI::Screen with its own curved
// canvas, so marking it DontDestroyOnLoad keeps one instance alive across the
// menu -> gameplay -> results scene transitions and it renders in all of them.
//
// This object also owns the coroutine host used by ChatManager and
// AvatarCache: as a DontDestroyOnLoad MonoBehaviour it is the one component
// guaranteed to survive a scene change, so coroutines started on it are not
// cancelled halfway through a poll when a level loads.
DECLARE_CLASS_CODEGEN(YouTubeLiveChat::UI, ChatOverlayController, UnityEngine::MonoBehaviour) {
   public:
    DECLARE_DEFAULT_CTOR();

    // Awake/Update/OnDestroy are not virtual -- Unity looks them up by name on
    // the concrete type -- so plain INSTANCE_METHOD is right here.
    DECLARE_INSTANCE_METHOD(void, Awake);
    DECLARE_INSTANCE_METHOD(void, Update);
    DECLARE_INSTANCE_METHOD(void, OnDestroy);

   public:
    void ApplyConfig(YouTubeLiveChat::ModConfig const& cfg);
    void SetVisible(bool visible);

    // Called from ChatManager's callbacks (always on the main thread, because
    // they are driven by Unity coroutines).
    void PushMessages(std::vector<YouTubeLiveChat::ChatMessage> const& messages);
    void PushSystemMessage(std::string const& text);
    void SetStatus(std::string const& text);
    void SetSignInPrompt(std::string const& userCode, std::string const& verificationUrl);

    void ClearMessages();

   private:
    static constexpr size_t kMaxPoolSize = 30;
    static constexpr float kScreenUnitsPerMeter = 50.0f;
    static constexpr float kRowHeight = 9.0f;
    static constexpr float kHeaderHeight = 13.0f;

    SafePtrUnity<BSML::FloatingScreen> screen_;
    SafePtrUnity<UnityEngine::CanvasGroup> canvasGroup_;
    SafePtrUnity<HMUI::CurvedTextMeshPro> statusText_;
    SafePtrUnity<HMUI::CurvedTextMeshPro> promptText_;
    SafePtrUnity<UnityEngine::AudioSource> audioSource_;
    SafePtrUnity<UnityEngine::AudioClip> beepClip_;

    std::vector<MessageRow> rows_;
    std::deque<size_t> activeOrder_;
    std::vector<size_t> freeList_;

    YouTubeLiveChat::ModConfig config_{};
    std::unique_ptr<AvatarCache> avatarCache_;

    std::vector<YouTubeLiveChat::ChatMessage> incoming_;
    std::string statusLine_;
    std::string promptLine_;

    bool built_ = false;
    float placementSaveTimer_ = 0.0f;
    bool placementDirty_ = false;

    void Build();
    void BuildRowPool();
    void ApplyPlacement(bool immediate);
    void TrackFreePlacement();
    void RelayoutRows();
    void RecycleOldest();
    void ApplyOneMessage(YouTubeLiveChat::ChatMessage const& msg);
    bool PassesFilter(YouTubeLiveChat::ChatMessage const& msg) const;
    void ExpireOldMessages();
    void RefreshHeaderText();
    void PlayNotificationBeep();
    float ScreenWidthUnits() const;
    float ScreenHeightUnits() const;
};

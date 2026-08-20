#pragma once
#include "UnityEngine/RectTransform.hpp"
#include "UnityEngine/UI/RawImage.hpp"
#include "HMUI/CurvedTextMeshPro.hpp"

#include "ChatTypes.hpp"
#include "Config.hpp"

namespace YouTubeLiveChat::UI {

// A single row in the pool. Rows are created once and reused for the whole
// session -- we only ever mutate .text / .color / .texture / anchoredPosition,
// never Instantiate/Destroy per message. That is the main lever for keeping
// this overlay cheap on a Quest: a busy stream produces several messages a
// second, and building a fresh GameObject hierarchy for each one would mean a
// steady stream of allocations and layout rebuilds during active gameplay.
//
// The text objects are built through BSML-Lite rather than by adding a bare
// CurvedTextMeshPro component: BSML clones Beat Saber's own text prefab, so
// the row comes out with a real font asset and the curved-canvas material
// already assigned. A hand-rolled CurvedTextMeshPro has neither, which is why
// it renders as nothing at all.
struct MessageRow {
    UnityEngine::RectTransform* root = nullptr;
    UnityEngine::UI::RawImage* avatar = nullptr;
    HMUI::CurvedTextMeshPro* text = nullptr;

    bool active = false;
    uint64_t assignedAtMillis = 0;
    ChatEventType type = ChatEventType::Unsupported;

    void SetVisible(bool visible);
    void SetAvatarVisible(bool visible);
    void ApplyMessage(const ChatMessage& msg, const ModConfig& cfg);

    static MessageRow Create(UnityEngine::Transform* parent, float width, float rowHeight);
};

}  // namespace YouTubeLiveChat::UI

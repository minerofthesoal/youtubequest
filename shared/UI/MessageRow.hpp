#pragma once
#include "UnityEngine/RectTransform.hpp"
#include "UnityEngine/UI/Image.hpp"
#include "UnityEngine/UI/RawImage.hpp"
#include "HMUI/CurvedTextMeshPro.hpp"

#include "ChatTypes.hpp"

namespace YouTubeLiveChat::UI {

// A single row in the pool. Rows are created once (up to maxVisibleMessages)
// and reused for the whole session -- we only ever mutate .text / .color /
// .sprite / anchoredPosition on them, never Instantiate/Destroy per message.
// That's the main lever for keeping this overlay cheap on a Quest 3S: a
// chat-heavy stream can produce several messages/second, and allocating a
// fresh GameObject hierarchy for each one would be a steady stream of GC
// pressure and layout rebuilds during active gameplay.
struct MessageRow {
    UnityEngine::RectTransform* root = nullptr;
    UnityEngine::UI::Image* background = nullptr;
    UnityEngine::UI::RawImage* avatar = nullptr;
    HMUI::CurvedTextMeshPro* authorText = nullptr;
    HMUI::CurvedTextMeshPro* bodyText = nullptr;
    HMUI::CurvedTextMeshPro* badgeText = nullptr; // small icon-as-text, e.g. an emoji glyph

    bool active = false;
    uint64_t assignedAtMillis = 0;
    ChatEventType type = ChatEventType::Unsupported;

    void SetVisible(bool visible);
    void ApplyMessage(const ChatMessage& msg, bool showUsername, bool showAvatar,
                       bool highlightSuperChat, bool highlightMembership);

    static MessageRow Create(UnityEngine::Transform* parent, float width, float rowHeight);
};

}

#include "MessageRow.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Vector2.hpp"
#include "UnityEngine/Vector3.hpp"
#include "UnityEngine/Color.hpp"
#include "TMPro/FontStyles.hpp"
#include "TMPro/TextAlignmentOptions.hpp"
#include "custom-types/shared/macros.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"

using namespace UnityEngine;
using namespace UnityEngine::UI;

namespace YouTubeLiveChat::UI {

namespace {
    // Approximates YouTube's own Super Chat tier color bands. Real tier
    // boundaries are broadcaster-currency-dependent server-side; we only
    // have `tier` (1..N ascending) from the API, so this is a "close
    // enough for a VR overlay" mapping rather than an exact reproduction
    // of YouTube's web palette.
    Color SuperChatTierColor(int32_t tier) {
        switch (tier) {
            case 1: return Color(0.11f, 0.71f, 0.90f, 0.92f); // light blue
            case 2: return Color(0.02f, 0.62f, 0.83f, 0.92f); // blue
            case 3: return Color(0.11f, 0.72f, 0.53f, 0.92f); // teal/green
            case 4: return Color(0.99f, 0.78f, 0.14f, 0.92f); // yellow
            case 5: return Color(0.98f, 0.55f, 0.09f, 0.92f); // orange
            case 6: return Color(0.90f, 0.22f, 0.21f, 0.92f); // red
            default: return Color(0.85f, 0.16f, 0.55f, 0.92f); // magenta (highest tiers)
        }
    }
}

MessageRow MessageRow::Create(Transform* parent, float width, float rowHeight) {
    MessageRow row{};

    auto* rootGo = GameObject::New_ctor("YTLC_MessageRow");
    row.root = rootGo->AddComponent<RectTransform*>();
    row.root->SetParent(parent, false);
    row.root->set_sizeDelta(Vector2(width, rowHeight));

    row.background = rootGo->AddComponent<Image*>();
    row.background->set_color(Color(0, 0, 0, 0.55f));

    // Avatar (left-aligned square)
    auto* avatarGo = GameObject::New_ctor("Avatar");
    auto* avatarRt = avatarGo->AddComponent<RectTransform*>();
    avatarRt->SetParent(row.root, false);
    avatarRt->set_anchorMin(Vector2(0, 1));
    avatarRt->set_anchorMax(Vector2(0, 1));
    avatarRt->set_pivot(Vector2(0, 1));
    avatarRt->set_anchoredPosition(Vector2(2, -2));
    avatarRt->set_sizeDelta(Vector2(rowHeight - 4, rowHeight - 4));
    row.avatar = avatarGo->AddComponent<RawImage*>();

    // Author name
    auto* authorGo = GameObject::New_ctor("Author");
    auto* authorRt = authorGo->AddComponent<RectTransform*>();
    authorRt->SetParent(row.root, false);
    authorRt->set_anchorMin(Vector2(0, 1));
    authorRt->set_anchorMax(Vector2(1, 1));
    authorRt->set_pivot(Vector2(0, 1));
    authorRt->set_anchoredPosition(Vector2(rowHeight + 2, -2));
    authorRt->set_sizeDelta(Vector2(-(rowHeight + 4), 16));
    row.authorText = authorGo->AddComponent<HMUI::CurvedTextMeshPro*>();
    row.authorText->set_fontSize(3.0f);
    row.authorText->set_fontStyle(TMPro::FontStyles::Bold);

    // Message body (wraps)
    auto* bodyGo = GameObject::New_ctor("Body");
    auto* bodyRt = bodyGo->AddComponent<RectTransform*>();
    bodyRt->SetParent(row.root, false);
    bodyRt->set_anchorMin(Vector2(0, 0));
    bodyRt->set_anchorMax(Vector2(1, 1));
    bodyRt->set_pivot(Vector2(0, 1));
    bodyRt->set_anchoredPosition(Vector2(rowHeight + 2, -18));
    bodyRt->set_sizeDelta(Vector2(-(rowHeight + 4), -20));
    row.bodyText = bodyGo->AddComponent<HMUI::CurvedTextMeshPro*>();
    row.bodyText->set_fontSize(3.2f);
    row.bodyText->set_enableWordWrapping(true);

    // Type badge (small glyph, top-right corner)
    auto* badgeGo = GameObject::New_ctor("Badge");
    auto* badgeRt = badgeGo->AddComponent<RectTransform*>();
    badgeRt->SetParent(row.root, false);
    badgeRt->set_anchorMin(Vector2(1, 1));
    badgeRt->set_anchorMax(Vector2(1, 1));
    badgeRt->set_pivot(Vector2(1, 1));
    badgeRt->set_anchoredPosition(Vector2(-2, -2));
    badgeRt->set_sizeDelta(Vector2(20, 16));
    row.badgeText = badgeGo->AddComponent<HMUI::CurvedTextMeshPro*>();
    row.badgeText->set_fontSize(3.0f);
    row.badgeText->set_alignment(TMPro::TextAlignmentOptions::TopRight);

    row.SetVisible(false);
    return row;
}

void MessageRow::SetVisible(bool visible) {
    active = visible;
    if (root) root->get_gameObject()->SetActive(visible);
}

void MessageRow::ApplyMessage(const ChatMessage& msg, bool showUsername, bool showAvatar,
                                bool highlightSuperChat, bool highlightMembership) {
    type = msg.type;

    if (authorText) {
        authorText->SetText(showUsername ? il2cpp_utils::newcsstr(msg.author.displayName) : il2cpp_utils::newcsstr(""));
    }

    std::string bodyStr = msg.displayText;
    std::string badge;
    Color bg(0, 0, 0, 0.55f);
    Color bodyColor(1, 1, 1, 1);

    switch (msg.type) {
        case ChatEventType::TextMessage:
            badge = "";
            break;
        case ChatEventType::SuperChat:
            badge = "\U0001F4B0"; // 💰
            bodyStr = (msg.amountDisplayString.empty() ? "" : (msg.amountDisplayString + "  ")) + bodyStr;
            if (highlightSuperChat) bg = SuperChatTierColor(msg.tier);
            break;
        case ChatEventType::SuperSticker:
            badge = "\U0001F389"; // 🎉
            bodyStr = (msg.amountDisplayString.empty() ? "" : (msg.amountDisplayString + "  ")) + bodyStr;
            if (highlightSuperChat) bg = SuperChatTierColor(msg.tier);
            break;
        case ChatEventType::NewMember:
        case ChatEventType::MemberMilestone:
        case ChatEventType::MembershipGifting:
        case ChatEventType::GiftMembershipReceived:
            badge = "\U0001F451"; // 👑
            if (highlightMembership) bg = Color(0.14f, 0.55f, 0.20f, 0.85f);
            break;
        case ChatEventType::MessageDeleted:
        case ChatEventType::MessageRetracted:
            bodyColor = Color(0.6f, 0.6f, 0.6f, 1.0f);
            break;
        case ChatEventType::UserBanned:
            bodyColor = Color(0.85f, 0.4f, 0.4f, 1.0f);
            break;
        default:
            break;
    }

    if (badgeText) badgeText->SetText(il2cpp_utils::newcsstr(badge));
    if (bodyText) {
        bodyText->SetText(il2cpp_utils::newcsstr(bodyStr));
        bodyText->set_color(bodyColor);
    }
    if (background) background->set_color(bg);
    if (avatar) avatar->get_gameObject()->SetActive(showAvatar);

    SetVisible(true);
}

}

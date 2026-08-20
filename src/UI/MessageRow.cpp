#include "UI/MessageRow.hpp"

#include "bsml/shared/BSML-Lite.hpp"

#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Vector2.hpp"
#include "UnityEngine/Color.hpp"
#include "UnityEngine/Texture2D.hpp"
#include "TMPro/TextAlignmentOptions.hpp"
#include "TMPro/TextOverflowModes.hpp"

#include <chrono>

using namespace UnityEngine;

namespace {

// Approximates YouTube's own Super Chat tier colors. The real tier boundaries
// are broadcaster-currency dependent server-side and the API only gives us
// `tier` (1..N ascending), so this is "close enough to read at a glance in
// VR" rather than an exact reproduction of the web palette.
const char* SuperChatTierColor(int32_t tier) {
    switch (tier) {
        case 1: return "#1CB5E0";
        case 2: return "#0A9ED4";
        case 3: return "#1CB888";
        case 4: return "#FDC824";
        case 5: return "#FA8C17";
        case 6: return "#E63935";
        default: return "#D92A8C";
    }
}

const char* AuthorColor(const YouTubeLiveChat::ChatAuthor& author) {
    if (author.isChatOwner) return "#FFD24D";      // broadcaster
    if (author.isChatModerator) return "#5E84F1";  // moderator
    if (author.isChatSponsor) return "#2BA640";    // member
    return "#9FD6FF";
}

// TMP treats < ... > as rich-text markup, so a chat message containing "<3"
// or "<script>" would either vanish or corrupt the row. Wrapping the body in
// <noparse> turns all of it back into literal text; the only thing that can
// break out of that is a literal closing tag, so strip those first.
std::string NoParse(std::string body) {
    const std::string close = "</noparse>";
    for (size_t pos = body.find(close); pos != std::string::npos; pos = body.find(close, pos)) {
        body.erase(pos, close.size());
    }
    return "<noparse>" + body + "</noparse>";
}

uint64_t NowMillis() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

}  // namespace

namespace YouTubeLiveChat::UI {

MessageRow MessageRow::Create(Transform* parent, float width, float rowHeight) {
    MessageRow row{};

    auto* rootGo = GameObject::New_ctor(StringW("YTLC_MessageRow"));
    row.root = rootGo->AddComponent<RectTransform*>();
    row.root->SetParent(parent, false);
    row.root->set_anchorMin(Vector2(0.0f, 1.0f));
    row.root->set_anchorMax(Vector2(1.0f, 1.0f));
    row.root->set_pivot(Vector2(0.5f, 1.0f));
    row.root->set_sizeDelta(Vector2(0.0f, rowHeight));

    const float avatarSize = rowHeight - 1.0f;

    row.avatar = BSML::Lite::CreateRawImage(row.root, nullptr,
                                            Vector2(-(width / 2.0f) + (avatarSize / 2.0f) + 1.0f, 0.0f),
                                            Vector2(avatarSize, avatarSize));

    const float textLeft = avatarSize + 2.0f;
    row.text = BSML::Lite::CreateText(row.root, StringW(""), 3.0f,
                                      Vector2(textLeft / 2.0f, 0.0f),
                                      Vector2(width - textLeft, rowHeight));
    row.text->set_alignment(TMPro::TextAlignmentOptions::MidlineLeft);
    row.text->set_enableWordWrapping(true);
    row.text->set_richText(true);
    row.text->set_overflowMode(TMPro::TextOverflowModes::Truncate);

    row.SetVisible(false);
    return row;
}

void MessageRow::SetVisible(bool visible) {
    active = visible;
    if (root) root->get_gameObject()->SetActive(visible);
}

void MessageRow::SetAvatarVisible(bool visible) {
    if (avatar) avatar->get_gameObject()->SetActive(visible);
}

void MessageRow::ApplyMessage(const ChatMessage& msg, const ModConfig& cfg) {
    type = msg.type;
    assignedAtMillis = NowMillis();

    std::string badge;
    std::string bodyColor = "#FFFFFF";
    std::string body = msg.displayText;

    switch (msg.type) {
        case ChatEventType::SuperChat:
        case ChatEventType::SuperSticker:
            badge = msg.amountDisplayString;
            if (cfg.highlightSuperChats) bodyColor = SuperChatTierColor(msg.tier);
            break;
        case ChatEventType::NewMember:
        case ChatEventType::MemberMilestone:
        case ChatEventType::MembershipGifting:
        case ChatEventType::GiftMembershipReceived:
            if (cfg.highlightMemberships) bodyColor = "#5BE07E";
            break;
        case ChatEventType::MessageDeleted:
        case ChatEventType::MessageRetracted:
            bodyColor = "#999999";
            break;
        case ChatEventType::UserBanned:
            bodyColor = "#D96666";
            break;
        case ChatEventType::System:
            bodyColor = "#C0C0C0";
            break;
        default:
            break;
    }

    std::string line;
    if (msg.type != ChatEventType::System && cfg.showUsernames && !msg.author.displayName.empty()) {
        line += "<color=" + std::string(AuthorColor(msg.author)) + "><b>" +
                NoParse(msg.author.displayName) + "</b></color>  ";
    }
    if (!badge.empty()) {
        line += "<color=" + std::string(SuperChatTierColor(msg.tier)) + "><b>" +
                NoParse(badge) + "</b></color>  ";
    }
    line += "<color=" + bodyColor + ">" + NoParse(body) + "</color>";

    if (text) text->set_text(StringW(line));

    const bool wantAvatar = cfg.showProfilePictures && msg.type != ChatEventType::System &&
                            !msg.author.profileImageUrl.empty();
    SetAvatarVisible(wantAvatar);
    if (avatar && !wantAvatar) {
        avatar->set_texture(nullptr);
    }

    SetVisible(true);
}

}  // namespace YouTubeLiveChat::UI

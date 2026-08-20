#include "ModState.hpp"
#include "UI/ChatOverlayController.hpp"
#include "UI/SettingsViewController.hpp"
#include "Logging.hpp"

namespace YouTubeLiveChat {

ModConfig& ModState::Config() {
    static ModConfig instance = ModConfig::Load();
    return instance;
}

ChatManager& ModState::Manager() {
    static ChatManager instance;
    return instance;
}

UI::ChatOverlayController*& ModState::OverlayPtr() {
    static UI::ChatOverlayController* instance = nullptr;
    return instance;
}

UI::SettingsViewController*& ModState::SettingsPtr() {
    static UI::SettingsViewController* instance = nullptr;
    return instance;
}

void ModState::InstallManagerCallbacks() {
    auto& manager = Manager();

    manager.OnMessagesReceived([](const std::vector<ChatMessage>& messages) {
        if (OverlayPtr()) OverlayPtr()->PushMessages(messages);
    });

    manager.OnStateChanged([](ConnectionState state, std::string detail) {
        std::string line = ToString(state);
        if (!detail.empty()) line += " - " + detail;
        Log().debug("Connection state: {}", line);
        if (OverlayPtr()) OverlayPtr()->SetStatus(line);
        if (SettingsPtr()) SettingsPtr()->SetStatus(line);
    });

    manager.OnSignInPrompt([](std::string userCode, std::string verificationUrl) {
        if (OverlayPtr()) OverlayPtr()->SetSignInPrompt(userCode, verificationUrl);
        if (SettingsPtr()) SettingsPtr()->SetSignInPrompt(userCode, verificationUrl);
    });

    manager.OnSignInComplete([](bool ok, std::string refreshToken, std::string error) {
        ModConfig cfg = Config();
        if (ok) {
            if (!refreshToken.empty()) cfg.oauthRefreshToken = refreshToken;
        } else if (error.empty()) {
            // An empty error means an explicit sign-out rather than a failure.
            cfg.oauthRefreshToken.clear();
        }
        ApplyConfig(cfg);
        if (SettingsPtr()) SettingsPtr()->RefreshFromConfig();
    });
}

void ModState::ApplyConfig(const ModConfig& config) {
    Config() = config;
    Config().Save();
    Manager().Configure(Config());
    if (OverlayPtr()) {
        OverlayPtr()->ApplyConfig(Config());
    }
}

}  // namespace YouTubeLiveChat

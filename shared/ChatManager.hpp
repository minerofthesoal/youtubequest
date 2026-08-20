#pragma once
#include <string>
#include <functional>
#include <vector>
#include <cstdint>

#include "custom-types/shared/coroutine.hpp"
#include "UnityEngine/MonoBehaviour.hpp"

#include "ChatTypes.hpp"
#include "Config.hpp"
#include "YouTubeApiClient.hpp"
#include "YouTubeAuth.hpp"

namespace YouTubeLiveChat {

// Orchestrates: resolve broadcast -> wait for live -> poll chat -> handle
// errors/backoff -> (optionally) auto-reconnect, plus the interactive
// sign-in flow.
//
// Deliberately NOT a MonoBehaviour itself -- it just needs *a* MonoBehaviour
// to host its coroutines on Unity's scheduler, which the overlay controller
// provides. That keeps the networking state machine independent of any
// particular UI object's lifetime.
class ChatManager {
public:
    using StateCallback = std::function<void(ConnectionState, std::string statusDetail)>;
    using MessagesCallback = std::function<void(const std::vector<ChatMessage>&)>;
    // Called during the device flow with the code the user has to enter, and
    // again with an empty code when the flow finishes (successfully or not).
    using SignInPromptCallback = std::function<void(std::string userCode, std::string verificationUrl)>;
    // Fired when a sign-in completes, so the caller can persist the new
    // refresh token.
    using SignInCompleteCallback = std::function<void(bool ok, std::string refreshToken, std::string error)>;

    void Configure(const ModConfig& config);
    void SetHost(UnityEngine::MonoBehaviour* host) { host_ = host; }

    void OnStateChanged(StateCallback cb) { stateCallback_ = std::move(cb); }
    void OnMessagesReceived(MessagesCallback cb) { messagesCallback_ = std::move(cb); }
    void OnSignInPrompt(SignInPromptCallback cb) { signInPromptCallback_ = std::move(cb); }
    void OnSignInComplete(SignInCompleteCallback cb) { signInCompleteCallback_ = std::move(cb); }

    // Starts (or restarts) the connect->poll loop.
    void Connect();
    void Disconnect();

    // Kicks off the OAuth device flow. Safe to call twice; the second call is
    // ignored while one is already running.
    void BeginSignIn();
    void SignOut();
    bool SignInInProgress() const { return signingIn_; }

    ConnectionState State() const { return state_; }
    const std::string& StatusDetail() const { return statusDetail_; }
    YouTubeAuth& Auth() { return auth_; }

private:
    UnityEngine::MonoBehaviour* host_ = nullptr;
    ModConfig config_{};
    YouTubeApiClient apiClient_;
    YouTubeAuth auth_;

    ConnectionState state_ = ConnectionState::Disconnected;
    std::string statusDetail_;
    StateCallback stateCallback_;
    MessagesCallback messagesCallback_;
    SignInPromptCallback signInPromptCallback_;
    SignInCompleteCallback signInCompleteCallback_;

    bool wantsConnection_ = false;
    bool signingIn_ = false;
    uint32_t generation_ = 0;  // bumped on Connect()/Disconnect() to invalidate stale coroutines

    void SetState(ConnectionState s, std::string detail = "");
    bool StartCoroutine(custom_types::Helpers::Coroutine coroutine);

    custom_types::Helpers::Coroutine RunLoop(uint32_t myGeneration);
    custom_types::Helpers::Coroutine RunSignIn();
    static custom_types::Helpers::Coroutine WaitSeconds(float seconds);
};

}  // namespace YouTubeLiveChat

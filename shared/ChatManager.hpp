#pragma once
#include <string>
#include <functional>
#include <memory>

#include "custom-types/shared/coroutine.hpp"
#include "UnityEngine/MonoBehaviour.hpp"

#include "ChatTypes.hpp"
#include "Config.hpp"
#include "YouTubeApiClient.hpp"

namespace YouTubeLiveChat {

// Orchestrates: resolve video -> wait for live -> poll chat -> handle
// errors/backoff -> (optionally) auto-reconnect. Deliberately NOT a
// custom-types/MonoBehaviour itself -- it just needs *a* MonoBehaviour to
// host its coroutines on Unity's scheduler, which the caller (the overlay
// controller) provides. This keeps the networking/state-machine logic
// testable and decoupled from any particular UI lifetime.
class ChatManager {
public:
    using StateCallback = std::function<void(ConnectionState, std::string statusDetail)>;
    using MessagesCallback = std::function<void(const std::vector<ChatMessage>&)>;

    ChatManager() = default;

    void Configure(const ModConfig& config);
    void SetHost(UnityEngine::MonoBehaviour* host) { host_ = host; }

    void OnStateChanged(StateCallback cb) { stateCallback_ = std::move(cb); }
    void OnMessagesReceived(MessagesCallback cb) { messagesCallback_ = std::move(cb); }

    // Starts (or restarts) the connect->poll loop for config_.videoIdOrUrl.
    void Connect();
    void Disconnect();

    ConnectionState State() const { return state_; }

private:
    UnityEngine::MonoBehaviour* host_ = nullptr;
    ModConfig config_{};
    std::unique_ptr<YouTubeApiClient> apiClient_;

    ConnectionState state_ = ConnectionState::Disconnected;
    StateCallback stateCallback_;
    MessagesCallback messagesCallback_;

    bool wantsConnection_ = false;
    uint32_t generation_ = 0; // bumped on every Connect()/Disconnect() to invalidate stale coroutines

    void SetState(ConnectionState s, std::string detail = "");

    custom_types::Helpers::Coroutine RunLoop(uint32_t myGeneration);
    custom_types::Helpers::Coroutine WaitSeconds(float seconds);
};

}

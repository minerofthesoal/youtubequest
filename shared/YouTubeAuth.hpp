#pragma once
#include <string>
#include <functional>
#include <cstdint>

#include "custom-types/shared/coroutine.hpp"
#include "Config.hpp"

// ---------------------------------------------------------------------------
// OAuth 2.0 for TV and Limited-Input Devices ("device flow"), which is the
// flow Google documents for exactly this situation: a device with no usable
// browser and no keyboard. Nothing is typed in the headset --
//
//   1. The mod POSTs to /device/code and gets back a short user_code.
//   2. The headset shows that code plus https://google.com/device.
//   3. You approve on your phone/PC.
//   4. The mod, which has been polling /token, receives an access token and a
//      refresh token. Only the refresh token is persisted.
//
// Why bother when an API key can already read public chat: an API key is
// anonymous, so it cannot read your own members-only chat and cannot ask
// "which broadcast is this account streaming right now" -- with OAuth the mod
// finds your live chat by itself and you never paste a video ID.
//
// You supply your own OAuth client (Google Cloud console -> Credentials ->
// OAuth client ID -> "TVs and Limited Input devices"). See the README.
// ---------------------------------------------------------------------------

namespace YouTubeLiveChat {

struct DeviceCodeResult {
    bool ok = false;
    std::string deviceCode;
    std::string userCode;
    std::string verificationUrl;
    int32_t intervalSeconds = 5;
    int32_t expiresInSeconds = 1800;
    std::string error;
};

struct TokenResult {
    bool ok = false;
    bool pending = false;      // user has not approved yet; keep polling
    bool slowDown = false;     // back off, we polled too fast
    bool denied = false;       // user refused, or the code expired
    std::string accessToken;
    std::string refreshToken;  // only present on the first grant
    int32_t expiresInSeconds = 3600;
    std::string error;
};

class YouTubeAuth {
public:
    void Configure(const ModConfig& config);

    bool HasRefreshToken() const { return !refreshToken_.empty(); }
    bool HasUsableCredentials() const { return !clientId_.empty() && !clientSecret_.empty(); }

    // Current access token if one is cached and not about to expire, else "".
    const std::string& CachedAccessToken() const { return accessToken_; }
    bool AccessTokenIsFresh() const;

    void SetRefreshToken(std::string token) { refreshToken_ = std::move(token); accessToken_.clear(); }
    void Clear();

    // Step 1 of the device flow.
    custom_types::Helpers::Coroutine RequestDeviceCode(std::function<void(DeviceCodeResult)> callback);

    // One /token poll for a device code. Callers drive the retry cadence so
    // they can honor `interval` / slow_down without this class owning a timer.
    custom_types::Helpers::Coroutine PollDeviceToken(std::string deviceCode,
                                                     std::function<void(TokenResult)> callback);

    // Exchanges the stored refresh token for a fresh access token. Callback
    // gets ok=false (and denied=true) if the grant was revoked, which means
    // the user has to sign in again.
    custom_types::Helpers::Coroutine RefreshAccessToken(std::function<void(TokenResult)> callback);

    // Ensures CachedAccessToken() is usable, refreshing only if needed.
    custom_types::Helpers::Coroutine EnsureAccessToken(std::function<void(bool ok, std::string error)> callback);

private:
    std::string clientId_;
    std::string clientSecret_;
    std::string refreshToken_;
    std::string accessToken_;
    int64_t accessTokenExpiryUnix_ = 0;

    void ApplyToken(const TokenResult& token);
};

}  // namespace YouTubeLiveChat

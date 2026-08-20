#include "YouTubeAuth.hpp"
#include "Http.hpp"
#include "UrlUtils.hpp"
#include "Logging.hpp"

#include "rapidjson/document.h"

#include <chrono>

namespace {

constexpr const char* kDeviceCodeUrl = "https://oauth2.googleapis.com/device/code";
constexpr const char* kTokenUrl = "https://oauth2.googleapis.com/token";
// Read-only is all this mod ever needs: it lists your active broadcast and
// reads live chat. It never posts, deletes, or modifies anything.
constexpr const char* kScope = "https://www.googleapis.com/auth/youtube.readonly";

// Refresh a little before the real expiry so a poll never races the boundary.
constexpr int64_t kExpirySlackSeconds = 60;

int64_t NowUnix() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string GetStr(const rapidjson::Value& v, const char* key, const std::string& def = "") {
    if (v.HasMember(key) && v[key].IsString()) return v[key].GetString();
    return def;
}

int32_t GetInt(const rapidjson::Value& v, const char* key, int32_t def) {
    if (v.HasMember(key) && v[key].IsInt()) return v[key].GetInt();
    return def;
}

}  // namespace

namespace YouTubeLiveChat {

void YouTubeAuth::Configure(const ModConfig& config) {
    // Changing the client invalidates any token minted by the previous one.
    if (clientId_ != config.oauthClientId || clientSecret_ != config.oauthClientSecret) {
        accessToken_.clear();
        accessTokenExpiryUnix_ = 0;
    }
    clientId_ = config.oauthClientId;
    clientSecret_ = config.oauthClientSecret;
    if (refreshToken_ != config.oauthRefreshToken) {
        refreshToken_ = config.oauthRefreshToken;
        accessToken_.clear();
        accessTokenExpiryUnix_ = 0;
    }
}

void YouTubeAuth::Clear() {
    refreshToken_.clear();
    accessToken_.clear();
    accessTokenExpiryUnix_ = 0;
}

bool YouTubeAuth::AccessTokenIsFresh() const {
    return !accessToken_.empty() && NowUnix() + kExpirySlackSeconds < accessTokenExpiryUnix_;
}

void YouTubeAuth::ApplyToken(const TokenResult& token) {
    if (!token.ok) return;
    accessToken_ = token.accessToken;
    accessTokenExpiryUnix_ = NowUnix() + token.expiresInSeconds;
    if (!token.refreshToken.empty()) {
        refreshToken_ = token.refreshToken;
    }
}

custom_types::Helpers::Coroutine YouTubeAuth::RequestDeviceCode(
    std::function<void(DeviceCodeResult)> callback) {
    if (!HasUsableCredentials()) {
        DeviceCodeResult r{};
        r.error = "Set an OAuth client ID and secret in settings first.";
        callback(r);
        co_return;
    }

    std::string body = "client_id=" + UrlUtils::UrlEncode(clientId_) +
                       "&scope=" + UrlUtils::UrlEncode(kScope);

    co_yield custom_types::Helpers::CoroutineHelper::New(
        Http::PostForm(kDeviceCodeUrl, body, [callback](Http::Response response) {
            DeviceCodeResult result{};

            if (!response.completed) {
                result.error = response.error.empty() ? "Network error" : response.error;
                callback(result);
                return;
            }

            rapidjson::Document doc;
            if (doc.Parse(response.body.c_str()).HasParseError() || !doc.IsObject()) {
                result.error = "Malformed response from Google";
                callback(result);
                return;
            }

            if (!response.ok) {
                std::string err = GetStr(doc, "error", "unknown_error");
                std::string desc = GetStr(doc, "error_description");
                result.error = desc.empty() ? err : (err + ": " + desc);
                if (err == "invalid_client") {
                    result.error = "That OAuth client was rejected. Check the ID/secret, and that "
                                   "the client type is \"TVs and Limited Input devices\".";
                }
                callback(result);
                return;
            }

            result.deviceCode = GetStr(doc, "device_code");
            result.userCode = GetStr(doc, "user_code");
            // Google returns verification_url on the device endpoint; the spec
            // calls it verification_uri, so accept either.
            result.verificationUrl = GetStr(doc, "verification_url", GetStr(doc, "verification_uri", "google.com/device"));
            result.intervalSeconds = GetInt(doc, "interval", 5);
            result.expiresInSeconds = GetInt(doc, "expires_in", 1800);
            result.ok = !result.deviceCode.empty() && !result.userCode.empty();
            if (!result.ok) result.error = "Google did not return a device code";
            callback(result);
        }));
    co_return;
}

custom_types::Helpers::Coroutine YouTubeAuth::PollDeviceToken(
    std::string deviceCode, std::function<void(TokenResult)> callback) {
    std::string body = "client_id=" + UrlUtils::UrlEncode(clientId_) +
                       "&client_secret=" + UrlUtils::UrlEncode(clientSecret_) +
                       "&device_code=" + UrlUtils::UrlEncode(deviceCode) +
                       "&grant_type=" + UrlUtils::UrlEncode("urn:ietf:params:oauth:grant-type:device_code");

    auto* self = this;
    co_yield custom_types::Helpers::CoroutineHelper::New(
        Http::PostForm(kTokenUrl, body, [self, callback](Http::Response response) {
            TokenResult result{};

            if (!response.completed) {
                result.error = response.error.empty() ? "Network error" : response.error;
                callback(result);
                return;
            }

            rapidjson::Document doc;
            if (doc.Parse(response.body.c_str()).HasParseError() || !doc.IsObject()) {
                result.error = "Malformed token response";
                callback(result);
                return;
            }

            if (!response.ok) {
                std::string err = GetStr(doc, "error");
                if (err == "authorization_pending") {
                    result.pending = true;
                } else if (err == "slow_down") {
                    result.pending = true;
                    result.slowDown = true;
                } else if (err == "access_denied" || err == "expired_token") {
                    result.denied = true;
                    result.error = (err == "expired_token")
                                       ? "The sign-in code expired. Start again."
                                       : "Sign-in was denied.";
                } else {
                    result.error = GetStr(doc, "error_description", err.empty() ? "Token request failed" : err);
                }
                callback(result);
                return;
            }

            result.accessToken = GetStr(doc, "access_token");
            result.refreshToken = GetStr(doc, "refresh_token");
            result.expiresInSeconds = GetInt(doc, "expires_in", 3600);
            result.ok = !result.accessToken.empty();
            if (!result.ok) result.error = "No access token in response";
            self->ApplyToken(result);
            callback(result);
        }));
    co_return;
}

custom_types::Helpers::Coroutine YouTubeAuth::RefreshAccessToken(
    std::function<void(TokenResult)> callback) {
    if (!HasUsableCredentials() || refreshToken_.empty()) {
        TokenResult r{};
        r.denied = true;
        r.error = "Not signed in.";
        callback(r);
        co_return;
    }

    std::string body = "client_id=" + UrlUtils::UrlEncode(clientId_) +
                       "&client_secret=" + UrlUtils::UrlEncode(clientSecret_) +
                       "&refresh_token=" + UrlUtils::UrlEncode(refreshToken_) +
                       "&grant_type=refresh_token";

    auto* self = this;
    co_yield custom_types::Helpers::CoroutineHelper::New(
        Http::PostForm(kTokenUrl, body, [self, callback](Http::Response response) {
            TokenResult result{};

            if (!response.completed) {
                result.error = response.error.empty() ? "Network error" : response.error;
                callback(result);
                return;
            }

            rapidjson::Document doc;
            if (doc.Parse(response.body.c_str()).HasParseError() || !doc.IsObject()) {
                result.error = "Malformed token response";
                callback(result);
                return;
            }

            if (!response.ok) {
                // invalid_grant means the refresh token was revoked or the
                // account changed its password -- the user has to sign in again,
                // so there is no point retrying with backoff.
                std::string err = GetStr(doc, "error");
                result.denied = (err == "invalid_grant" || err == "invalid_client");
                result.error = GetStr(doc, "error_description",
                                      result.denied ? "Sign-in expired, please sign in again." : err);
                callback(result);
                return;
            }

            result.accessToken = GetStr(doc, "access_token");
            result.expiresInSeconds = GetInt(doc, "expires_in", 3600);
            result.ok = !result.accessToken.empty();
            if (!result.ok) result.error = "No access token in refresh response";
            self->ApplyToken(result);
            callback(result);
        }));
    co_return;
}

custom_types::Helpers::Coroutine YouTubeAuth::EnsureAccessToken(
    std::function<void(bool, std::string)> callback) {
    if (AccessTokenIsFresh()) {
        callback(true, "");
        co_return;
    }
    if (refreshToken_.empty()) {
        callback(false, "Not signed in to YouTube.");
        co_return;
    }

    bool ok = false;
    std::string error;
    co_yield custom_types::Helpers::CoroutineHelper::New(
        RefreshAccessToken([&ok, &error](TokenResult token) {
            ok = token.ok;
            error = token.error;
        }));

    if (!ok && error.empty()) error = "Could not refresh the YouTube sign-in.";
    Log().debug("EnsureAccessToken -> {}", ok);
    callback(ok, error);
    co_return;
}

}  // namespace YouTubeLiveChat

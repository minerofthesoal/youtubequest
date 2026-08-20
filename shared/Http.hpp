#pragma once
#include <string>
#include <functional>

#include "custom-types/shared/coroutine.hpp"

// ---------------------------------------------------------------------------
// Minimal HTTP client, built on System.Net.Http.HttpClient through codegen.
//
// It deliberately does NOT use UnityWebRequest: Beat Saber's IL2CPP build has
// UnityWebRequest::Post, UploadHandlerRaw and SetRequestHeader stripped out
// (they are absent from bs-cordl because nothing in the game references them),
// so UnityWebRequest can only issue header-less GETs. OAuth needs POST with a
// form body, and every authenticated call needs an `Authorization: Bearer`
// header, so neither is optional here.
//
// System.Net.Http does the transfer on a .NET thread-pool thread. Request()
// is a Unity coroutine that parks on the returned Task and yields a frame at a
// time until it completes, so the callback always runs on the main thread and
// callers never have to think about threading.
// ---------------------------------------------------------------------------

namespace YouTubeLiveChat::Http {

struct Response {
    bool ok = false;       // transport succeeded AND status is 2xx
    bool completed = false;  // transport succeeded (a non-2xx status still sets this)
    long status = 0;
    std::string body;
    std::string error;     // transport-level failure text, empty on completion
};

using Callback = std::function<void(Response)>;

// method: "GET" or "POST".
// body/contentType are ignored for GET.
// bearerToken, when non-empty, is sent as `Authorization: Bearer <token>`.
custom_types::Helpers::Coroutine Request(std::string method,
                                         std::string url,
                                         std::string body,
                                         std::string contentType,
                                         std::string bearerToken,
                                         Callback callback);

inline custom_types::Helpers::Coroutine Get(std::string url, std::string bearerToken, Callback callback) {
    return Request("GET", std::move(url), "", "", std::move(bearerToken), std::move(callback));
}

inline custom_types::Helpers::Coroutine PostForm(std::string url, std::string formBody, Callback callback) {
    return Request("POST", std::move(url), std::move(formBody),
                   "application/x-www-form-urlencoded", "", std::move(callback));
}

}  // namespace YouTubeLiveChat::Http

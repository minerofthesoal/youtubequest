#include "UI/AvatarCache.hpp"
#include "Logging.hpp"

#include "UnityEngine/Networking/UnityWebRequest.hpp"
#include "UnityEngine/Networking/UnityWebRequestTexture.hpp"
#include "UnityEngine/Networking/DownloadHandlerTexture.hpp"
#include "UnityEngine/Object.hpp"

#include "beatsaber-hook/shared/utils/typedefs-wrappers.hpp"

using namespace UnityEngine;
using namespace UnityEngine::Networking;

namespace {
constexpr System::Collections::IEnumerator* kNextFrame = nullptr;
}

namespace YouTubeLiveChat::UI {

void AvatarCache::GetOrFetch(const std::string& url,
                             std::function<void(UnityEngine::Texture2D*)> callback) {
    if (url.empty() || !host_) {
        callback(nullptr);
        return;
    }

    auto cached = cache_.find(url);
    if (cached != cache_.end()) {
        // A cached entry can still go stale if Unity destroyed the texture on
        // a scene unload; SafePtrUnity reports that as not-alive.
        if (cached->second) {
            callback(cached->second.ptr());
            return;
        }
        cache_.erase(cached);
    }

    // Coalesce: several chatters' messages can arrive in the same poll, and a
    // repeat chatter would otherwise start a second download for a URL that is
    // already in flight.
    auto pending = inFlight_.find(url);
    if (pending != inFlight_.end()) {
        pending->second.push_back(std::move(callback));
        return;
    }
    inFlight_[url].push_back(std::move(callback));

    System::Collections::IEnumerator* enumerator =
        custom_types::Helpers::CoroutineHelper::New(FetchCoroutine(url));
    host_->StartCoroutine(enumerator);
}

void AvatarCache::Clear() {
    cache_.clear();
}

custom_types::Helpers::Coroutine AvatarCache::FetchCoroutine(std::string url) {
    SafePtr<UnityWebRequest> request;
    try {
        // Avatars are plain public image GETs, so UnityWebRequest is fine here
        // (unlike the API calls, which need POST bodies and auth headers that
        // this game's stripped UnityWebRequest cannot express).
        request = UnityWebRequestTexture::GetTexture(StringW(url), true);
        request->set_timeout(15);
        request->SendWebRequest();
    } catch (std::exception const& e) {
        Log().warn("Avatar request failed to start: {}", e.what());
        auto waiters = std::move(inFlight_[url]);
        inFlight_.erase(url);
        for (auto& cb : waiters) cb(nullptr);
        co_return;
    }

    while (!request->get_isDone()) {
        co_yield kNextFrame;
    }

    Texture2D* texture = nullptr;
    try {
        if (request->get_result().value__ == UnityWebRequest::Result::Success.value__) {
            auto* handler = static_cast<DownloadHandlerTexture*>(request->get_downloadHandler());
            if (handler) texture = handler->get_texture();
        } else {
            Log().debug("Avatar fetch failed for {}", url);
        }
    } catch (std::exception const& e) {
        Log().warn("Avatar decode failed: {}", e.what());
    }

    if (texture) {
        // Simplest possible eviction: drop everything rather than track an LRU
        // for a set that is rarely more than a few dozen chatters per stream.
        // The textures themselves are owned by Unity and collected normally.
        if (cache_.size() >= maxEntries_) cache_.clear();
        cache_[url] = texture;
    }

    auto waiters = std::move(inFlight_[url]);
    inFlight_.erase(url);
    for (auto& cb : waiters) cb(texture);

    try {
        request->Dispose();
    } catch (std::exception const&) {
        // Best effort -- a failed dispose must not take the callbacks with it.
    }
    co_return;
}

}  // namespace YouTubeLiveChat::UI

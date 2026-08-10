#include "AvatarCache.hpp"

#include "UnityEngine/Networking/UnityWebRequest.hpp"
#include "UnityEngine/Networking/UnityWebRequestTexture.hpp"
#include "UnityEngine/Networking/DownloadHandlerTexture.hpp"
#include "UnityEngine/Texture2D.hpp"
#include "UnityEngine/Rect.hpp"
#include "UnityEngine/Vector2.hpp"
#include "custom-types/shared/macros.hpp"

#include "paper2_scotland2/shared/logger.hpp"

using namespace UnityEngine;
using namespace UnityEngine::Networking;

static auto& log() {
    static auto logger = Paper::ConstLoggerContext("YTLiveChat.Avatar");
    return logger;
}

namespace YouTubeLiveChat::UI {

void AvatarCache::GetOrFetch(const std::string& url, std::function<void(UnityEngine::Sprite*)> callback) {
    if (url.empty()) {
        callback(nullptr);
        return;
    }

    auto it = cache_.find(url);
    if (it != cache_.end()) {
        callback(it->second);
        return;
    }

    auto pending = inFlight_.find(url);
    if (pending != inFlight_.end()) {
        pending->second.push_back(std::move(callback));
        return;
    }

    inFlight_[url] = { std::move(callback) };

    host_->StartCoroutine(
        reinterpret_cast<System::Collections::IEnumerator*>(
            custom_types::Helpers::CoroutineHelper::New(FetchCoroutine(url))
        )
    );
}

custom_types::Helpers::Coroutine AvatarCache::FetchCoroutine(std::string url) {
    auto* request = UnityWebRequestTexture::GetTexture(url);
    auto* op = request->SendWebRequest();
    co_yield reinterpret_cast<System::Collections::IEnumerator*>(op);

    Sprite* sprite = nullptr;
    // VERIFY: get_result()/Result::Success vs isNetworkError()/isHttpError()
    // -- same caveat as YouTubeApiClient::SendGet, check against your codegen.
    if (request->get_result() == UnityWebRequest::Result::Success) {
        auto* handler = reinterpret_cast<DownloadHandlerTexture*>(request->get_downloadHandler());
        Texture2D* tex = handler->get_texture();
        if (tex) {
            sprite = Sprite::Create(
                tex,
                Rect(0, 0, tex->get_width(), tex->get_height()),
                Vector2(0.5f, 0.5f),
                100.0f);
        }
    } else {
        log().warn("Avatar fetch failed for {}", url);
    }

    if (cache_.size() >= maxEntries_) {
        cache_.clear(); // simplest eviction: clear-all rather than tracking LRU for a
                         // list that's rarely more than a few dozen unique chatters/stream
    }
    if (sprite) cache_[url] = sprite;

    auto waiters = std::move(inFlight_[url]);
    inFlight_.erase(url);
    for (auto& cb : waiters) cb(sprite);

    request->Dispose();
    co_return;
}

}

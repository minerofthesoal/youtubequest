#pragma once
#include <string>
#include <unordered_map>
#include <functional>

#include "custom-types/shared/coroutine.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/Sprite.hpp"

namespace YouTubeLiveChat::UI {

// Downloads + caches profile-picture Sprites by URL so repeat chatters don't
// re-download their avatar for every message. Deliberately capped in size --
// a long stream with hundreds of unique chatters should not grow this
// unbounded on a headset with limited RAM.
class AvatarCache {
public:
    explicit AvatarCache(UnityEngine::MonoBehaviour* host, size_t maxEntries = 200)
        : host_(host), maxEntries_(maxEntries) {}

    // Calls back with a Sprite* once available (immediately, from cache, if
    // we already have it -- otherwise after an async download completes).
    // Callback may be invoked with nullptr on failure; callers should treat
    // that as "no avatar" and fall back to a placeholder / initials.
    void GetOrFetch(const std::string& url, std::function<void(UnityEngine::Sprite*)> callback);

private:
    UnityEngine::MonoBehaviour* host_;
    size_t maxEntries_;
    std::unordered_map<std::string, UnityEngine::Sprite*> cache_;
    std::unordered_map<std::string, std::vector<std::function<void(UnityEngine::Sprite*)>>> inFlight_;

    custom_types::Helpers::Coroutine FetchCoroutine(std::string url);
};

}

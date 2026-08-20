#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

#include "custom-types/shared/coroutine.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/Texture2D.hpp"
#include "UnityEngine/Object.hpp"  // SafePtr static_assert needs Object complete

#include "beatsaber-hook/shared/utils/typedefs-wrappers.hpp"

namespace YouTubeLiveChat::UI {

// Downloads and caches profile-picture textures by URL so repeat chatters
// don't re-download their avatar for every message. Capped in size -- a long
// stream with hundreds of unique chatters should not grow this unbounded on a
// headset with limited RAM.
class AvatarCache {
public:
    explicit AvatarCache(UnityEngine::MonoBehaviour* host, size_t maxEntries = 128)
        : host_(host), maxEntries_(maxEntries) {}

    // Calls back with a texture once available (immediately, from cache, if we
    // already have it -- otherwise after an async download). The callback may
    // be invoked with nullptr on failure; callers should treat that as "no
    // avatar" and leave the placeholder in place.
    void GetOrFetch(const std::string& url, std::function<void(UnityEngine::Texture2D*)> callback);

    void Clear();

private:
    UnityEngine::MonoBehaviour* host_ = nullptr;
    size_t maxEntries_ = 128;
    // SafePtrUnity, not a bare pointer: this map is plain C++ memory, which
    // the il2cpp GC does not scan, so a cached texture with no other live
    // reference would be collected out from under us between polls.
    std::unordered_map<std::string, SafePtrUnity<UnityEngine::Texture2D>> cache_;
    std::unordered_map<std::string, std::vector<std::function<void(UnityEngine::Texture2D*)>>> inFlight_;

    custom_types::Helpers::Coroutine FetchCoroutine(std::string url);
};

}  // namespace YouTubeLiveChat::UI

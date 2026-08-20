#pragma once
#include "paper2_scotland2/shared/logger.hpp"

#include <string>

namespace YouTubeLiveChat {

// One process-wide logger context. paper2's ConstLoggerContext is a
// class-type NTTP holding the tag as a char array, so it has to be built
// with CTAD from a literal -- `ConstLoggerContext<"tag">()` is NOT the API
// (that spelling reads the literal as a std::size_t template argument and
// fails with a confusing conversion error).
//
// Returned by non-const reference because beatsaber-hook's INSTALL_HOOK
// takes `L&` and constrains it on `is_logger<L>`.
inline auto& Log() {
    static auto logger = Paper::ConstLoggerContext("YTLiveChat");
    return logger;
}

}  // namespace YouTubeLiveChat

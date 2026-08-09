#pragma once
#include <string>
#include <optional>

namespace YouTubeLiveChat::UrlUtils {

// Accepts either a bare 11-character video ID or one of:
//   https://www.youtube.com/watch?v=VIDEOID
//   https://youtu.be/VIDEOID
//   https://www.youtube.com/live/VIDEOID
//   https://m.youtube.com/watch?v=VIDEOID
//   (any of the above with extra query params, which are stripped)
// Returns std::nullopt if nothing that looks like a video ID could be found.
std::optional<std::string> ExtractVideoId(const std::string& input);

// Trims ASCII whitespace from both ends.
std::string Trim(const std::string& input);

}

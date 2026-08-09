#include "UrlUtils.hpp"
#include <cctype>
#include <array>

namespace YouTubeLiveChat::UrlUtils {

std::string Trim(const std::string& input) {
    size_t start = 0;
    size_t end = input.size();
    while (start < end && std::isspace(static_cast<unsigned char>(input[start]))) start++;
    while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1]))) end--;
    return input.substr(start, end - start);
}

namespace {
    bool IsValidIdChar(char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-';
    }

    // Video IDs are (in practice) always 11 chars of [A-Za-z0-9_-].
    std::optional<std::string> TakeElevenCharId(const std::string& s, size_t startPos) {
        size_t end = startPos;
        while (end < s.size() && IsValidIdChar(s[end])) end++;
        std::string candidate = s.substr(startPos, end - startPos);
        // YouTube IDs are 11 chars; be lenient (8-16) in case of future format changes,
        // but this is the pragmatic heuristic every third-party YouTube tool uses --
        // there is no public spec guaranteeing the length forever.
        if (candidate.size() >= 8 && candidate.size() <= 16) {
            return candidate;
        }
        return std::nullopt;
    }

    std::optional<std::string> FindAfterMarker(const std::string& s, const std::string& marker) {
        auto pos = s.find(marker);
        if (pos == std::string::npos) return std::nullopt;
        return TakeElevenCharId(s, pos + marker.size());
    }
}

std::optional<std::string> ExtractVideoId(const std::string& rawInput) {
    std::string input = Trim(rawInput);
    if (input.empty()) return std::nullopt;

    // If it doesn't look like a URL at all, and every char is a valid ID char,
    // treat the whole trimmed string as a raw video ID.
    if (input.find("youtube.com") == std::string::npos &&
        input.find("youtu.be") == std::string::npos) {
        bool allValid = !input.empty();
        for (char c : input) {
            if (!IsValidIdChar(c)) { allValid = false; break; }
        }
        if (allValid && input.size() >= 8 && input.size() <= 16) {
            return input;
        }
        // Not a URL and not a plausible bare ID.
        return std::nullopt;
    }

    static const std::array<std::string, 3> markers = {
        "watch?v=", "youtu.be/", "/live/"
    };
    for (const auto& marker : markers) {
        if (auto id = FindAfterMarker(input, marker)) {
            return id;
        }
    }

    // "&v=VIDEOID" as a fallback for odd param ordering.
    if (auto id = FindAfterMarker(input, "&v=")) {
        return id;
    }

    return std::nullopt;
}

}

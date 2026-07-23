#include "ReadChunkConfig.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
// Smaller high-thread chunks reduce serialized input-lock hold and final-tail
// imbalance; lower thread counts retain STAR's legacy buffer granularity.
constexpr std::uint32_t adaptiveThreadThreshold = 64;
constexpr std::uint64_t adaptiveTargetBytes = 1000000;

std::invalid_argument invalidValue(const std::string &message)
{
    return std::invalid_argument("invalid read chunk configuration: " + message);
}
}

ReadChunkConfig calculateReadChunkConfig(
    std::uint64_t maximumTotalBytes,
    std::uint64_t requestedTotalBytes,
    std::uint32_t readEnds,
    std::uint32_t runThreads,
    std::uint64_t reservePerEnd
)
{
    if (readEnds == 0) {
        throw invalidValue("the number of read ends is zero");
    }
    if (reservePerEnd == std::numeric_limits<std::uint64_t>::max()) {
        throw invalidValue("the per-end reserve overflows");
    }

    const std::uint64_t minimumPerEnd = reservePerEnd + 1;
    if (minimumPerEnd > std::numeric_limits<std::uint64_t>::max() / readEnds) {
        throw invalidValue("the minimum total buffer size overflows");
    }
    const std::uint64_t minimumTotalBytes = minimumPerEnd * readEnds;
    if (maximumTotalBytes < minimumTotalBytes) {
        throw invalidValue(
            "limitIObufferSize input bytes must be at least " +
            std::to_string(minimumTotalBytes)
        );
    }
    if (requestedTotalBytes > maximumTotalBytes) {
        throw invalidValue(
            "readChunkSizeBytes exceeds limitIObufferSize input bytes"
        );
    }

    std::uint64_t effectiveTotalBytes = maximumTotalBytes;
    bool adaptive = false;
    if (requestedTotalBytes > 0) {
        effectiveTotalBytes = requestedTotalBytes;
    } else if (runThreads >= adaptiveThreadThreshold) {
        const std::uint64_t autoTarget = std::max(
            adaptiveTargetBytes,
            minimumTotalBytes
        );
        effectiveTotalBytes = std::min(maximumTotalBytes, autoTarget);
        adaptive = effectiveTotalBytes < maximumTotalBytes;
    }

    if (effectiveTotalBytes < minimumTotalBytes) {
        throw invalidValue(
            "readChunkSizeBytes must be zero or at least " +
            std::to_string(minimumTotalBytes)
        );
    }

    const std::uint64_t perEndArrayBytes = effectiveTotalBytes / readEnds;
    if (perEndArrayBytes > std::numeric_limits<std::uint32_t>::max()) {
        throw invalidValue("the per-end input buffer exceeds 32-bit storage");
    }

    return {
        effectiveTotalBytes,
        perEndArrayBytes,
        perEndArrayBytes - reservePerEnd,
        adaptive
    };
}

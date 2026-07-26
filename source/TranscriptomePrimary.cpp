#include "TranscriptomePrimary.h"

namespace {
std::uint64_t splitMix64(std::uint64_t value)
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}
}

std::uint32_t transcriptomePrimaryIndex(
    int runSeed,
    std::uint64_t readIndex,
    std::uint32_t alignmentCount
)
{
    if (alignmentCount == 0) {
        return 0;
    }

    const std::uint64_t seed =
        static_cast<std::uint64_t>(static_cast<std::uint32_t>(runSeed));
    const std::uint64_t randomValue =
        splitMix64(readIndex ^ (seed << 32) ^ seed);
    return static_cast<std::uint32_t>(randomValue % alignmentCount);
}

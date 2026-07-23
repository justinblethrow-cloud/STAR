#ifndef READ_CHUNK_CONFIG_H
#define READ_CHUNK_CONFIG_H

#include <cstdint>

struct ReadChunkConfig {
    std::uint64_t effectiveTotalBytes;
    std::uint64_t perEndArrayBytes;
    std::uint64_t perEndPayloadBytes;
    bool adaptive;
};

ReadChunkConfig calculateReadChunkConfig(
    std::uint64_t maximumTotalBytes,
    std::uint64_t requestedTotalBytes,
    std::uint32_t readEnds,
    std::uint32_t runThreads,
    std::uint64_t reservePerEnd
);

#endif

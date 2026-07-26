#ifndef READ_CHUNK_CONFIG_H
#define READ_CHUNK_CONFIG_H

#include <cstdint>
#include <string>

struct ReadChunkConfig {
    std::uint64_t effectiveTotalBytes;
    std::uint64_t perEndArrayBytes;
    std::uint64_t perEndPayloadBytes;
    bool adaptive;
};

bool automaticReadChunkSizingAllowed(
    std::uint32_t readFilesType
);

ReadChunkConfig calculateReadChunkConfig(
    std::uint64_t maximumTotalBytes,
    std::uint64_t requestedTotalBytes,
    std::uint32_t readEnds,
    std::uint32_t runThreads,
    std::uint64_t reservePerEnd,
    bool adaptiveAllowed = true,
    std::uint32_t minimumRecordSlots = 1
);

bool appendReadChunkRecord(
    char *buffer,
    std::uint64_t capacity,
    std::uint64_t &used,
    const std::string &record
);

#endif

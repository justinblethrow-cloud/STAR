#ifndef H_TranscriptomePrimary
#define H_TranscriptomePrimary

#include <cstdint>

std::uint32_t transcriptomePrimaryIndex(
    int runSeed,
    std::uint64_t readIndex,
    std::uint32_t alignmentCount
);

#endif

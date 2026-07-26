#include "TranscriptomePrimary.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>

int main()
{
    assert(transcriptomePrimaryIndex(777, 1, 0) == 0);
    assert(transcriptomePrimaryIndex(777, 1, 1) == 0);

    const std::uint32_t first =
        transcriptomePrimaryIndex(777, 123456789ULL, 17);
    assert(first == 9);
    assert(first == transcriptomePrimaryIndex(777, 123456789ULL, 17));
    assert(transcriptomePrimaryIndex(778, 123456789ULL, 17) == 3);
    assert(transcriptomePrimaryIndex(777, 123456790ULL, 17) == 6);
    assert(transcriptomePrimaryIndex(0, UINT64_MAX, 31) == 17);

    std::array<std::uint32_t, 7> buckets = {{0, 0, 0, 0, 0, 0, 0}};
    for (std::uint64_t readIndex = 1; readIndex <= 70000; ++readIndex) {
        ++buckets[transcriptomePrimaryIndex(777, readIndex, buckets.size())];
    }
    for (std::uint32_t count : buckets) {
        assert(count > 9500);
        assert(count < 10500);
    }

    std::cout << "transcriptome primary-selection tests passed\n";
    return 0;
}

#include "ReadChunkConfig.h"

#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
void expectInvalid(const std::function<void()> &operation)
{
    bool rejected = false;
    try {
        operation();
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    assert(rejected);
}
}

int main()
{
    const std::uint64_t reservePerEnd = 101302;

    const ReadChunkConfig adaptive = calculateReadChunkConfig(
        30000000, 0, 2, 64, reservePerEnd
    );
    assert(adaptive.adaptive);
    assert(adaptive.effectiveTotalBytes == 1000000);
    assert(adaptive.perEndArrayBytes == 500000);
    assert(adaptive.perEndPayloadBytes == 398698);

    const ReadChunkConfig lowThread = calculateReadChunkConfig(
        30000000, 0, 2, 63, reservePerEnd
    );
    assert(!lowThread.adaptive);
    assert(lowThread.effectiveTotalBytes == 30000000);
    assert(lowThread.perEndArrayBytes == 15000000);

    const ReadChunkConfig alreadyFineGrained = calculateReadChunkConfig(
        500000, 0, 2, 96, reservePerEnd
    );
    assert(!alreadyFineGrained.adaptive);
    assert(alreadyFineGrained.effectiveTotalBytes == 500000);

    const ReadChunkConfig explicitTarget = calculateReadChunkConfig(
        30000000, 4000000, 2, 96, reservePerEnd
    );
    assert(!explicitTarget.adaptive);
    assert(explicitTarget.effectiveTotalBytes == 4000000);

    const ReadChunkConfig longRead = calculateReadChunkConfig(
        30000000, 0, 2, 96, 1100000
    );
    assert(longRead.adaptive);
    assert(longRead.effectiveTotalBytes == 2200002);
    assert(longRead.perEndPayloadBytes == 1);

    expectInvalid([&]() {
        calculateReadChunkConfig(30000000, 31000000, 2, 96, reservePerEnd);
    });
    expectInvalid([&]() {
        calculateReadChunkConfig(200000, 0, 2, 96, reservePerEnd);
    });
    expectInvalid([&]() {
        calculateReadChunkConfig(30000000, 200000, 2, 96, reservePerEnd);
    });
    expectInvalid([&]() {
        calculateReadChunkConfig(30000000, 0, 0, 96, reservePerEnd);
    });
    expectInvalid([&]() {
        calculateReadChunkConfig(8589934592ULL, 0, 2, 1, reservePerEnd);
    });

    std::cout << "read chunk configuration tests passed\n";
    return 0;
}

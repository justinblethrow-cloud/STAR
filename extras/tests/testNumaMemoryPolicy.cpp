#include "NumaMemoryPolicy.h"

#include <iostream>
#include <string>

namespace {
bool expect(
    const std::string &requested,
    const std::string &runMode,
    const std::string &genomeLoad,
    int runThreads,
    int allowedNodes,
    bool supported,
    bool valid,
    bool interleave,
    const std::string &reason
)
{
    const BlackstarNumaPolicyChoice choice =
        blackstarSelectNumaMemoryPolicy(
            requested,
            runMode,
            genomeLoad,
            runThreads,
            allowedNodes,
            supported
        );
    if (choice.valid != valid ||
        choice.interleave != interleave ||
        choice.reason != reason) {
        std::cerr << "unexpected NUMA policy choice for " << requested
                  << ": valid=" << choice.valid
                  << " interleave=" << choice.interleave
                  << " reason=" << choice.reason << '\n';
        return false;
    };
    return true;
}
}

int main()
{
    bool passed = true;
    passed &= expect(
        "Auto", "alignReads", "NoSharedMemory", 96, 8, true,
        true, true, "automatic-interleave"
    );
    passed &= expect(
        "Auto", "alignReads", "NoSharedMemory", 63, 8, true,
        true, false, "below-thread-threshold"
    );
    passed &= expect(
        "Auto", "alignReads", "NoSharedMemory", 96, 1, true,
        true, false, "single-memory-node"
    );
    passed &= expect(
        "Auto", "alignReads", "LoadAndKeep", 96, 8, true,
        true, false, "shared-genome"
    );
    passed &= expect(
        "Auto", "genomeGenerate", "NoSharedMemory", 96, 8, true,
        true, false, "not-align-reads"
    );
    passed &= expect(
        "Auto", "alignReads", "NoSharedMemory", 96, 8, false,
        true, false, "platform-unsupported"
    );
    passed &= expect(
        "Default", "alignReads", "NoSharedMemory", 96, 8, true,
        true, false, "explicit-default"
    );
    passed &= expect(
        "Interleave", "alignReads", "NoSharedMemory", 1, 1, true,
        true, true, "explicit-interleave"
    );
    passed &= expect(
        "invalid", "alignReads", "NoSharedMemory", 96, 8, true,
        false, false, "invalid-request"
    );

    const BlackstarNumaPolicyResult unchanged =
        blackstarApplyNumaMemoryPolicy(
            "Default", "alignReads", "NoSharedMemory", 96
        );
    passed &= !unchanged.active &&
              unchanged.status == 0 &&
              unchanged.effective == "Default" &&
              unchanged.reason == "explicit-default";

    return passed ? 0 : 1;
}

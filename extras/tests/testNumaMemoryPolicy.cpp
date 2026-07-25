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
    BlackstarNumaInheritedPolicy inheritedPolicy,
    bool valid,
    bool interleave,
    bool applyInterleave,
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
            supported,
            inheritedPolicy
        );
    if (choice.valid != valid ||
        choice.interleave != interleave ||
        choice.applyInterleave != applyInterleave ||
        choice.reason != reason) {
        std::cerr << "unexpected NUMA policy choice for " << requested
                  << ": valid=" << choice.valid
                  << " interleave=" << choice.interleave
                  << " applyInterleave=" << choice.applyInterleave
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
        BlackstarNumaInheritedDefault,
        true, true, true, "automatic-interleave"
    );
    passed &= expect(
        "Auto", "alignReads", "NoSharedMemory", 63, 8, true,
        BlackstarNumaInheritedDefault,
        true, false, false, "below-thread-threshold"
    );
    passed &= expect(
        "Auto", "alignReads", "NoSharedMemory", 96, 1, true,
        BlackstarNumaInheritedDefault,
        true, false, false, "single-memory-node"
    );
    passed &= expect(
        "Auto", "alignReads", "LoadAndKeep", 96, 8, true,
        BlackstarNumaInheritedDefault,
        true, false, false, "shared-genome"
    );
    passed &= expect(
        "Auto", "genomeGenerate", "NoSharedMemory", 96, 8, true,
        BlackstarNumaInheritedDefault,
        true, false, false, "not-align-reads"
    );
    passed &= expect(
        "Auto", "alignReads", "NoSharedMemory", 96, 8, false,
        BlackstarNumaInheritedUnknown,
        true, false, false, "platform-unsupported"
    );
    passed &= expect(
        "Auto", "alignReads", "NoSharedMemory", 96, 8, true,
        BlackstarNumaInheritedInterleave,
        true, true, false, "inherited-interleave"
    );
    passed &= expect(
        "Auto", "alignReads", "NoSharedMemory", 96, 8, true,
        BlackstarNumaInheritedOther,
        true, false, false, "inherited-policy-preserved"
    );
    passed &= expect(
        "Auto", "alignReads", "NoSharedMemory", 96, 8, true,
        BlackstarNumaInheritedUnknown,
        true, false, false, "inherited-policy-unavailable"
    );
    passed &= expect(
        "Default", "alignReads", "NoSharedMemory", 96, 8, true,
        BlackstarNumaInheritedOther,
        true, false, false, "explicit-default"
    );
    passed &= expect(
        "Interleave", "alignReads", "NoSharedMemory", 1, 1, true,
        BlackstarNumaInheritedOther,
        true, true, true, "explicit-interleave"
    );
    passed &= expect(
        "invalid", "alignReads", "NoSharedMemory", 96, 8, true,
        BlackstarNumaInheritedDefault,
        false, false, false, "invalid-request"
    );

    BlackstarNumaPolicyState state;
    const BlackstarNumaPolicyResult unchanged =
        blackstarApplyNumaMemoryPolicy(
            "Default", "alignReads", "NoSharedMemory", 96, state
        );
    passed &= !unchanged.active &&
              unchanged.status == 0 &&
              unchanged.effective == "Default" &&
              unchanged.reason == "explicit-default" &&
              !state.restoreRequired;

    const BlackstarNumaPolicyRestoreResult restore =
        blackstarRestoreNumaMemoryPolicy(state);
    passed &= !restore.attempted &&
              restore.restored &&
              restore.status == 0 &&
              restore.reason == "not-required";

    return passed ? 0 : 1;
}

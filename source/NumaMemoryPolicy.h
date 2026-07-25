#ifndef H_BLACKSTAR_NUMA_MEMORY_POLICY
#define H_BLACKSTAR_NUMA_MEMORY_POLICY

#include <string>
#include <vector>

enum BlackstarNumaInheritedPolicy {
    BlackstarNumaInheritedUnknown,
    BlackstarNumaInheritedDefault,
    BlackstarNumaInheritedInterleave,
    BlackstarNumaInheritedOther
};

struct BlackstarNumaPolicyChoice {
    bool valid;
    bool interleave;
    bool applyInterleave;
    std::string reason;
};

struct BlackstarNumaPolicyResult {
    bool active;
    int allowedNodeCount;
    int inheritedNodeCount;
    int status;
    std::string requested;
    std::string effective;
    std::string inherited;
    std::string reason;
};

struct BlackstarNumaPolicyState {
    bool restoreRequired;
    int inheritedMode;
    unsigned long maximumNodes;
    std::vector<unsigned long> inheritedMask;
    std::string inheritedName;
};

struct BlackstarNumaPolicyRestoreResult {
    bool attempted;
    bool restored;
    int status;
    std::string effective;
    std::string reason;
};

BlackstarNumaPolicyChoice blackstarSelectNumaMemoryPolicy(
    const std::string &requested,
    const std::string &runMode,
    const std::string &genomeLoad,
    int runThreads,
    int allowedNodeCount,
    bool platformSupported,
    BlackstarNumaInheritedPolicy inheritedPolicy
);

BlackstarNumaPolicyResult blackstarApplyNumaMemoryPolicy(
    const std::string &requested,
    const std::string &runMode,
    const std::string &genomeLoad,
    int runThreads,
    BlackstarNumaPolicyState &state
);

BlackstarNumaPolicyRestoreResult blackstarRestoreNumaMemoryPolicy(
    const BlackstarNumaPolicyState &state
);

#endif

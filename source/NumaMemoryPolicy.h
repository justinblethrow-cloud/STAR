#ifndef H_BLACKSTAR_NUMA_MEMORY_POLICY
#define H_BLACKSTAR_NUMA_MEMORY_POLICY

#include <string>

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
    int runThreads
);

#endif

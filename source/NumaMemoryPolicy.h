#ifndef H_BLACKSTAR_NUMA_MEMORY_POLICY
#define H_BLACKSTAR_NUMA_MEMORY_POLICY

#include <string>

struct BlackstarNumaPolicyChoice {
    bool valid;
    bool interleave;
    std::string reason;
};

struct BlackstarNumaPolicyResult {
    bool active;
    int allowedNodeCount;
    int status;
    std::string requested;
    std::string effective;
    std::string reason;
};

BlackstarNumaPolicyChoice blackstarSelectNumaMemoryPolicy(
    const std::string &requested,
    const std::string &runMode,
    const std::string &genomeLoad,
    int runThreads,
    int allowedNodeCount,
    bool platformSupported
);

BlackstarNumaPolicyResult blackstarApplyNumaMemoryPolicy(
    const std::string &requested,
    const std::string &runMode,
    const std::string &genomeLoad,
    int runThreads
);

#endif

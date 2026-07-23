#include "NumaMemoryPolicy.h"

#include <cerrno>
#include <vector>

#if defined(__linux__)
#include <linux/mempolicy.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace {
const int automaticThreadThreshold = 64;

#if defined(__linux__) && defined(SYS_get_mempolicy) && defined(SYS_set_mempolicy)
const unsigned long maximumNumaNodes = 1024;

int allowedMemoryNodes(std::vector<unsigned long> &mask)
{
    const unsigned long bitsPerWord = sizeof(unsigned long) * 8;
    mask.assign(
        (maximumNumaNodes + bitsPerWord - 1) / bitsPerWord,
        0
    );
    int mode = 0;
    if (syscall(
            SYS_get_mempolicy,
            &mode,
            mask.data(),
            maximumNumaNodes,
            NULL,
            MPOL_F_MEMS_ALLOWED
        ) != 0) {
        return -1;
    };

    int count = 0;
    for (std::vector<unsigned long>::const_iterator word = mask.begin();
         word != mask.end();
         ++word) {
        count += __builtin_popcountl(*word);
    };
    return count;
}

int applyInterleave(const std::vector<unsigned long> &mask)
{
    if (syscall(
            SYS_set_mempolicy,
            MPOL_INTERLEAVE,
            mask.data(),
            maximumNumaNodes
        ) == 0) {
        return 0;
    };
    return errno == 0 ? EIO : errno;
}
#endif
}

BlackstarNumaPolicyChoice blackstarSelectNumaMemoryPolicy(
    const std::string &requested,
    const std::string &runMode,
    const std::string &genomeLoad,
    int runThreads,
    int allowedNodeCount,
    bool platformSupported
)
{
    if (requested != "Auto" &&
        requested != "Default" &&
        requested != "Interleave") {
        return {false, false, "invalid-request"};
    };
    if (requested == "Default") {
        return {true, false, "explicit-default"};
    };
    if (runMode != "alignReads") {
        return {true, false, "not-align-reads"};
    };
    if (genomeLoad != "NoSharedMemory") {
        return {true, false, "shared-genome"};
    };
    if (!platformSupported) {
        return {true, false, "platform-unsupported"};
    };
    if (allowedNodeCount < 1) {
        return {true, false, "allowed-nodes-unavailable"};
    };
    if (requested == "Auto" && runThreads < automaticThreadThreshold) {
        return {true, false, "below-thread-threshold"};
    };
    if (requested == "Auto" && allowedNodeCount < 2) {
        return {true, false, "single-memory-node"};
    };
    return {
        true,
        true,
        requested == "Auto" ? "automatic-interleave" : "explicit-interleave"
    };
}

BlackstarNumaPolicyResult blackstarApplyNumaMemoryPolicy(
    const std::string &requested,
    const std::string &runMode,
    const std::string &genomeLoad,
    int runThreads
)
{
    BlackstarNumaPolicyResult result = {
        false,
        -1,
        0,
        requested,
        "Default",
        "not-evaluated"
    };

    std::vector<unsigned long> allowedMask;
    bool platformSupported = false;
#if defined(__linux__) && defined(SYS_get_mempolicy) && defined(SYS_set_mempolicy)
    platformSupported = true;
    if (requested != "Default" &&
        runMode == "alignReads" &&
        genomeLoad == "NoSharedMemory") {
        result.allowedNodeCount = allowedMemoryNodes(allowedMask);
        if (result.allowedNodeCount < 0) {
            result.status = errno == 0 ? EIO : errno;
        };
    };
#endif

    const BlackstarNumaPolicyChoice choice =
        blackstarSelectNumaMemoryPolicy(
            requested,
            runMode,
            genomeLoad,
            runThreads,
            result.allowedNodeCount,
            platformSupported
        );
    result.reason = choice.reason;
    if (!choice.valid) {
        result.status = EINVAL;
        return result;
    };
    if (!choice.interleave) {
        return result;
    };
    if (result.status != 0) {
        result.reason = "allowed-nodes-query-failed";
        return result;
    };

#if defined(__linux__) && defined(SYS_get_mempolicy) && defined(SYS_set_mempolicy)
    result.status = applyInterleave(allowedMask);
    if (result.status == 0) {
        result.active = true;
        result.effective = "Interleave";
    } else {
        result.reason = "set-mempolicy-failed";
    };
#else
    result.status = ENOTSUP;
#endif
    return result;
}

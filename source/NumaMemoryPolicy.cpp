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

int countNodes(const std::vector<unsigned long> &mask)
{
    int count = 0;
    for (std::vector<unsigned long>::const_iterator word = mask.begin();
         word != mask.end();
         ++word) {
        count += __builtin_popcountl(*word);
    };
    return count;
}

void allocateNodeMask(std::vector<unsigned long> &mask)
{
    const unsigned long bitsPerWord = sizeof(unsigned long) * 8;
    mask.assign(
        (maximumNumaNodes + bitsPerWord - 1) / bitsPerWord,
        0
    );
}

int allowedMemoryNodes(std::vector<unsigned long> &mask)
{
    allocateNodeMask(mask);
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
    return countNodes(mask);
}

int inheritedMemoryPolicy(
    int &mode,
    std::vector<unsigned long> &mask
)
{
    allocateNodeMask(mask);
    mode = MPOL_DEFAULT;
    if (syscall(
            SYS_get_mempolicy,
            &mode,
            mask.data(),
            maximumNumaNodes,
            NULL,
            0
        ) != 0) {
        return errno == 0 ? EIO : errno;
    };
    return 0;
}

int basePolicyMode(int mode)
{
#if defined(MPOL_F_STATIC_NODES) && defined(MPOL_F_RELATIVE_NODES)
    return mode & ~(MPOL_F_STATIC_NODES | MPOL_F_RELATIVE_NODES);
#else
    return mode;
#endif
}

BlackstarNumaInheritedPolicy classifyInheritedPolicy(int mode)
{
    switch (basePolicyMode(mode)) {
        case MPOL_DEFAULT:
            return BlackstarNumaInheritedDefault;
        case MPOL_INTERLEAVE:
            return BlackstarNumaInheritedInterleave;
        default:
            return BlackstarNumaInheritedOther;
    };
}

std::string inheritedPolicyName(int mode)
{
    switch (basePolicyMode(mode)) {
        case MPOL_DEFAULT:
            return "Default";
        case MPOL_PREFERRED:
            return "Preferred";
        case MPOL_BIND:
            return "Bind";
        case MPOL_INTERLEAVE:
            return "Interleave";
#ifdef MPOL_LOCAL
        case MPOL_LOCAL:
            return "Local";
#endif
#ifdef MPOL_PREFERRED_MANY
        case MPOL_PREFERRED_MANY:
            return "PreferredMany";
#endif
        default:
            return "Unknown";
    };
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
    bool platformSupported,
    BlackstarNumaInheritedPolicy inheritedPolicy
)
{
    if (requested != "Auto" &&
        requested != "Default" &&
        requested != "Interleave") {
        return {false, false, false, "invalid-request"};
    };
    if (requested == "Default") {
        return {true, false, false, "explicit-default"};
    };
    if (runMode != "alignReads") {
        return {true, false, false, "not-align-reads"};
    };
    if (genomeLoad != "NoSharedMemory") {
        return {true, false, false, "shared-genome"};
    };
    if (!platformSupported) {
        return {true, false, false, "platform-unsupported"};
    };
    if (allowedNodeCount < 1) {
        return {true, false, false, "allowed-nodes-unavailable"};
    };
    if (requested == "Auto" && runThreads < automaticThreadThreshold) {
        return {true, false, false, "below-thread-threshold"};
    };
    if (requested == "Auto" && allowedNodeCount < 2) {
        return {true, false, false, "single-memory-node"};
    };
    if (requested == "Auto" &&
        inheritedPolicy == BlackstarNumaInheritedUnknown) {
        return {true, false, false, "inherited-policy-unavailable"};
    };
    if (requested == "Auto" &&
        inheritedPolicy == BlackstarNumaInheritedOther) {
        return {true, false, false, "inherited-policy-preserved"};
    };
    if (requested == "Auto" &&
        inheritedPolicy == BlackstarNumaInheritedInterleave) {
        return {true, true, false, "inherited-interleave"};
    };
    return {
        true,
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
        -1,
        0,
        requested,
        "Default",
        "NotQueried",
        "not-evaluated"
    };

    std::vector<unsigned long> allowedMask;
    BlackstarNumaInheritedPolicy inheritedPolicy =
        BlackstarNumaInheritedUnknown;
    bool platformSupported = false;
#if defined(__linux__) && defined(SYS_get_mempolicy) && defined(SYS_set_mempolicy)
    platformSupported = true;
    if (requested != "Default" &&
        runMode == "alignReads" &&
        genomeLoad == "NoSharedMemory") {
        result.allowedNodeCount = allowedMemoryNodes(allowedMask);
        if (result.allowedNodeCount < 0) {
            result.status = errno == 0 ? EIO : errno;
        } else if (requested == "Auto") {
            int inheritedMode = MPOL_DEFAULT;
            std::vector<unsigned long> inheritedMask;
            const int inheritedStatus =
                inheritedMemoryPolicy(inheritedMode, inheritedMask);
            if (inheritedStatus == 0) {
                inheritedPolicy = classifyInheritedPolicy(inheritedMode);
                result.inherited = inheritedPolicyName(inheritedMode);
                result.inheritedNodeCount = countNodes(inheritedMask);
            } else {
                result.status = inheritedStatus;
                result.inherited = "Unavailable";
            };
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
            platformSupported,
            inheritedPolicy
        );
    result.reason = choice.reason;
    if (!choice.valid) {
        result.status = EINVAL;
        return result;
    };
    if (!choice.interleave) {
        if (choice.reason == "inherited-policy-preserved") {
            result.effective = result.inherited;
        };
        return result;
    };
    if (!choice.applyInterleave) {
        result.active = true;
        result.effective = "Interleave";
        return result;
    };
    if (result.status != 0) {
        result.reason =
            result.allowedNodeCount < 0
                ? "allowed-nodes-query-failed"
                : "inherited-policy-query-failed";
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

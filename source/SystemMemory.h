#ifndef SYSTEM_MEMORY_H
#define SYSTEM_MEMORY_H

#include <cstdint>
#include <string>

struct SystemMemoryAvailability {
    std::uint64_t hostAvailableBytes;
    std::uint64_t cgroupAvailableBytes;
    std::uint64_t effectiveAvailableBytes;
    bool hostAvailableKnown;
    bool cgroupAvailableKnown;
    bool effectiveAvailableKnown;
};

SystemMemoryAvailability systemMemoryAvailability();

SystemMemoryAvailability systemMemoryAvailability(
    const std::string &memInfoPath,
    const std::string &selfCgroupPath,
    const std::string &cgroupRoot
);

#endif

#include "SystemMemory.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <sys/stat.h>

namespace {
bool parseUint64(const std::string &token, std::uint64_t &value)
{
    if (token.empty()) {
        return false;
    }

    value=0;
    for (std::string::const_iterator it=token.begin(); it!=token.end(); ++it) {
        if (*it<'0' || *it>'9') {
            return false;
        }
        const std::uint64_t digit=static_cast<std::uint64_t>(*it-'0');
        if (value>(std::numeric_limits<std::uint64_t>::max()-digit)/10) {
            return false;
        }
        value=value*10+digit;
    }
    return true;
}

bool readHostAvailable(const std::string &path, std::uint64_t &bytes)
{
    std::ifstream input(path.c_str());
    std::string field;
    std::string valueToken;
    std::string unit;
    while (input >> field >> valueToken >> unit) {
        if (field!="MemAvailable:") {
            continue;
        }

        std::uint64_t kibibytes=0;
        if (unit!="kB" || !parseUint64(valueToken, kibibytes) ||
            kibibytes>std::numeric_limits<std::uint64_t>::max()/1024) {
            return false;
        }
        bytes=kibibytes*1024;
        return true;
    }
    return false;
}

bool directoryExists(const std::string &path)
{
    struct stat status;
    return stat(path.c_str(), &status)==0 && S_ISDIR(status.st_mode);
}

bool readCgroupValue(
    const std::string &path,
    bool allowUnlimited,
    std::uint64_t &value,
    bool &unlimited
)
{
    std::ifstream input(path.c_str());
    std::string token;
    if (!(input >> token)) {
        return false;
    }
    if (allowUnlimited && token=="max") {
        unlimited=true;
        value=0;
        return true;
    }
    unlimited=false;
    return parseUint64(token, value);
}

std::string joinCgroupPath(
    const std::string &cgroupRoot,
    const std::string &cgroupPath
)
{
    std::string relative=cgroupPath;
    while (!relative.empty() && relative[0]=='/') {
        relative.erase(relative.begin());
    }
    return relative.empty() ? cgroupRoot : cgroupRoot+"/"+relative;
}

std::string parentPath(const std::string &path, const std::string &root)
{
    if (path==root) {
        return root;
    }
    const std::string::size_type slash=path.find_last_of('/');
    if (slash==std::string::npos || slash<root.size()) {
        return root;
    }
    return path.substr(0, slash);
}

bool controllerListContainsMemory(const std::string &controllers)
{
    std::string::size_type start=0;
    while (start<=controllers.size()) {
        const std::string::size_type end=controllers.find(',', start);
        const std::string controller=controllers.substr(
            start,
            end==std::string::npos ? std::string::npos : end-start
        );
        if (controller=="memory") {
            return true;
        }
        if (end==std::string::npos) {
            break;
        }
        start=end+1;
    }
    return false;
}

struct CgroupLocation {
    bool found;
    bool unified;
    std::string path;
};

CgroupLocation readCgroupLocation(const std::string &path)
{
    CgroupLocation unified={false, true, ""};
    CgroupLocation legacy={false, false, ""};
    std::ifstream input(path.c_str());
    std::string line;
    while (std::getline(input, line)) {
        const std::string::size_type firstColon=line.find(':');
        const std::string::size_type secondColon=firstColon==std::string::npos
            ? std::string::npos : line.find(':', firstColon+1);
        if (secondColon==std::string::npos) {
            continue;
        }

        const std::string hierarchy=line.substr(0, firstColon);
        const std::string controllers=line.substr(
            firstColon+1,
            secondColon-firstColon-1
        );
        const std::string cgroupPath=line.substr(secondColon+1);
        if (hierarchy=="0" && controllers.empty()) {
            unified.found=true;
            unified.path=cgroupPath;
        } else if (controllerListContainsMemory(controllers)) {
            legacy.found=true;
            legacy.path=cgroupPath;
        }
    }
    return unified.found ? unified : legacy;
}

bool cgroupAvailableAt(
    const std::string &start,
    const std::string &root,
    bool unified,
    std::uint64_t &available
)
{
    const std::string limitName=unified ? "memory.max" : "memory.limit_in_bytes";
    const std::string usageName=unified ? "memory.current" : "memory.usage_in_bytes";
    bool foundFiniteLimit=false;
    std::uint64_t tightest=std::numeric_limits<std::uint64_t>::max();
    std::string current=start;

    while (true) {
        std::uint64_t limit=0;
        std::uint64_t usage=0;
        bool unlimited=false;
        bool usageUnlimited=false;
        const bool limitRead=readCgroupValue(
            current+"/"+limitName,
            unified,
            limit,
            unlimited
        );
        const bool usageRead=readCgroupValue(
            current+"/"+usageName,
            false,
            usage,
            usageUnlimited
        );
        const bool legacyUnlimited=!unified && limit>=(1ULL<<60);
        if (limitRead && !unlimited && !legacyUnlimited) {
            // A finite limit without a readable usage value is not safe to
            // treat as available capacity for an optional allocation.
            const std::uint64_t remaining=usageRead && usage<limit
                ? limit-usage : 0;
            tightest=std::min(tightest, remaining);
            foundFiniteLimit=true;
        }

        if (current==root) {
            break;
        }
        current=parentPath(current, root);
    }

    if (foundFiniteLimit) {
        available=tightest;
    }
    return foundFiniteLimit;
}

bool readCgroupAvailable(
    const std::string &selfCgroupPath,
    const std::string &cgroupRoot,
    std::uint64_t &available
)
{
    const CgroupLocation location=readCgroupLocation(selfCgroupPath);
    if (!location.found) {
        return false;
    }

    const std::string hierarchyRoot=location.unified
        ? cgroupRoot : cgroupRoot+"/memory";
    std::string start=joinCgroupPath(hierarchyRoot, location.path);
    if (!directoryExists(start)) {
        start=hierarchyRoot;
    }
    if (!directoryExists(start)) {
        return false;
    }
    return cgroupAvailableAt(start, hierarchyRoot, location.unified, available);
}
}

SystemMemoryAvailability systemMemoryAvailability(
    const std::string &memInfoPath,
    const std::string &selfCgroupPath,
    const std::string &cgroupRoot
)
{
    SystemMemoryAvailability result={0, 0, 0, false, false, false};
    result.hostAvailableKnown=readHostAvailable(
        memInfoPath,
        result.hostAvailableBytes
    );
    result.cgroupAvailableKnown=readCgroupAvailable(
        selfCgroupPath,
        cgroupRoot,
        result.cgroupAvailableBytes
    );

    if (result.hostAvailableKnown && result.cgroupAvailableKnown) {
        result.effectiveAvailableBytes=std::min(
            result.hostAvailableBytes,
            result.cgroupAvailableBytes
        );
        result.effectiveAvailableKnown=true;
    } else if (result.hostAvailableKnown) {
        result.effectiveAvailableBytes=result.hostAvailableBytes;
        result.effectiveAvailableKnown=true;
    } else if (result.cgroupAvailableKnown) {
        result.effectiveAvailableBytes=result.cgroupAvailableBytes;
        result.effectiveAvailableKnown=true;
    }
    return result;
}

SystemMemoryAvailability systemMemoryAvailability()
{
#ifdef __linux__
    return systemMemoryAvailability(
        "/proc/meminfo",
        "/proc/self/cgroup",
        "/sys/fs/cgroup"
    );
#else
    return SystemMemoryAvailability{0, 0, 0, false, false, false};
#endif
}

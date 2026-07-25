#include "SystemMemory.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>

namespace {
void require(bool condition, const std::string &message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void makeDirectory(const std::string &path)
{
    if (mkdir(path.c_str(), 0700)!=0) {
        throw std::runtime_error("could not create fixture directory "+path);
    }
}

void writeFile(const std::string &path, const std::string &contents)
{
    std::ofstream output(path.c_str());
    if (!output.good()) {
        throw std::runtime_error("could not create fixture file "+path);
    }
    output << contents;
}

void makeProcTree(const std::string &root)
{
    makeDirectory(root);
    makeDirectory(root+"/proc");
    makeDirectory(root+"/proc/self");
    makeDirectory(root+"/cgroup");
}

SystemMemoryAvailability inspect(const std::string &root)
{
    return systemMemoryAvailability(
        root+"/proc/meminfo",
        root+"/proc/self/cgroup",
        root+"/cgroup"
    );
}
}

int main(int argc, char **argv)
{
    if (argc!=2) {
        std::cerr << "usage: testSystemMemory ROOT\n";
        return 2;
    }

    try {
        const std::string base=argv[1];
        makeDirectory(base);

        const std::string hostOnly=base+"/host-only";
        makeProcTree(hostOnly);
        writeFile(hostOnly+"/proc/meminfo", "MemAvailable: 1000 kB\n");
        writeFile(hostOnly+"/proc/self/cgroup", "0::/\n");
        writeFile(hostOnly+"/cgroup/memory.max", "max\n");
        writeFile(hostOnly+"/cgroup/memory.current", "500\n");
        SystemMemoryAvailability result=inspect(hostOnly);
        require(result.hostAvailableKnown, "host MemAvailable was not detected");
        require(!result.cgroupAvailableKnown, "unlimited cgroup was treated as finite");
        require(result.effectiveAvailableBytes==1024000, "host-only bound is wrong");

        const std::string unified=base+"/unified";
        makeProcTree(unified);
        makeDirectory(unified+"/cgroup/team");
        makeDirectory(unified+"/cgroup/team/job");
        writeFile(unified+"/proc/meminfo", "MemAvailable: 10000 kB\n");
        writeFile(unified+"/proc/self/cgroup", "0::/team/job\n");
        writeFile(unified+"/cgroup/memory.max", "max\n");
        writeFile(unified+"/cgroup/memory.current", "1\n");
        writeFile(unified+"/cgroup/team/memory.max", "10000\n");
        writeFile(unified+"/cgroup/team/memory.current", "7000\n");
        writeFile(unified+"/cgroup/team/job/memory.max", "6000\n");
        writeFile(unified+"/cgroup/team/job/memory.current", "1000\n");
        result=inspect(unified);
        require(result.cgroupAvailableKnown, "cgroup v2 bound was not detected");
        require(result.cgroupAvailableBytes==3000, "ancestor cgroup v2 bound is wrong");
        require(result.effectiveAvailableBytes==3000, "effective cgroup v2 bound is wrong");

        const std::string exhausted=base+"/exhausted";
        makeProcTree(exhausted);
        writeFile(exhausted+"/proc/meminfo", "MemAvailable: 10000 kB\n");
        writeFile(exhausted+"/proc/self/cgroup", "0::/\n");
        writeFile(exhausted+"/cgroup/memory.max", "1000\n");
        writeFile(exhausted+"/cgroup/memory.current", "1200\n");
        result=inspect(exhausted);
        require(result.cgroupAvailableKnown, "exhausted cgroup was not detected");
        require(result.effectiveAvailableBytes==0, "exhausted cgroup did not clamp to zero");

        const std::string unreadableUsage=base+"/unreadable-usage";
        makeProcTree(unreadableUsage);
        writeFile(unreadableUsage+"/proc/meminfo", "MemAvailable: 10000 kB\n");
        writeFile(unreadableUsage+"/proc/self/cgroup", "0::/\n");
        writeFile(unreadableUsage+"/cgroup/memory.max", "1000\n");
        result=inspect(unreadableUsage);
        require(result.cgroupAvailableKnown, "finite cgroup with missing usage was ignored");
        require(result.effectiveAvailableBytes==0, "missing cgroup usage did not fail closed");

        const std::string legacy=base+"/legacy";
        makeProcTree(legacy);
        makeDirectory(legacy+"/cgroup/memory");
        makeDirectory(legacy+"/cgroup/memory/team");
        makeDirectory(legacy+"/cgroup/memory/team/job");
        writeFile(legacy+"/proc/meminfo", "MemAvailable: 10000 kB\n");
        writeFile(legacy+"/proc/self/cgroup", "5:cpu,memory:/team/job\n");
        writeFile(legacy+"/cgroup/memory/memory.limit_in_bytes", "1152921504606846976\n");
        writeFile(legacy+"/cgroup/memory/memory.usage_in_bytes", "1\n");
        writeFile(legacy+"/cgroup/memory/team/memory.limit_in_bytes", "10000\n");
        writeFile(legacy+"/cgroup/memory/team/memory.usage_in_bytes", "6000\n");
        writeFile(legacy+"/cgroup/memory/team/job/memory.limit_in_bytes", "8000\n");
        writeFile(legacy+"/cgroup/memory/team/job/memory.usage_in_bytes", "2500\n");
        result=inspect(legacy);
        require(result.cgroupAvailableKnown, "cgroup v1 bound was not detected");
        require(result.cgroupAvailableBytes==4000, "ancestor cgroup v1 bound is wrong");
        require(result.effectiveAvailableBytes==4000, "effective cgroup v1 bound is wrong");

        const std::string malformed=base+"/malformed";
        makeProcTree(malformed);
        writeFile(malformed+"/proc/meminfo", "MemAvailable: invalid kB\n");
        writeFile(malformed+"/proc/self/cgroup", "not-a-cgroup-record\n");
        result=inspect(malformed);
        require(!result.effectiveAvailableKnown, "malformed sources produced a memory bound");
    } catch (const std::exception &error) {
        std::cerr << "system memory tests failed: " << error.what() << "\n";
        return 1;
    }

    std::cout << "system memory tests passed\n";
    return 0;
}

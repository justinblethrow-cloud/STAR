#include "AlignmentThreadAffinity.h"

#include <pthread.h>
#include <sched.h>
#include <unistd.h>

#include <iostream>

namespace {
int affinityCpuCount() {
    const long configuredCpus = sysconf(_SC_NPROCESSORS_CONF);
    if (configuredCpus <= 0) {
        return -1;
    }
    const size_t cpuSetSize = CPU_ALLOC_SIZE(configuredCpus);
    cpu_set_t *cpuSet = CPU_ALLOC(configuredCpus);
    if (cpuSet == NULL) {
        return -1;
    }
    CPU_ZERO_S(cpuSetSize, cpuSet);
    const int status = pthread_getaffinity_np(pthread_self(), cpuSetSize, cpuSet);
    const int count = status == 0 ? CPU_COUNT_S(cpuSetSize, cpuSet) : -1;
    CPU_FREE(cpuSet);
    return count;
}

void *captureChildAffinity(void *value) {
    *static_cast<int *>(value) = affinityCpuCount();
    return NULL;
}
}

int main() {
    const int before = affinityCpuCount();
    const AlignmentThreadAffinityResult result =
        alignmentThreadAffinityRestoreOpenMpPlaces();
    const int after = affinityCpuCount();

    int child = -1;
    pthread_t thread;
    if (pthread_create(&thread, NULL, captureChildAffinity, &child) != 0 ||
        pthread_join(thread, NULL) != 0) {
        return 2;
    }

    std::cout << "binding_active\t" << result.bindingActive << '\n'
              << "place_count\t" << result.placeCount << '\n'
              << "reported_cpu_count\t" << result.cpuCount << '\n'
              << "status\t" << result.status << '\n'
              << "before_cpu_count\t" << before << '\n'
              << "after_cpu_count\t" << after << '\n'
              << "child_cpu_count\t" << child << '\n';

    if (before <= 0 || after <= 0 || child != after || result.status != 0) {
        return 1;
    }
    if (result.bindingActive) {
        return result.placeCount > 0 && result.cpuCount == after && after >= before
            ? 0 : 1;
    }
    return result.placeCount == 0 && result.cpuCount == 0 && after == before
        ? 0 : 1;
}

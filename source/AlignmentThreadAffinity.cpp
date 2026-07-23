#include "AlignmentThreadAffinity.h"
#include "IncludeDefine.h"

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

AlignmentThreadAffinityResult::AlignmentThreadAffinityResult()
    : bindingActive(false), placeCount(0), cpuCount(0), status(0) {
}

AlignmentThreadAffinityResult alignmentThreadAffinityRestoreOpenMpPlaces() {
    AlignmentThreadAffinityResult result;

#if defined(__linux__) && defined(_OPENMP) && _OPENMP >= 201307
    result.bindingActive = omp_get_proc_bind() != omp_proc_bind_false;
    if (!result.bindingActive) {
        return result;
    };

    result.placeCount = omp_get_num_places();
    if (result.placeCount <= 0) {
        result.status = EINVAL;
        return result;
    };

    vector<int> cpus;
    for (int place = 0; place < result.placeCount; place++) {
        const int placeCpuCount = omp_get_place_num_procs(place);
        if (placeCpuCount <= 0) {
            continue;
        };
        vector<int> placeCpus(static_cast<size_t>(placeCpuCount));
        omp_get_place_proc_ids(place, placeCpus.data());
        cpus.insert(cpus.end(), placeCpus.begin(), placeCpus.end());
    };

    sort(cpus.begin(), cpus.end());
    cpus.erase(unique(cpus.begin(), cpus.end()), cpus.end());
    if (cpus.empty() || cpus.front() < 0) {
        result.status = EINVAL;
        return result;
    };
    result.cpuCount = static_cast<int>(cpus.size());

#if defined(CPU_ALLOC)
    const int maxCpu = cpus.back();
    const size_t cpuSetSize = CPU_ALLOC_SIZE(maxCpu + 1);
    cpu_set_t *cpuSet = CPU_ALLOC(maxCpu + 1);
    if (cpuSet == NULL) {
        result.status = ENOMEM;
        return result;
    };
    CPU_ZERO_S(cpuSetSize, cpuSet);
    for (vector<int>::const_iterator cpu = cpus.begin(); cpu != cpus.end(); ++cpu) {
        CPU_SET_S(*cpu, cpuSetSize, cpuSet);
    };
    result.status = pthread_setaffinity_np(pthread_self(), cpuSetSize, cpuSet);
    CPU_FREE(cpuSet);
#else
    if (cpus.back() >= CPU_SETSIZE) {
        result.status = E2BIG;
        return result;
    };
    cpu_set_t cpuSet;
    CPU_ZERO(&cpuSet);
    for (vector<int>::const_iterator cpu = cpus.begin(); cpu != cpus.end(); ++cpu) {
        CPU_SET(*cpu, &cpuSet);
    };
    result.status = pthread_setaffinity_np(pthread_self(), sizeof(cpuSet), &cpuSet);
#endif
#endif

    return result;
}

#ifndef CODE_AlignmentThreadAffinity
#define CODE_AlignmentThreadAffinity

struct AlignmentThreadAffinityResult {
    bool bindingActive;
    int placeCount;
    int cpuCount;
    int status;

    AlignmentThreadAffinityResult();
};

// OpenMP may bind STAR's initial thread before main(), causing subsequently
// created alignment pthreads to inherit a single OpenMP place.
AlignmentThreadAffinityResult alignmentThreadAffinityRestoreOpenMpPlaces();

#endif

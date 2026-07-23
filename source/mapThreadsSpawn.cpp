#include "mapThreadsSpawn.h"
#include "ThreadControl.h"
#include "GlobalVariables.h"
#include "ErrorWarning.h"
#include "AlignmentThreadAffinity.h"

void mapThreadsSpawn (Parameters &P, ReadAlignChunk** RAchunk) {
    if (P.runThreadN > 1) {
        const AlignmentThreadAffinityResult affinity =
            alignmentThreadAffinityRestoreOpenMpPlaces();
        if (affinity.bindingActive) {
            P.inOut->logMain << "BLACKSTAR_ALIGN_AFFINITY_RECOVERY"
                             << "\tplaces\t" << affinity.placeCount
                             << "\tcpus\t" << affinity.cpuCount
                             << "\tstatus\t" << affinity.status << '\n' << flush;
            if (affinity.status != 0) {
                ostringstream errOut;
                errOut << "EXITING because OpenMP processor binding restricted the "
                       << "alignment pthread pool and the full OpenMP place set could "
                       << "not be restored, error code: " << affinity.status;
                exitWithError(errOut.str(), std::cerr, P.inOut->logMain,
                              EXIT_CODE_RUNTIME, P);
            };
        };
    };
    for (int ithread=1;ithread<P.runThreadN;ithread++) {//spawn threads
        int threadStatus=pthread_create(&g_threadChunks.threadArray[ithread], NULL, &g_threadChunks.threadRAprocessChunks, (void *) RAchunk[ithread]);
        if (threadStatus>0) {//something went wrong with one of threads
                ostringstream errOut;
                errOut << "EXITING because of FATAL ERROR: phtread error while creating thread # " << ithread <<", error code: "<<threadStatus ;
                exitWithError(errOut.str(),std::cerr, P.inOut->logMain, 1, P);
        };
        pthread_mutex_lock(&g_threadChunks.mutexLogMain);
        P.inOut->logMain << "Created thread # " <<ithread <<"\n"<<flush;
        pthread_mutex_unlock(&g_threadChunks.mutexLogMain);
    };

    RAchunk[0]->processChunks(); //start main thread

    for (int ithread=1;ithread<P.runThreadN;ithread++) {//wait for all threads to complete
        int threadStatus = pthread_join(g_threadChunks.threadArray[ithread], NULL);
        if (threadStatus>0) {//something went wrong with one of threads
                ostringstream errOut;
                errOut << "EXITING because of FATAL ERROR: phtread error while joining thread # " << ithread <<", error code: "<<threadStatus ;
                exitWithError(errOut.str(),std::cerr, P.inOut->logMain, 1, P);
        };
        pthread_mutex_lock(&g_threadChunks.mutexLogMain);
        P.inOut->logMain << "Joined thread # " <<ithread <<"\n"<<flush;
        pthread_mutex_unlock(&g_threadChunks.mutexLogMain);
    };
};

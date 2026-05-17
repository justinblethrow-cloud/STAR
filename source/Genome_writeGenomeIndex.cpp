#include "Genome.h"
#include "TimeFunctions.h"
#include "genomeParametersWrite.h"
#include "streamFuns.h"

#include <cstdio>
#include <sys/stat.h>

namespace {
bool fileExists(const string &fileName)
{
    struct stat statBuf;
    return stat(fileName.c_str(), &statBuf)==0 && S_ISREG(statBuf.st_mode);
}

void copyIfExists(const string &dirIn, const string &dirOut, const string &fileName)
{
    const string pathIn = dirIn + "/" + fileName;
    const string pathOut = dirOut + "/" + fileName;
    remove(pathOut.c_str());
    if (fileExists(pathIn)) {
        copyFile(pathIn, pathOut);
    };
}
}

void Genome::writeGenomeIndex(const string dirOut)
{
    string dirOut1=dirOut;
    if (dirOut1.back()=='/') {
        dirOut1.erase(dirOut1.end()-1);
    };

    createDirectory(dirOut1+"/", P.runDirPerm, "--genomeInsertOutDir", P);

    // Preserve annotation sidecar files from the base index. genomeInsert adds
    // named sequences, but does not add new GTF-derived annotations.
    const vector<string> auxiliaryFiles = {
        "sjdbInfo.txt",
        "sjdbList.out.tab",
        "sjdbList.fromGTF.out.tab",
        "exonInfo.tab",
        "exonGeTrInfo.tab",
        "geneInfo.tab",
        "transcriptInfo.tab"
    };
    for (auto &fileName : auxiliaryFiles) {
        copyIfExists(pGe.gDir, dirOut1, fileName);
    };

    writeChrInfo(dirOut1);

    pGe.gFileSizes.clear();
    pGe.gFileSizes.push_back(nGenome);
    pGe.gFileSizes.push_back(SA.lengthByte);

    // Record both the loaded base FASTA files and the inserted FASTA files in
    // genomeParameters.txt, while leaving the in-memory parameters unchanged.
    vector<string> genomeFastaFilesSaved=pGe.gFastaFiles;
    if (genomeInsertL>0 && genomeFastaFilesSaved.at(0)!="-" && genomeFastaFilesLoaded.size()>0) {
        pGe.gFastaFiles=genomeFastaFilesLoaded;
        if (pGe.gFastaFiles.size()==1 && pGe.gFastaFiles.at(0)=="-") {
            pGe.gFastaFiles.clear();
        };
        pGe.gFastaFiles.insert(pGe.gFastaFiles.end(), genomeFastaFilesSaved.begin(), genomeFastaFilesSaved.end());
    };

    genomeParametersWrite(dirOut1+"/genomeParameters.txt", P, ERROR_OUT, *this);
    pGe.gFastaFiles=genomeFastaFilesSaved;

    time_t rawTime;
    time(&rawTime);
    P.inOut->logMain    << timeMonthDayTime(rawTime) <<" ... writing Genome to disk ...\n" <<flush;
    *P.inOut->logStdOut << timeMonthDayTime(rawTime) <<" ... writing Genome to disk ...\n" <<flush;
    writeGenomeSequence(dirOut1);

    time(&rawTime);
    P.inOut->logMain    << timeMonthDayTime(rawTime) <<" ... writing Suffix Array to disk ...\n" <<flush;
    *P.inOut->logStdOut << timeMonthDayTime(rawTime) <<" ... writing Suffix Array to disk ...\n" <<flush;
    ofstream &saOut = ofstrOpen(dirOut1+"/SA", ERROR_OUT, P);
    fstreamWriteBig(saOut, (char*) SA.charArray, (streamsize) SA.lengthByte, dirOut1+"/SA", ERROR_OUT, P);
    saOut.close();

    time(&rawTime);
    P.inOut->logMain    << timeMonthDayTime(rawTime) <<" ... writing SAindex to disk\n" <<flush;
    *P.inOut->logStdOut << timeMonthDayTime(rawTime) <<" ... writing SAindex to disk\n" <<flush;
    ofstream &saIndexOut = ofstrOpen(dirOut1+"/SAindex", ERROR_OUT, P);
    fstreamWriteBig(saIndexOut, (char*) &pGe.gSAindexNbases, sizeof(pGe.gSAindexNbases), dirOut1+"/SAindex", ERROR_OUT, P);
    fstreamWriteBig(saIndexOut, (char*) genomeSAindexStart, sizeof(genomeSAindexStart[0])*(pGe.gSAindexNbases+1), dirOut1+"/SAindex", ERROR_OUT, P);
    fstreamWriteBig(saIndexOut, SAi.charArray, SAi.lengthByte, dirOut1+"/SAindex", ERROR_OUT, P);
    saIndexOut.close();

    time(&rawTime);
    P.inOut->logMain    << timeMonthDayTime(rawTime) << " ..... finished writing updated genome index\n" <<flush;
    *P.inOut->logStdOut << timeMonthDayTime(rawTime) << " ..... finished writing updated genome index\n" <<flush;
};

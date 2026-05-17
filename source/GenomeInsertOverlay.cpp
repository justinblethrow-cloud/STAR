#include "GenomeInsertOverlay.h"
#include "Parameters.h"
#include "ErrorWarning.h"
#include "streamFuns.h"

#include <cstdlib>
#include <limits.h>
#include <sys/stat.h>
#include <unordered_set>

namespace {
const string overlayFileName = "genomeInsertOverlay.tsv";
const string deltaFileName = "genomeInsertDelta.bin";

struct GenomeInsertOverlay {
    string baseGenomeDir;
    vector<string> genomeFastaFiles;
    string sjdbGTFfile;
    string deltaFile;
    uint sjdbOverhang;
};

string stripTrailingSlash(string path)
{
    while (path.size()>1 && path.back()=='/') {
        path.erase(path.end()-1);
    };
    return path;
}

string addTrailingSlash(string path)
{
    if (path.back()!='/') {
        path += '/';
    };
    return path;
}

string manifestPath(const string &dir)
{
    return stripTrailingSlash(dir) + "/" + overlayFileName;
}

bool fileExists(const string &fileName)
{
    struct stat statBuf;
    return stat(fileName.c_str(), &statBuf)==0 && S_ISREG(statBuf.st_mode);
}

string absoluteExistingPath(const string &path, const string &label, Parameters &P)
{
    char resolvedPath[PATH_MAX];
    if (realpath(path.c_str(), resolvedPath)==NULL) {
        ostringstream errOut;
        errOut << "EXITING because of fatal INPUT FILE error: could not resolve " << label << " path " << path << "\n";
        exitWithError(errOut.str(), std::cerr, P.inOut->logMain, EXIT_CODE_INPUT_FILES, P);
    };
    return string(resolvedPath);
}

string resolveManifestPath(const string &path, const string &overlayDir)
{
    if (path=="-" || path.size()==0 || path.at(0)=='/') {
        return path;
    };
    return stripTrailingSlash(overlayDir) + "/" + path;
}

unordered_set<string> readFastaNames(const vector<string> &fastaFiles, Parameters &P)
{
    unordered_set<string> names;
    for (auto &fastaFile : fastaFiles) {
        ifstream fastaIn(fastaFile.c_str());
        if (fastaIn.fail()) {
            ostringstream errOut;
            errOut << "EXITING because of fatal INPUT FILE error: could not open inserted FASTA file " << fastaFile << "\n";
            exitWithError(errOut.str(), std::cerr, P.inOut->logMain, EXIT_CODE_INPUT_FILES, P);
        };

        string line;
        while (getline(fastaIn, line)) {
            if (line.size()==0 || line.at(0)!='>') {
                continue;
            };
            istringstream nameStream(line.substr(1));
            string name;
            nameStream >> name;
            if (name.size()==0) {
                ostringstream errOut;
                errOut << "EXITING because of fatal INPUT FILE error: empty sequence name in inserted FASTA file " << fastaFile << "\n";
                exitWithError(errOut.str(), std::cerr, P.inOut->logMain, EXIT_CODE_INPUT_FILES, P);
            };
            names.insert(name);
        };
    };

    if (names.size()==0) {
        ostringstream errOut;
        errOut << "EXITING because of fatal INPUT FILE error: no sequence names were found in inserted FASTA files\n";
        exitWithError(errOut.str(), std::cerr, P.inOut->logMain, EXIT_CODE_INPUT_FILES, P);
    };
    return names;
}

void validateOverlayGTF(const string &gtfFile, const unordered_set<string> &insertedNames, Parameters &P)
{
    if (gtfFile=="-") {
        return;
    };

    ifstream gtfIn(gtfFile.c_str());
    if (gtfIn.fail()) {
        ostringstream errOut;
        errOut << "EXITING because of fatal INPUT FILE error: could not open inserted GTF file " << gtfFile << "\n";
        exitWithError(errOut.str(), std::cerr, P.inOut->logMain, EXIT_CODE_INPUT_FILES, P);
    };

    string line;
    while (getline(gtfIn, line)) {
        if (line.size()==0 || line.at(0)=='#') {
            continue;
        };
        size_t fieldEnd=line.find('\t');
        if (fieldEnd==string::npos) {
            continue;
        };
        string chrName=line.substr(0, fieldEnd);
        if (insertedNames.find(chrName)==insertedNames.end()) {
            ostringstream errOut;
            errOut << "EXITING because of fatal INPUT FILE error: --runMode genomeInsert with overlay output modes expects --sjdbGTFfile to contain only annotations for inserted sequences\n";
            errOut << "Offending GTF chromosome: " << chrName << "\n";
            exitWithError(errOut.str(), std::cerr, P.inOut->logMain, EXIT_CODE_INPUT_FILES, P);
        };
    };
}

GenomeInsertOverlay readOverlay(const string &overlayDir, Parameters &P)
{
    GenomeInsertOverlay overlay;
    overlay.sjdbGTFfile="-";
    overlay.deltaFile="-";
    overlay.sjdbOverhang=0;

    const string path=manifestPath(overlayDir);
    ifstream fileIn(path.c_str());
    if (fileIn.fail()) {
        ostringstream errOut;
        errOut << "EXITING because of fatal INPUT FILE error: could not open genome insert overlay file " << path << "\n";
        exitWithError(errOut.str(), std::cerr, P.inOut->logMain, EXIT_CODE_INPUT_FILES, P);
    };

    string line;
    while (getline(fileIn, line)) {
        if (line.size()==0 || line.at(0)=='#') {
            continue;
        };

        istringstream lineStream(line);
        string key;
        lineStream >> key;

        if (key=="genomeInsertOverlayVersion") {
            uint version=0;
            lineStream >> version;
            if (version!=1) {
                ostringstream errOut;
                errOut << "EXITING because of fatal INPUT FILE error: unsupported genome insert overlay version " << version << " in " << path << "\n";
                exitWithError(errOut.str(), std::cerr, P.inOut->logMain, EXIT_CODE_INPUT_FILES, P);
            };
        } else if (key=="baseGenomeDir") {
            lineStream >> overlay.baseGenomeDir;
            overlay.baseGenomeDir=addTrailingSlash(resolveManifestPath(overlay.baseGenomeDir, overlayDir));
        } else if (key=="genomeFastaFiles") {
            string fastaFile;
            while (lineStream >> fastaFile) {
                overlay.genomeFastaFiles.push_back(resolveManifestPath(fastaFile, overlayDir));
            };
        } else if (key=="sjdbGTFfile") {
            lineStream >> overlay.sjdbGTFfile;
            overlay.sjdbGTFfile=resolveManifestPath(overlay.sjdbGTFfile, overlayDir);
        } else if (key=="genomeInsertDeltaFile") {
            lineStream >> overlay.deltaFile;
            overlay.deltaFile=resolveManifestPath(overlay.deltaFile, overlayDir);
        } else if (key=="sjdbOverhang") {
            lineStream >> overlay.sjdbOverhang;
        };
    };

    if (overlay.baseGenomeDir.size()==0 || overlay.genomeFastaFiles.size()==0) {
        ostringstream errOut;
        errOut << "EXITING because of fatal INPUT FILE error: malformed genome insert overlay file " << path << "\n";
        exitWithError(errOut.str(), std::cerr, P.inOut->logMain, EXIT_CODE_INPUT_FILES, P);
    };

    return overlay;
}
}

string genomeInsertDeltaFilePath(const string &dir)
{
    return stripTrailingSlash(dir) + "/" + deltaFileName;
}

bool genomeInsertOverlayLoad(Parameters &P)
{
    const string overlayDir=P.pGe.gDir;
    const string path=manifestPath(overlayDir);
    if (!fileExists(path)) {
        return false;
    };

    if (P.runModeIn.at(0)!="alignReads") {
        ostringstream errOut;
        errOut << "EXITING because of fatal INPUT FILE error: genome insert overlay directories can only be used with --runMode alignReads\n";
        exitWithError(errOut.str(), std::cerr, P.inOut->logMain, EXIT_CODE_INPUT_FILES, P);
    };
    if (P.pGe.gLoad!="NoSharedMemory") {
        ostringstream errOut;
        errOut << "EXITING because of fatal PARAMETERS error: genome insert overlay loading requires --genomeLoad NoSharedMemory\n";
        exitWithError(errOut.str(), std::cerr, P.inOut->logMain, EXIT_CODE_PARAMETER, P);
    };
    if (P.pGe.gFastaFiles.at(0)!="-") {
        ostringstream errOut;
        errOut << "EXITING because of fatal PARAMETERS error: genome insert overlay cannot be combined with user-defined --genomeFastaFiles\n";
        exitWithError(errOut.str(), std::cerr, P.inOut->logMain, EXIT_CODE_PARAMETER, P);
    };
    if (P.pGe.sjdbGTFfile!="-") {
        ostringstream errOut;
        errOut << "EXITING because of fatal PARAMETERS error: genome insert overlay cannot be combined with user-defined --sjdbGTFfile\n";
        exitWithError(errOut.str(), std::cerr, P.inOut->logMain, EXIT_CODE_PARAMETER, P);
    };

    GenomeInsertOverlay overlay=readOverlay(overlayDir, P);
    P.pGe.gDir=overlay.baseGenomeDir;
    P.pGe.gFastaFiles=overlay.genomeFastaFiles;
    P.pGe.sjdbGTFfile=overlay.sjdbGTFfile;
    P.pGe.gInsertOverlay=true;
    P.pGe.gInsertOverlayDeltaFile=overlay.deltaFile;
    if (P.pGe.gInsertOverlayDeltaFile!="-" && !fileExists(P.pGe.gInsertOverlayDeltaFile)) {
        ostringstream errOut;
        errOut << "EXITING because of fatal INPUT FILE error: could not find genome insert delta file " << P.pGe.gInsertOverlayDeltaFile << "\n";
        exitWithError(errOut.str(), std::cerr, P.inOut->logMain, EXIT_CODE_INPUT_FILES, P);
    };
    if (overlay.sjdbOverhang>0) {
        P.pGe.sjdbOverhang=overlay.sjdbOverhang;
    };

    P.inOut->logMain << "Genome insert overlay detected: " << path << "\n";
    P.inOut->logMain << "Genome insert overlay base genomeDir: " << P.pGe.gDir << "\n";
    return true;
}

void genomeInsertOverlayWrite(Parameters &P)
{
    string outDir=P.pGe.gInsertOutDir;
    createDirectory(outDir, P.runDirPerm, "--genomeInsertOutDir", P);

    vector<string> fastaFiles;
    for (auto &fastaFile : P.pGe.gFastaFiles) {
        fastaFiles.push_back(absoluteExistingPath(fastaFile, "inserted FASTA", P));
    };
    const string baseGenomeDir=addTrailingSlash(absoluteExistingPath(stripTrailingSlash(P.pGe.gDir), "base genomeDir", P));
    const string gtfFile=P.pGe.sjdbGTFfile=="-" ? "-" : absoluteExistingPath(P.pGe.sjdbGTFfile, "inserted GTF", P);

    unordered_set<string> fastaNames=readFastaNames(fastaFiles, P);
    validateOverlayGTF(gtfFile, fastaNames, P);

    const string path=manifestPath(outDir);
    ofstream &overlayOut=ofstrOpen(path, ERROR_OUT, P);
    overlayOut << "genomeInsertOverlayVersion\t1\n";
    overlayOut << "baseGenomeDir\t" << baseGenomeDir << "\n";
    overlayOut << "genomeFastaFiles";
    for (auto &fastaFile : fastaFiles) {
        overlayOut << "\t" << fastaFile;
    };
    overlayOut << "\n";
    overlayOut << "sjdbGTFfile\t" << gtfFile << "\n";
    if (P.pGe.gInsertOutMode=="Delta") {
        overlayOut << "genomeInsertDeltaFile\t" << deltaFileName << "\n";
    };
    overlayOut << "sjdbOverhang\t" << P.pGe.sjdbOverhang << "\n";
    overlayOut.close();

    P.inOut->logMain << "Genome insert overlay written: " << path << "\n";
};

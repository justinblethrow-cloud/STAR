#include "GenomeInsertIdentity.h"

#include "BlackstarSha256.h"
#include "ErrorWarning.h"
#include "Genome.h"
#include "Parameters.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <limits>
#include <sys/stat.h>

namespace {
const vector<string> requiredIdentityFiles = {
    "Genome",
    "SA",
    "SAindex",
    "genomeParameters.txt",
    "chrName.txt",
    "chrLength.txt",
    "chrStart.txt",
    "chrNameLength.txt"
};

const vector<string> optionalIdentityFiles = {
    "sjdbInfo.txt",
    "sjdbList.out.tab",
    "sjdbList.fromGTF.out.tab",
    "exonInfo.tab",
    "exonGeTrInfo.tab",
    "geneInfo.tab",
    "transcriptInfo.tab"
};

struct IdentityFile {
    string name;
    bool present;
    uint64 size;
    string digest;
};

string joinPath(const string &directory, const string &name)
{
    if (directory.size()>0 && directory.back()=='/') return directory+name;
    return directory+"/"+name;
}

bool regularFile(const string &path, uint64 *sizeOut)
{
    struct stat fileStat;
    if (stat(path.c_str(), &fileStat)!=0 || !S_ISREG(fileStat.st_mode) || fileStat.st_size<0) return false;
    if (sizeOut!=NULL) *sizeOut=static_cast<uint64>(fileStat.st_size);
    return true;
}

void updateUint64LE(BlackstarSha256 &sha, uint64 value)
{
    unsigned char bytes[8];
    for (uint ii=0; ii<8; ++ii) bytes[ii]=static_cast<unsigned char>((value >> (8U*ii)) & 0xffU);
    sha.update(bytes, sizeof(bytes));
}

string aggregateIdentity(const vector<IdentityFile> &files)
{
    BlackstarSha256 sha;
    const char domain[]="BlackSTAR index identity v1";
    sha.update(domain, sizeof(domain)-1);
    updateUint64LE(sha, files.size());
    for (vector<IdentityFile>::const_iterator it=files.begin(); it!=files.end(); ++it) {
        updateUint64LE(sha, it->name.size());
        sha.update(it->name.data(), it->name.size());
        const unsigned char present=it->present ? 1 : 0;
        sha.update(&present, sizeof(present));
        updateUint64LE(sha, it->size);
        if (it->present) {
            string digestBytes;
            if (!blackstarHexDecode(it->digest, digestBytes) || digestBytes.size()!=32) {
                throw runtime_error("internal invalid file identity for " + it->name);
            }
            sha.update(digestBytes.data(), digestBytes.size());
        }
    }
    return BlackstarSha256::hex(sha.final());
}

IdentityFile identityFromPath(const string &directory, const string &name, bool required, uint threadN)
{
    IdentityFile identity;
    identity.name=name;
    identity.present=false;
    identity.size=0;
    const string path=joinPath(directory, name);
    if (!regularFile(path, &identity.size)) {
        if (required) throw runtime_error("required base index file is missing or not regular: " + path);
        return identity;
    }
    identity.present=true;
    identity.digest=blackstarFileMerkleSha256(path, threadN);
    return identity;
}

IdentityFile identityFromSegments(const string &name, const vector<BlackstarMemorySegment> &segments, uint threadN)
{
    IdentityFile identity;
    identity.name=name;
    identity.present=true;
    identity.size=0;
    for (vector<BlackstarMemorySegment>::const_iterator it=segments.begin(); it!=segments.end(); ++it) {
        if (it->size>std::numeric_limits<uint64>::max()-identity.size) {
            throw overflow_error("base index identity size overflow");
        }
        identity.size+=it->size;
    }
    identity.digest=blackstarMerkleSha256(segments, threadN);
    return identity;
}

void identityFailure(const string &message, Parameters &P)
{
    ostringstream error;
    error << "EXITING because of fatal INPUT FILE error while identifying the base genome index\n";
    error << message << "\n";
    exitWithError(error.str(), std::cerr, P.inOut->logMain, EXIT_CODE_INPUT_FILES, P);
}
}

string genomeInsertBaseIdentityFromDirectory(const string &directory, uint threadN, Parameters &P)
{
    try {
        vector<IdentityFile> identities;
        for (vector<string>::const_iterator it=requiredIdentityFiles.begin(); it!=requiredIdentityFiles.end(); ++it) {
            identities.push_back(identityFromPath(directory, *it, true, threadN));
        }
        for (vector<string>::const_iterator it=optionalIdentityFiles.begin(); it!=optionalIdentityFiles.end(); ++it) {
            identities.push_back(identityFromPath(directory, *it, false, threadN));
        }
        return aggregateIdentity(identities);
    } catch (const exception &error) {
        identityFailure(error.what(), P);
    }
    return "";
}

string genomeInsertBaseIdentityFromLoaded(Genome &genome)
{
    Parameters &P=genome.P;
    try {
        vector<IdentityFile> identities;
        vector<BlackstarMemorySegment> genomeSegments;
        genomeSegments.push_back(BlackstarMemorySegment(genome.G, genome.nGenome));
        identities.push_back(identityFromSegments("Genome", genomeSegments, P.runThreadN));

        vector<BlackstarMemorySegment> saSegments;
        saSegments.push_back(BlackstarMemorySegment(genome.SA.charArray, genome.SA.lengthByte));
        identities.push_back(identityFromSegments("SA", saSegments, P.runThreadN));

        vector<BlackstarMemorySegment> saiSegments;
        saiSegments.push_back(BlackstarMemorySegment(&genome.pGe.gSAindexNbases, sizeof(genome.pGe.gSAindexNbases)));
        saiSegments.push_back(BlackstarMemorySegment(genome.genomeSAindexStart,
                              sizeof(genome.genomeSAindexStart[0])*(genome.pGe.gSAindexNbases+1)));
        saiSegments.push_back(BlackstarMemorySegment(genome.SAi.charArray, genome.SAi.lengthByte));
        identities.push_back(identityFromSegments("SAindex", saiSegments, P.runThreadN));

        for (uint ii=3; ii<requiredIdentityFiles.size(); ++ii) {
            identities.push_back(identityFromPath(genome.pGe.gDir, requiredIdentityFiles[ii], true, P.runThreadN));
        }
        for (vector<string>::const_iterator it=optionalIdentityFiles.begin(); it!=optionalIdentityFiles.end(); ++it) {
            identities.push_back(identityFromPath(genome.pGe.gDir, *it, false, P.runThreadN));
        }
        return aggregateIdentity(identities);
    } catch (const exception &error) {
        identityFailure(error.what(), P);
    }
    return "";
}

string genomeInsertFileIdentity(const string &path, uint threadN, Parameters &P)
{
    try {
        return blackstarFileMerkleSha256(path, threadN);
    } catch (const exception &error) {
        ostringstream message;
        message << "EXITING because of fatal INPUT FILE error while hashing " << path << "\n";
        message << error.what() << "\n";
        exitWithError(message.str(), std::cerr, P.inOut->logMain, EXIT_CODE_INPUT_FILES, P);
    }
    return "";
}

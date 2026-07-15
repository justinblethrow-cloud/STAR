#include "GenomeInsertOverlay.h"

#include "BlackstarSha256.h"
#include "ErrorWarning.h"
#include "GenomeInsertIdentity.h"
#include "Parameters.h"
#include "streamFuns.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <map>
#include <set>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_set>

namespace {
const string overlayFileName="genomeInsertOverlay.tsv";
const string deltaFileName="genomeInsertDelta.bin";
const string packagedFastaName="inserted.fa";
const string packagedGtfName="inserted.gtf";
const string completionFileName="blackstar.complete.tsv";

struct GenomeInsertOverlay {
    string mode;
    string baseGenomeDir;
    vector<string> genomeFastaFiles;
    string sjdbGTFfile;
    string deltaFile;
    string baseSha256;
    string fastaSha256;
    string gtfSha256;
    uint sjdbOverhang;
    bool gtfHasJunctions;
};

struct CompletionEntry {
    string name;
    uint64 size;
    string digest;
};

void fail(const string &message, int exitCode, Parameters &P)
{
    exitWithError(message, std::cerr, P.inOut->logMain, exitCode, P);
}

string stripTrailingSlash(string path)
{
    while (path.size()>1 && path.back()=='/') path.erase(path.end()-1);
    return path;
}

string addTrailingSlash(const string &path, Parameters &P)
{
    if (path.size()==0) {
        fail("EXITING because of fatal INPUT FILE error: empty genome insert path\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    return path.back()=='/' ? path : path+"/";
}

string joinPath(const string &directory, const string &name)
{
    return stripTrailingSlash(directory)+"/"+name;
}

string manifestPath(const string &directory)
{
    return joinPath(directory, overlayFileName);
}

bool pathStat(const string &path, struct stat &value)
{
    return lstat(path.c_str(), &value)==0;
}

bool regularFile(const string &path)
{
    struct stat value;
    return stat(path.c_str(), &value)==0 && S_ISREG(value.st_mode);
}

bool directoryExists(const string &path)
{
    struct stat value;
    return stat(path.c_str(), &value)==0 && S_ISDIR(value.st_mode);
}

string absoluteExistingPath(const string &path, const string &label, Parameters &P)
{
    if (path.size()==0) {
        fail("EXITING because of fatal INPUT FILE error: empty " + label + " path\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    char resolvedPath[PATH_MAX];
    if (realpath(path.c_str(), resolvedPath)==NULL) {
        ostringstream error;
        error << "EXITING because of fatal INPUT FILE error: could not resolve " << label << " path " << path << "\n";
        error << "ERROR: " << strerror(errno) << "\n";
        fail(error.str(), EXIT_CODE_INPUT_FILES, P);
    }
    return string(resolvedPath);
}

void ensureDirectoryPath(const string &directory, mode_t permissions, Parameters &P)
{
    if (directory.size()==0 || directory=="/") return;
    string current=directory.at(0)=='/' ? "/" : "";
    size_t start=directory.at(0)=='/' ? 1 : 0;
    while (start<=directory.size()) {
        const size_t end=directory.find('/', start);
        const string component=directory.substr(start, end==string::npos ? string::npos : end-start);
        if (component.size()>0) {
            if (current.size()>1 && current.back()!='/') current+="/";
            current+=component;
            if (mkdir(current.c_str(), permissions)!=0 && errno!=EEXIST) {
                ostringstream error;
                error << "EXITING because of fatal OUTPUT FILE error: could not create directory " << current << "\n";
                error << "ERROR: " << strerror(errno) << "\n";
                fail(error.str(), EXIT_CODE_FILE_OPEN, P);
            }
            if (!directoryExists(current)) {
                fail("EXITING because of fatal OUTPUT FILE error: path is not a directory: " + current + "\n",
                     EXIT_CODE_FILE_OPEN, P);
            }
        }
        if (end==string::npos) break;
        start=end+1;
    }
}

string absoluteOutputPath(const string &pathIn, mode_t permissions, Parameters &P)
{
    const string path=stripTrailingSlash(pathIn);
    const size_t slash=path.find_last_of('/');
    string parent=slash==string::npos ? "." : (slash==0 ? "/" : path.substr(0, slash));
    const string leaf=slash==string::npos ? path : path.substr(slash+1);
    if (leaf.size()==0 || leaf=="." || leaf=="..") {
        fail("EXITING because of fatal OUTPUT FILE error: invalid --genomeInsertOutDir " + pathIn + "\n",
             EXIT_CODE_FILE_OPEN, P);
    }
    ensureDirectoryPath(parent, permissions, P);
    parent=absoluteExistingPath(parent, "output parent directory", P);
    return joinPath(parent, leaf);
}

vector<string> listDirectory(const string &directory, Parameters &P)
{
    DIR *stream=opendir(directory.c_str());
    if (stream==NULL) {
        fail("EXITING because of fatal INPUT FILE error: could not open directory " + directory + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    vector<string> names;
    errno=0;
    while (struct dirent *entry=readdir(stream)) {
        const string name=entry->d_name;
        if (name!="." && name!="..") names.push_back(name);
    }
    const int savedErrno=errno;
    closedir(stream);
    if (savedErrno!=0) {
        fail("EXITING because of fatal INPUT FILE error while reading directory " + directory + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    sort(names.begin(), names.end());
    return names;
}

bool directoryEmpty(const string &directory, Parameters &P)
{
    return listDirectory(directory, P).empty();
}

string decodePath(const string &encoded, const string &key, Parameters &P)
{
    string decoded;
    if (!blackstarHexDecode(encoded, decoded) || decoded.size()==0 || decoded.find('\0')!=string::npos) {
        fail("EXITING because of fatal INPUT FILE error: invalid hex path for " + key + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    return decoded;
}

uint64 parseUint64(const string &value, const string &key, Parameters &P)
{
    if (value.size()==0 || value.find_first_not_of("0123456789")!=string::npos) {
        fail("EXITING because of fatal INPUT FILE error: invalid integer for " + key + ": " + value + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    errno=0;
    char *end=NULL;
    const unsigned long long parsed=strtoull(value.c_str(), &end, 10);
    if (errno!=0 || end==NULL || *end!='\0') {
        fail("EXITING because of fatal INPUT FILE error: invalid integer for " + key + ": " + value + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    return static_cast<uint64>(parsed);
}

uint64 readBaseSjdbOverhang(const string &baseDirectory, Parameters &P)
{
    const string path=joinPath(baseDirectory, "genomeParameters.txt");
    ifstream input(path.c_str());
    if (input.fail()) {
        fail("EXITING because of fatal INPUT FILE error: could not open " + path + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }

    bool found=false;
    uint64 overhang=0;
    string line;
    while (getline(input, line)) {
        if (line.empty() || line.at(0)=='#') continue;
        istringstream fields(line);
        string key;
        fields >> key;
        if (key!="sjdbOverhang") continue;
        string value;
        string extra;
        if (found || !(fields >> value) || (fields >> extra)) {
            fail("EXITING because of fatal INPUT FILE error: malformed or duplicate sjdbOverhang in " + path + "\n",
                 EXIT_CODE_INPUT_FILES, P);
        }
        overhang=parseUint64(value, "base sjdbOverhang", P);
        found=true;
    }
    if (!found) {
        fail("EXITING because of fatal INPUT FILE error: missing sjdbOverhang in " + path + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    return overhang;
}

void normalizeSjdbOverhangForBase(const string &baseDirectory, Parameters &P)
{
    const uint64 baseOverhang=readBaseSjdbOverhang(baseDirectory, P);
    const bool explicitlySet=P.parArray.at(P.pGe.sjdbOverhang_par)->inputLevel>0;
    const bool baseHasJunctions=regularFile(joinPath(baseDirectory, "sjdbInfo.txt"));
    if (!explicitlySet && baseOverhang>0) {
        P.pGe.sjdbOverhang=baseOverhang;
    } else if (baseHasJunctions && explicitlySet && P.pGe.sjdbOverhang!=baseOverhang) {
        ostringstream error;
        error << "EXITING because of fatal PARAMETERS error: --sjdbOverhang=" << P.pGe.sjdbOverhang
              << " does not match the base index value " << baseOverhang << "\n";
        fail(error.str(), EXIT_CODE_PARAMETER, P);
    }
}

map<string,string> readStrictKeyValues(const string &path, const set<string> &allowed, Parameters &P)
{
    ifstream input(path.c_str(), ios::in | ios::binary);
    if (input.fail()) {
        fail("EXITING because of fatal INPUT FILE error: could not open " + path + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    map<string,string> values;
    string line;
    uint64 lineNumber=0;
    while (getline(input, line)) {
        ++lineNumber;
        if (!line.empty() && line.back()=='\r') line.erase(line.end()-1);
        if (line.size()==0 || line.at(0)=='#') continue;
        const size_t tab=line.find('\t');
        if (tab==string::npos || tab==0 || line.find('\t', tab+1)!=string::npos) {
            ostringstream error;
            error << "EXITING because of fatal INPUT FILE error: malformed line " << lineNumber << " in " << path << "\n";
            fail(error.str(), EXIT_CODE_INPUT_FILES, P);
        }
        const string key=line.substr(0, tab);
        const string value=line.substr(tab+1);
        if (allowed.find(key)==allowed.end()) {
            fail("EXITING because of fatal INPUT FILE error: unknown key " + key + " in " + path + "\n",
                 EXIT_CODE_INPUT_FILES, P);
        }
        if (!values.insert(make_pair(key, value)).second) {
            fail("EXITING because of fatal INPUT FILE error: duplicate key " + key + " in " + path + "\n",
                 EXIT_CODE_INPUT_FILES, P);
        }
    }
    return values;
}

const string &requiredValue(const map<string,string> &values, const string &key, const string &path, Parameters &P)
{
    map<string,string>::const_iterator found=values.find(key);
    if (found==values.end()) {
        fail("EXITING because of fatal INPUT FILE error: missing key " + key + " in " + path + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    return found->second;
}

unordered_set<string> readBaseNames(const string &baseDirectory, Parameters &P)
{
    const string path=joinPath(baseDirectory, "chrName.txt");
    ifstream input(path.c_str());
    if (input.fail()) {
        fail("EXITING because of fatal INPUT FILE error: could not open " + path + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    unordered_set<string> names;
    string name;
    while (getline(input, name)) {
        if (!name.empty() && name.back()=='\r') name.erase(name.end()-1);
        if (name.size()==0) continue;
        if (!names.insert(name).second) {
            fail("EXITING because of fatal INPUT FILE error: base index contains duplicate reference name " + name + "\n",
                 EXIT_CODE_INPUT_FILES, P);
        }
    }
    return names;
}

unordered_set<string> readFastaNames(const vector<string> &fastaFiles, const unordered_set<string> &baseNames, Parameters &P)
{
    unordered_set<string> names;
    for (vector<string>::const_iterator file=fastaFiles.begin(); file!=fastaFiles.end(); ++file) {
        ifstream input(file->c_str());
        if (input.fail()) {
            fail("EXITING because of fatal INPUT FILE error: could not open inserted FASTA file " + *file + "\n",
                 EXIT_CODE_INPUT_FILES, P);
        }
        string line;
        string activeName;
        uint64 activeBases=0;
        uint64 lineNumber=0;
        while (getline(input, line)) {
            ++lineNumber;
            if (!line.empty() && line.back()=='\r') line.erase(line.end()-1);
            if (!line.empty() && line.at(0)=='>') {
                if (!activeName.empty() && activeBases==0) {
                    fail("EXITING because of fatal INPUT FILE error: inserted FASTA sequence " + activeName + " is empty\n",
                         EXIT_CODE_INPUT_FILES, P);
                }
                istringstream header(line.substr(1));
                header >> activeName;
                activeBases=0;
                if (activeName.size()==0) {
                    fail("EXITING because of fatal INPUT FILE error: empty sequence name in " + *file + "\n",
                         EXIT_CODE_INPUT_FILES, P);
                }
                if (!names.insert(activeName).second) {
                    fail("EXITING because of fatal INPUT FILE error: duplicate inserted reference name " + activeName + "\n",
                         EXIT_CODE_INPUT_FILES, P);
                }
                if (baseNames.find(activeName)!=baseNames.end()) {
                    fail("EXITING because of fatal INPUT FILE error: inserted reference name collides with base index: " + activeName + "\n",
                         EXIT_CODE_INPUT_FILES, P);
                }
            } else {
                if (activeName.size()==0 && line.find_first_not_of(" \t")==string::npos) continue;
                if (activeName.size()==0) {
                    ostringstream error;
                    error << "EXITING because of fatal INPUT FILE error: sequence data precedes the first FASTA header in "
                          << *file << ", line " << lineNumber << "\n";
                    fail(error.str(), EXIT_CODE_INPUT_FILES, P);
                }
                for (string::const_iterator it=line.begin(); it!=line.end(); ++it) {
                    if (static_cast<unsigned char>(*it)>=32 && !isspace(static_cast<unsigned char>(*it))) ++activeBases;
                }
            }
        }
        if (!activeName.empty() && activeBases==0) {
            fail("EXITING because of fatal INPUT FILE error: inserted FASTA sequence " + activeName + " is empty\n",
                 EXIT_CODE_INPUT_FILES, P);
        }
    }
    if (names.empty()) {
        fail("EXITING because of fatal INPUT FILE error: no sequence names were found in inserted FASTA files\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    return names;
}

vector<string> splitTabs(const string &line)
{
    vector<string> fields;
    size_t start=0;
    while (true) {
        const size_t end=line.find('\t', start);
        fields.push_back(line.substr(start, end==string::npos ? string::npos : end-start));
        if (end==string::npos) break;
        start=end+1;
    }
    return fields;
}

string gtfAttribute(const string &attributes, const string &tag)
{
    size_t start=0;
    while (start<attributes.size()) {
        const size_t end=attributes.find(';', start);
        string field=attributes.substr(start, end==string::npos ? string::npos : end-start);
        start=end==string::npos ? attributes.size() : end+1;

        size_t fieldStart=0;
        while (fieldStart<field.size() && isspace(static_cast<unsigned char>(field[fieldStart]))) ++fieldStart;
        size_t keyEnd=fieldStart;
        while (keyEnd<field.size() && field[keyEnd]!='=' && !isspace(static_cast<unsigned char>(field[keyEnd]))) ++keyEnd;
        if (field.substr(fieldStart, keyEnd-fieldStart)!=tag) continue;

        size_t valueStart=keyEnd;
        while (valueStart<field.size() && isspace(static_cast<unsigned char>(field[valueStart]))) ++valueStart;
        if (valueStart<field.size() && field[valueStart]=='=') ++valueStart;
        while (valueStart<field.size() && isspace(static_cast<unsigned char>(field[valueStart]))) ++valueStart;
        if (valueStart==field.size()) return "";

        if (field[valueStart]=='"') {
            const size_t quoteEnd=field.find('"', valueStart+1);
            if (quoteEnd==string::npos) return "";
            return field.substr(valueStart+1, quoteEnd-valueStart-1);
        }
        size_t valueEnd=field.size();
        while (valueEnd>valueStart && isspace(static_cast<unsigned char>(field[valueEnd-1]))) --valueEnd;
        return field.substr(valueStart, valueEnd-valueStart);
    }
    return "";
}

string stagingDirectoryToClean;

void removeStagedTree(const string &directory)
{
    DIR *stream=opendir(directory.c_str());
    if (stream==NULL) return;
    while (struct dirent *entry=readdir(stream)) {
        const string name=entry->d_name;
        if (name=="." || name=="..") continue;
        const string path=joinPath(directory, name);
        struct stat value;
        if (lstat(path.c_str(), &value)!=0) continue;
        if (S_ISDIR(value.st_mode)) {
            removeStagedTree(path);
            rmdir(path.c_str());
        } else {
            unlink(path.c_str());
        }
    }
    closedir(stream);
}

void cleanupStagingDirectoryAtExit()
{
    if (stagingDirectoryToClean.empty()) return;
    removeStagedTree(stagingDirectoryToClean);
    rmdir(stagingDirectoryToClean.c_str());
}

unordered_set<string> readSidecarIds(const string &path, Parameters &P)
{
    unordered_set<string> ids;
    if (!regularFile(path)) return ids;
    ifstream input(path.c_str());
    if (input.fail()) {
        fail("EXITING because of fatal INPUT FILE error: could not open annotation sidecar " + path + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    string line;
    getline(input, line); // count
    while (getline(input, line)) {
        const size_t tab=line.find('\t');
        const string id=line.substr(0, tab);
        if (id.size()>0) ids.insert(id);
    }
    return ids;
}

bool validateGtf(const string &gtfFile, const unordered_set<string> &insertedNames,
                 const string &baseDirectory, Parameters &P)
{
    if (gtfFile=="-") return false;
    ifstream input(gtfFile.c_str());
    if (input.fail()) {
        fail("EXITING because of fatal INPUT FILE error: could not open inserted GTF file " + gtfFile + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }

    const unordered_set<string> baseGenes=readSidecarIds(joinPath(baseDirectory, "geneInfo.tab"), P);
    const unordered_set<string> baseTranscripts=readSidecarIds(joinPath(baseDirectory, "transcriptInfo.tab"), P);
    map<string,string> transcriptContexts;
    unordered_set<string> seenTranscriptExons;
    bool hasJunctions=false;
    uint64 exonCount=0;
    uint64 lineNumber=0;
    string line;
    while (getline(input, line)) {
        ++lineNumber;
        if (line.empty() || line.at(0)=='#') continue;
        const vector<string> fields=splitTabs(line);
        if (fields.size()!=9) {
            ostringstream error;
            error << "EXITING because of fatal INPUT FILE error: expected 9 tab-separated GTF fields in "
                  << gtfFile << ", line " << lineNumber << "\n";
            fail(error.str(), EXIT_CODE_INPUT_FILES, P);
        }
        if (fields[2]!=P.pGe.sjdbGTFfeatureExon) continue;
        ++exonCount;
        const string chromosome=P.pGe.sjdbGTFchrPrefix=="-" ? fields[0] : P.pGe.sjdbGTFchrPrefix+fields[0];
        if (insertedNames.find(chromosome)==insertedNames.end()) {
            ostringstream error;
            error << "EXITING because of fatal INPUT FILE error: genomeInsert expects only annotations for inserted sequences; found "
                  << chromosome << " at " << gtfFile << ":" << lineNumber << "\n";
            fail(error.str(), EXIT_CODE_INPUT_FILES, P);
        }

        const string geneId=gtfAttribute(fields[8], P.pGe.sjdbGTFtagExonParentGene);
        const string transcriptId=gtfAttribute(fields[8], P.pGe.sjdbGTFtagExonParentTranscript);
        if (geneId.empty() || transcriptId.empty()) {
            ostringstream error;
            error << "EXITING because of fatal INPUT FILE error: inserted GTF exon lacks "
                  << P.pGe.sjdbGTFtagExonParentGene << " or " << P.pGe.sjdbGTFtagExonParentTranscript
                  << " at " << gtfFile << ":" << lineNumber << "\n";
            fail(error.str(), EXIT_CODE_INPUT_FILES, P);
        }
        if (baseGenes.find(geneId)!=baseGenes.end()) {
            fail("EXITING because of fatal INPUT FILE error: inserted gene_id collides with base annotation: " + geneId + "\n",
                 EXIT_CODE_INPUT_FILES, P);
        }
        if (baseTranscripts.find(transcriptId)!=baseTranscripts.end()) {
            fail("EXITING because of fatal INPUT FILE error: inserted transcript_id collides with base annotation: " + transcriptId + "\n",
                 EXIT_CODE_INPUT_FILES, P);
        }
        const string transcriptContext=chromosome+"\t"+fields[6]+"\t"+geneId;
        map<string,string>::const_iterator priorContext=transcriptContexts.find(transcriptId);
        if (priorContext==transcriptContexts.end()) {
            transcriptContexts.insert(make_pair(transcriptId, transcriptContext));
        } else if (priorContext->second!=transcriptContext) {
            fail("EXITING because of fatal INPUT FILE error: inserted transcript_id is reused across incompatible chromosome, strand, or gene contexts: "
                 + transcriptId + "\n", EXIT_CODE_INPUT_FILES, P);
        }
        const string transcriptKey=chromosome+"\t"+fields[6]+"\t"+transcriptId;
        if (!seenTranscriptExons.insert(transcriptKey).second) hasJunctions=true;
    }
    if (exonCount==0) {
        fail("EXITING because of fatal INPUT FILE error: no exon records were found in inserted GTF " + gtfFile + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    return hasJunctions;
}

void copyFileChecked(const string &source, const string &destination, Parameters &P)
{
    ifstream input(source.c_str(), ios::in | ios::binary);
    ofstream output(destination.c_str(), ios::out | ios::binary | ios::trunc);
    if (input.fail() || output.fail()) {
        fail("EXITING because of fatal OUTPUT FILE error while packaging " + source + "\n",
             EXIT_CODE_FILE_OPEN, P);
    }
    output << input.rdbuf();
    output.close();
    if (input.bad() || output.fail()) {
        fail("EXITING because of fatal OUTPUT FILE error while packaging " + source + "\n",
             EXIT_CODE_FILE_WRITE, P);
    }
}

void concatenateFastas(const vector<string> &sources, const string &destination, Parameters &P)
{
    ofstream output(destination.c_str(), ios::out | ios::binary | ios::trunc);
    if (output.fail()) {
        fail("EXITING because of fatal OUTPUT FILE error: could not create " + destination + "\n",
             EXIT_CODE_FILE_OPEN, P);
    }
    bool needSeparator=false;
    for (vector<string>::const_iterator source=sources.begin(); source!=sources.end(); ++source) {
        ifstream input(source->c_str(), ios::in | ios::binary);
        if (input.fail()) {
            fail("EXITING because of fatal INPUT FILE error: could not open " + *source + "\n",
                 EXIT_CODE_INPUT_FILES, P);
        }
        if (needSeparator) output.put('\n');
        char buffer[1024*1024];
        char last='\n';
        bool wrote=false;
        while (input.good()) {
            input.read(buffer, sizeof(buffer));
            const streamsize count=input.gcount();
            if (count>0) {
                output.write(buffer, count);
                last=buffer[count-1];
                wrote=true;
            }
        }
        if (input.bad() || output.fail()) {
            fail("EXITING because of fatal OUTPUT FILE error while packaging inserted FASTA files\n",
                 EXIT_CODE_FILE_WRITE, P);
        }
        needSeparator=wrote && last!='\n';
    }
    if (needSeparator) output.put('\n');
    output.close();
    if (output.fail()) {
        fail("EXITING because of fatal OUTPUT FILE error while closing " + destination + "\n",
             EXIT_CODE_FILE_WRITE, P);
    }
}

void fsyncPath(const string &path, bool directory, Parameters &P)
{
    const int flags=directory ? (O_RDONLY | O_DIRECTORY) : O_RDONLY;
    const int descriptor=open(path.c_str(), flags);
    if (descriptor<0 || fsync(descriptor)!=0) {
        const string error=strerror(errno);
        if (descriptor>=0) close(descriptor);
        fail("EXITING because of fatal OUTPUT FILE error while syncing " + path + ": " + error + "\n",
             EXIT_CODE_FILE_WRITE, P);
    }
    close(descriptor);
}

vector<CompletionEntry> completionEntries(const string &directory, Parameters &P)
{
    vector<CompletionEntry> entries;
    const vector<string> names=listDirectory(directory, P);
    for (vector<string>::const_iterator name=names.begin(); name!=names.end(); ++name) {
        if (*name==completionFileName) continue;
        const string path=joinPath(directory, *name);
        struct stat value;
        if (lstat(path.c_str(), &value)!=0 || !S_ISREG(value.st_mode) || value.st_size<0) {
            fail("EXITING because of fatal OUTPUT FILE error: artifact contains unsupported non-regular entry " + path + "\n",
                 EXIT_CODE_FILE_WRITE, P);
        }
        CompletionEntry entry;
        entry.name=*name;
        entry.size=static_cast<uint64>(value.st_size);
        entry.digest=genomeInsertFileIdentity(path, P.runThreadN, P);
        entries.push_back(entry);
    }
    return entries;
}

void writeCompletionFile(const string &directory, const string &mode, Parameters &P)
{
    const vector<CompletionEntry> entries=completionEntries(directory, P);
    const string path=joinPath(directory, completionFileName);
    ofstream output(path.c_str(), ios::out | ios::binary | ios::trunc);
    if (output.fail()) {
        fail("EXITING because of fatal OUTPUT FILE error: could not create " + path + "\n",
             EXIT_CODE_FILE_OPEN, P);
    }
    output << "blackstarCompleteVersion\t1\n";
    output << "artifactMode\t" << mode << "\n";
    output << "fileCount\t" << entries.size() << "\n";
    for (vector<CompletionEntry>::const_iterator it=entries.begin(); it!=entries.end(); ++it) {
        output << "file\t" << blackstarHexEncode(it->name) << "\t" << it->size << "\t" << it->digest << "\n";
    }
    output << "END\n";
    output.close();
    if (output.fail()) {
        fail("EXITING because of fatal OUTPUT FILE error while writing " + path + "\n",
             EXIT_CODE_FILE_WRITE, P);
    }
    for (vector<CompletionEntry>::const_iterator it=entries.begin(); it!=entries.end(); ++it) {
        fsyncPath(joinPath(directory, it->name), false, P);
    }
    fsyncPath(path, false, P);
    fsyncPath(directory, true, P);
}

string validateCompletionFile(const string &directory, Parameters &P)
{
    const string path=joinPath(directory, completionFileName);
    ifstream input(path.c_str(), ios::in | ios::binary);
    if (input.fail()) {
        fail("EXITING because of fatal INPUT FILE error: incomplete BlackSTAR artifact; missing " + path + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    string line;
    if (!getline(input, line) || line!="blackstarCompleteVersion\t1") {
        fail("EXITING because of fatal INPUT FILE error: unsupported completion manifest " + path + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    if (!getline(input, line) || line.find("artifactMode\t")!=0) {
        fail("EXITING because of fatal INPUT FILE error: malformed completion manifest " + path + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    const string mode=line.substr(strlen("artifactMode\t"));
    if (mode!="Full" && mode!="Overlay" && mode!="Delta") {
        fail("EXITING because of fatal INPUT FILE error: invalid artifact mode in " + path + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    if (!getline(input, line) || line.find("fileCount\t")!=0) {
        fail("EXITING because of fatal INPUT FILE error: malformed completion manifest " + path + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    const uint64 fileCount=parseUint64(line.substr(strlen("fileCount\t")), "fileCount", P);
    map<string,CompletionEntry> expected;
    for (uint64 ii=0; ii<fileCount; ++ii) {
        if (!getline(input, line)) {
            fail("EXITING because of fatal INPUT FILE error: truncated completion manifest " + path + "\n",
                 EXIT_CODE_INPUT_FILES, P);
        }
        const vector<string> fields=splitTabs(line);
        if (fields.size()!=4 || fields[0]!="file") {
            fail("EXITING because of fatal INPUT FILE error: malformed file record in " + path + "\n",
                 EXIT_CODE_INPUT_FILES, P);
        }
        string name=decodePath(fields[1], "completion file", P);
        if (name=="." || name==".." || name.find('/')!=string::npos || name==completionFileName) {
            fail("EXITING because of fatal INPUT FILE error: unsafe file name in " + path + "\n",
                 EXIT_CODE_INPUT_FILES, P);
        }
        CompletionEntry entry;
        entry.name=name;
        entry.size=parseUint64(fields[2], "completion file size", P);
        entry.digest=fields[3];
        if (!blackstarIsSha256(entry.digest) || !expected.insert(make_pair(name, entry)).second) {
            fail("EXITING because of fatal INPUT FILE error: invalid or duplicate file record in " + path + "\n",
                 EXIT_CODE_INPUT_FILES, P);
        }
    }
    if (!getline(input, line) || line!="END" || getline(input, line)) {
        fail("EXITING because of fatal INPUT FILE error: malformed completion manifest terminator in " + path + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }

    const vector<string> actualNames=listDirectory(directory, P);
    set<string> actual;
    for (vector<string>::const_iterator name=actualNames.begin(); name!=actualNames.end(); ++name) {
        if (*name!=completionFileName) actual.insert(*name);
    }
    if (actual.size()!=expected.size()) {
        fail("EXITING because of fatal INPUT FILE error: completion manifest file set does not match artifact directory " + directory + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    for (map<string,CompletionEntry>::const_iterator it=expected.begin(); it!=expected.end(); ++it) {
        if (actual.find(it->first)==actual.end()) {
            fail("EXITING because of fatal INPUT FILE error: completion manifest references missing file " + it->first + "\n",
                 EXIT_CODE_INPUT_FILES, P);
        }
        const string filePath=joinPath(directory, it->first);
        struct stat value;
        if (lstat(filePath.c_str(), &value)!=0 || !S_ISREG(value.st_mode)
                || static_cast<uint64>(value.st_size)!=it->second.size) {
            fail("EXITING because of fatal INPUT FILE error: artifact file size mismatch for " + filePath + "\n",
                 EXIT_CODE_INPUT_FILES, P);
        }
        if (genomeInsertFileIdentity(filePath, P.runThreadN, P)!=it->second.digest) {
            fail("EXITING because of fatal INPUT FILE error: artifact checksum mismatch for " + filePath + "\n",
                 EXIT_CODE_INPUT_FILES, P);
        }
    }
    return mode;
}

GenomeInsertOverlay readOverlay(const string &overlayDirectory, Parameters &P)
{
    const string completionMode=validateCompletionFile(overlayDirectory, P);
    const string path=manifestPath(overlayDirectory);
    const set<string> allowed = {
        "genomeInsertOverlayVersion", "artifactMode", "baseGenomeDirHex",
        "genomeFastaFileCount", "genomeFastaFile0Hex", "sjdbGTFfileHex",
        "genomeInsertDeltaFileHex", "genomeInsertGTFhasJunctions", "sjdbOverhang",
        "baseIndexSha256Tree", "insertFastaSha256Tree", "insertGtfSha256Tree"
    };
    const map<string,string> values=readStrictKeyValues(path, allowed, P);
    if (values.size()!=allowed.size()) {
        fail("EXITING because of fatal INPUT FILE error: overlay manifest does not contain the exact v2 schema: " + path + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    if (parseUint64(requiredValue(values, "genomeInsertOverlayVersion", path, P), "genomeInsertOverlayVersion", P)!=2) {
        fail("EXITING because of fatal INPUT FILE error: unsupported genome insert overlay version in " + path + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }

    GenomeInsertOverlay overlay;
    overlay.mode=requiredValue(values, "artifactMode", path, P);
    if ((overlay.mode!="Overlay" && overlay.mode!="Delta") || overlay.mode!=completionMode) {
        fail("EXITING because of fatal INPUT FILE error: inconsistent artifact mode in " + path + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    overlay.baseGenomeDir=addTrailingSlash(absoluteExistingPath(
        decodePath(requiredValue(values, "baseGenomeDirHex", path, P), "baseGenomeDirHex", P),
        "base genomeDir", P), P);
    if (parseUint64(requiredValue(values, "genomeFastaFileCount", path, P), "genomeFastaFileCount", P)!=1) {
        fail("EXITING because of fatal INPUT FILE error: BlackSTAR v2 overlays require one packaged FASTA\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    const string fastaRelative=decodePath(requiredValue(values, "genomeFastaFile0Hex", path, P), "genomeFastaFile0Hex", P);
    if (fastaRelative!=packagedFastaName) {
        fail("EXITING because of fatal INPUT FILE error: unsafe packaged FASTA path in " + path + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    overlay.genomeFastaFiles.push_back(joinPath(overlayDirectory, fastaRelative));

    const string gtfRelative=decodePath(requiredValue(values, "sjdbGTFfileHex", path, P), "sjdbGTFfileHex", P);
    if (gtfRelative!="-" && gtfRelative!=packagedGtfName) {
        fail("EXITING because of fatal INPUT FILE error: unsafe packaged GTF path in " + path + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    overlay.sjdbGTFfile=gtfRelative=="-" ? "-" : joinPath(overlayDirectory, gtfRelative);

    const string deltaRelative=decodePath(requiredValue(values, "genomeInsertDeltaFileHex", path, P), "genomeInsertDeltaFileHex", P);
    if ((overlay.mode=="Delta" && deltaRelative!=deltaFileName) || (overlay.mode=="Overlay" && deltaRelative!="-")) {
        fail("EXITING because of fatal INPUT FILE error: inconsistent Delta path in " + path + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    overlay.deltaFile=deltaRelative=="-" ? "-" : joinPath(overlayDirectory, deltaRelative);
    const uint64 junctionFlag=parseUint64(requiredValue(values, "genomeInsertGTFhasJunctions", path, P), "genomeInsertGTFhasJunctions", P);
    if (junctionFlag>1) {
        fail("EXITING because of fatal INPUT FILE error: genomeInsertGTFhasJunctions must be 0 or 1 in " + path + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    overlay.gtfHasJunctions=junctionFlag==1;
    overlay.sjdbOverhang=static_cast<uint>(parseUint64(requiredValue(values, "sjdbOverhang", path, P), "sjdbOverhang", P));
    overlay.baseSha256=requiredValue(values, "baseIndexSha256Tree", path, P);
    overlay.fastaSha256=requiredValue(values, "insertFastaSha256Tree", path, P);
    overlay.gtfSha256=requiredValue(values, "insertGtfSha256Tree", path, P);
    if (!blackstarIsSha256(overlay.baseSha256) || !blackstarIsSha256(overlay.fastaSha256)
            || (overlay.gtfSha256!="-" && !blackstarIsSha256(overlay.gtfSha256))) {
        fail("EXITING because of fatal INPUT FILE error: invalid SHA-256 tree identity in " + path + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    if (genomeInsertFileIdentity(overlay.genomeFastaFiles[0], P.runThreadN, P)!=overlay.fastaSha256) {
        fail("EXITING because of fatal INPUT FILE error: packaged FASTA identity mismatch in " + path + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    if ((overlay.sjdbGTFfile=="-")!=(overlay.gtfSha256=="-")) {
        fail("EXITING because of fatal INPUT FILE error: inconsistent packaged GTF identity in " + path + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    if (overlay.sjdbGTFfile!="-" && genomeInsertFileIdentity(overlay.sjdbGTFfile, P.runThreadN, P)!=overlay.gtfSha256) {
        fail("EXITING because of fatal INPUT FILE error: packaged GTF identity mismatch in " + path + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    const unordered_set<string> baseNames=readBaseNames(overlay.baseGenomeDir, P);
    const unordered_set<string> fastaNames=readFastaNames(overlay.genomeFastaFiles, baseNames, P);
    const bool detectedJunctions=validateGtf(overlay.sjdbGTFfile, fastaNames, overlay.baseGenomeDir, P);
    if (detectedJunctions!=overlay.gtfHasJunctions) {
        fail("EXITING because of fatal INPUT FILE error: packaged GTF junction classification mismatch in " + path + "\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    return overlay;
}
}

string genomeInsertDeltaFilePath(const string &directory)
{
    return joinPath(directory, deltaFileName);
}

void genomeInsertOutputPrepare(Parameters &P)
{
    const string baseDirectory=addTrailingSlash(absoluteExistingPath(stripTrailingSlash(P.pGe.gDir), "base genomeDir", P), P);
    vector<string> fastaFiles;
    for (vector<string>::const_iterator it=P.pGe.gFastaFiles.begin(); it!=P.pGe.gFastaFiles.end(); ++it) {
        fastaFiles.push_back(absoluteExistingPath(*it, "inserted FASTA", P));
    }
    const string gtfFile=P.pGe.sjdbGTFfile=="-" ? "-" : absoluteExistingPath(P.pGe.sjdbGTFfile, "inserted GTF", P);
    const unordered_set<string> baseNames=readBaseNames(baseDirectory, P);
    const unordered_set<string> fastaNames=readFastaNames(fastaFiles, baseNames, P);
    const bool gtfHasJunctions=validateGtf(gtfFile, fastaNames, baseDirectory, P);
    normalizeSjdbOverhangForBase(baseDirectory, P);

    const string finalDirectory=absoluteOutputPath(P.pGe.gInsertOutDir, P.runDirPerm, P);
    struct stat existing;
    if (pathStat(finalDirectory, existing)) {
        if (!S_ISDIR(existing.st_mode) || !directoryEmpty(finalDirectory, P) || rmdir(finalDirectory.c_str())!=0) {
            fail("EXITING because of fatal OUTPUT FILE error: --genomeInsertOutDir already exists and is not an empty removable directory: "
                 + finalDirectory + "\n", EXIT_CODE_FILE_OPEN, P);
        }
    }

    string stageDirectory;
    for (uint attempt=0; attempt<100; ++attempt) {
        stageDirectory=finalDirectory+".tmp."+to_string(static_cast<unsigned long long>(getpid()))+"."+to_string(attempt);
        if (mkdir(stageDirectory.c_str(), P.runDirPerm)==0) break;
        if (errno!=EEXIST) {
            fail("EXITING because of fatal OUTPUT FILE error: could not create staging directory " + stageDirectory + "\n",
                 EXIT_CODE_FILE_OPEN, P);
        }
        stageDirectory.clear();
    }
    if (stageDirectory.empty()) {
        fail("EXITING because of fatal OUTPUT FILE error: could not allocate a unique genome insert staging directory\n",
             EXIT_CODE_FILE_OPEN, P);
    }

    stagingDirectoryToClean=stageDirectory;
    static bool cleanupRegistered=false;
    if (!cleanupRegistered) {
        if (atexit(cleanupStagingDirectoryAtExit)!=0) {
            fail("EXITING because of fatal internal ERROR: could not register genome insert staging cleanup\n",
                 EXIT_CODE_INCONSISTENT_DATA, P);
        }
        cleanupRegistered=true;
    }

    P.pGe.gDir=baseDirectory;
    P.pGe.gInsertOutDirFinal=finalDirectory;
    P.pGe.gInsertStageDir=stageDirectory;
    P.pGe.gInsertOutDir=addTrailingSlash(stageDirectory, P);
    P.pGe.gInsertArtifactMode=P.pGe.gInsertOutMode;
    P.pGe.gInsertOverlayGTFhasJunctions=gtfHasJunctions;

    if (P.pGe.gInsertOutMode=="Overlay" || P.pGe.gInsertOutMode=="Delta") {
        const string packagedFasta=joinPath(stageDirectory, packagedFastaName);
        concatenateFastas(fastaFiles, packagedFasta, P);
        P.pGe.gFastaFiles.clear();
        P.pGe.gFastaFiles.push_back(packagedFasta);
        P.pGe.gInsertFastaSha256=genomeInsertFileIdentity(packagedFasta, P.runThreadN, P);
        if (gtfFile=="-") {
            P.pGe.sjdbGTFfile="-";
            P.pGe.gInsertGtfSha256="-";
        } else {
            const string packagedGtf=joinPath(stageDirectory, packagedGtfName);
            copyFileChecked(gtfFile, packagedGtf, P);
            P.pGe.sjdbGTFfile=packagedGtf;
            P.pGe.gInsertGtfSha256=genomeInsertFileIdentity(packagedGtf, P.runThreadN, P);
        }
    } else {
        P.pGe.gFastaFiles=fastaFiles;
        P.pGe.sjdbGTFfile=gtfFile;
    }
    P.inOut->logMain << "BlackSTAR genome insert staging directory: " << stageDirectory << "\n";
}

bool genomeInsertOverlayLoad(Parameters &P)
{
    const string overlayDirectory=stripTrailingSlash(P.pGe.gDir);
    const string path=manifestPath(overlayDirectory);
    if (!regularFile(path)) return false;

    if (P.runModeIn.at(0)!="alignReads") {
        fail("EXITING because of fatal INPUT FILE error: genome insert overlay directories can only be used with --runMode alignReads\n",
             EXIT_CODE_INPUT_FILES, P);
    }
    if (P.pGe.gLoad!="NoSharedMemory") {
        fail("EXITING because of fatal PARAMETERS error: genome insert overlay loading requires --genomeLoad NoSharedMemory\n",
             EXIT_CODE_PARAMETER, P);
    }
    if (P.pGe.gFastaFiles.at(0)!="-" || P.pGe.sjdbGTFfile!="-") {
        fail("EXITING because of fatal PARAMETERS error: genome insert overlays cannot be combined with user FASTA or GTF inputs\n",
             EXIT_CODE_PARAMETER, P);
    }

    const GenomeInsertOverlay overlay=readOverlay(overlayDirectory, P);
    P.pGe.gDir=overlay.baseGenomeDir;
    P.pGe.gFastaFiles=overlay.genomeFastaFiles;
    P.pGe.sjdbGTFfile=overlay.sjdbGTFfile;
    P.pGe.gInsertOverlay=true;
    P.pGe.gInsertArtifactMode=overlay.mode;
    P.pGe.gInsertOverlayDeltaFile=overlay.deltaFile;
    P.pGe.gInsertOverlayGTFhasJunctions=overlay.gtfHasJunctions;
    P.pGe.gInsertBaseSha256Expected=overlay.baseSha256;
    P.pGe.gInsertFastaSha256=overlay.fastaSha256;
    P.pGe.gInsertGtfSha256=overlay.gtfSha256;
    if (overlay.sjdbOverhang>0) P.pGe.sjdbOverhang=overlay.sjdbOverhang;

    P.inOut->logMain << "Genome insert overlay detected: " << path << "\n";
    P.inOut->logMain << "Genome insert overlay base genomeDir: " << P.pGe.gDir << "\n";
    return true;
}

void genomeInsertOverlayWrite(Parameters &P)
{
    if (P.pGe.gInsertBaseSha256=="-") {
        P.pGe.gInsertBaseSha256=genomeInsertBaseIdentityFromDirectory(P.pGe.gDir, P.runThreadN, P);
    }
    if (!blackstarIsSha256(P.pGe.gInsertBaseSha256) || !blackstarIsSha256(P.pGe.gInsertFastaSha256)
            || (P.pGe.gInsertGtfSha256!="-" && !blackstarIsSha256(P.pGe.gInsertGtfSha256))) {
        fail("EXITING because of fatal internal ERROR: incomplete genome insert artifact identity\n",
             EXIT_CODE_INCONSISTENT_DATA, P);
    }

    const string path=manifestPath(P.pGe.gInsertOutDir);
    ofstream output(path.c_str(), ios::out | ios::binary | ios::trunc);
    if (output.fail()) {
        fail("EXITING because of fatal OUTPUT FILE error: could not create " + path + "\n",
             EXIT_CODE_FILE_OPEN, P);
    }
    output << "genomeInsertOverlayVersion\t2\n";
    output << "artifactMode\t" << P.pGe.gInsertOutMode << "\n";
    output << "baseGenomeDirHex\t" << blackstarHexEncode(stripTrailingSlash(P.pGe.gDir)) << "\n";
    output << "genomeFastaFileCount\t1\n";
    output << "genomeFastaFile0Hex\t" << blackstarHexEncode(packagedFastaName) << "\n";
    output << "sjdbGTFfileHex\t" << blackstarHexEncode(P.pGe.sjdbGTFfile=="-" ? "-" : packagedGtfName) << "\n";
    output << "genomeInsertDeltaFileHex\t" << blackstarHexEncode(P.pGe.gInsertOutMode=="Delta" ? deltaFileName : "-") << "\n";
    output << "genomeInsertGTFhasJunctions\t" << (P.pGe.gInsertOverlayGTFhasJunctions ? 1 : 0) << "\n";
    output << "sjdbOverhang\t" << P.pGe.sjdbOverhang << "\n";
    output << "baseIndexSha256Tree\t" << P.pGe.gInsertBaseSha256 << "\n";
    output << "insertFastaSha256Tree\t" << P.pGe.gInsertFastaSha256 << "\n";
    output << "insertGtfSha256Tree\t" << P.pGe.gInsertGtfSha256 << "\n";
    output.close();
    if (output.fail()) {
        fail("EXITING because of fatal OUTPUT FILE error while writing " + path + "\n",
             EXIT_CODE_FILE_WRITE, P);
    }
    P.inOut->logMain << "Genome insert overlay staged: " << path << "\n";
}

void genomeInsertOutputFinalize(Parameters &P)
{
    const string stageDirectory=stripTrailingSlash(P.pGe.gInsertStageDir);
    const string finalDirectory=stripTrailingSlash(P.pGe.gInsertOutDirFinal);
    if (stageDirectory.empty() || finalDirectory.empty() || !directoryExists(stageDirectory)) {
        fail("EXITING because of fatal internal ERROR: genome insert output was not staged\n",
             EXIT_CODE_INCONSISTENT_DATA, P);
    }
    writeCompletionFile(stageDirectory, P.pGe.gInsertArtifactMode, P);
    struct stat existing;
    if (pathStat(finalDirectory, existing)) {
        fail("EXITING because of fatal OUTPUT FILE error: destination appeared during genome insert publication: " + finalDirectory + "\n",
             EXIT_CODE_FILE_WRITE, P);
    }
    if (rename(stageDirectory.c_str(), finalDirectory.c_str())!=0) {
        fail("EXITING because of fatal OUTPUT FILE error: atomic publication failed for " + finalDirectory
             + ": " + strerror(errno) + "\n", EXIT_CODE_FILE_WRITE, P);
    }
    stagingDirectoryToClean.clear();
    const size_t slash=finalDirectory.find_last_of('/');
    const string parent=slash==0 ? "/" : finalDirectory.substr(0, slash);
    fsyncPath(parent, true, P);
    P.inOut->logMain << "BlackSTAR genome insert artifact published atomically: " << finalDirectory << "\n";
}

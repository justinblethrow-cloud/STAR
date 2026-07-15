#include "BlackstarSha256.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
const uint32_t roundConstants[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

const uint64_t merkleChunkSize = 4ULL * 1024ULL * 1024ULL;
const char shaDomain[] = "BlackSTAR SHA-256 tree v1";

inline uint32_t rotateRight(uint32_t value, uint32_t bits)
{
    return (value >> bits) | (value << (32U - bits));
}

void updateUint64LE(BlackstarSha256 &sha, uint64_t value)
{
    unsigned char bytes[8];
    for (uint ii=0; ii<8; ++ii) {
        bytes[ii]=static_cast<unsigned char>((value >> (8U*ii)) & 0xffU);
    }
    sha.update(bytes, sizeof(bytes));
}

void updateLogicalRange(BlackstarSha256 &sha, const std::vector<BlackstarMemorySegment> &segments,
                        uint64_t offset, uint64_t length)
{
    uint64_t segmentStart=0;
    uint64_t remaining=length;
    for (std::vector<BlackstarMemorySegment>::const_iterator it=segments.begin();
         it!=segments.end() && remaining>0; ++it) {
        const uint64_t segmentEnd=segmentStart+it->size;
        if (offset<segmentEnd) {
            const uint64_t within=offset>segmentStart ? offset-segmentStart : 0;
            const uint64_t available=it->size-within;
            const uint64_t take=std::min(available, remaining);
            if (take>0) {
                sha.update(it->data+within, static_cast<std::size_t>(take));
                offset+=take;
                remaining-=take;
            }
        }
        segmentStart=segmentEnd;
    }
    if (remaining!=0) {
        throw std::runtime_error("internal error while hashing segmented data");
    }
}

int hexValue(char value)
{
    if (value>='0' && value<='9') return value-'0';
    if (value>='a' && value<='f') return value-'a'+10;
    if (value>='A' && value<='F') return value-'A'+10;
    return -1;
}
}

BlackstarSha256::BlackstarSha256()
    : totalBytes(0), bufferLength(0), finalized(false)
{
    state[0]=0x6a09e667U;
    state[1]=0xbb67ae85U;
    state[2]=0x3c6ef372U;
    state[3]=0xa54ff53aU;
    state[4]=0x510e527fU;
    state[5]=0x9b05688cU;
    state[6]=0x1f83d9abU;
    state[7]=0x5be0cd19U;
    std::memset(buffer, 0, sizeof(buffer));
}

void BlackstarSha256::transform(const unsigned char block[64])
{
    uint32_t words[64];
    for (uint ii=0; ii<16; ++ii) {
        words[ii]=(static_cast<uint32_t>(block[4*ii]) << 24)
                | (static_cast<uint32_t>(block[4*ii+1]) << 16)
                | (static_cast<uint32_t>(block[4*ii+2]) << 8)
                | static_cast<uint32_t>(block[4*ii+3]);
    }
    for (uint ii=16; ii<64; ++ii) {
        const uint32_t s0=rotateRight(words[ii-15], 7) ^ rotateRight(words[ii-15], 18) ^ (words[ii-15] >> 3);
        const uint32_t s1=rotateRight(words[ii-2], 17) ^ rotateRight(words[ii-2], 19) ^ (words[ii-2] >> 10);
        words[ii]=words[ii-16]+s0+words[ii-7]+s1;
    }

    uint32_t a=state[0], b=state[1], c=state[2], d=state[3];
    uint32_t e=state[4], f=state[5], g=state[6], h=state[7];
    for (uint ii=0; ii<64; ++ii) {
        const uint32_t sum1=rotateRight(e, 6) ^ rotateRight(e, 11) ^ rotateRight(e, 25);
        const uint32_t choice=(e & f) ^ ((~e) & g);
        const uint32_t temporary1=h+sum1+choice+roundConstants[ii]+words[ii];
        const uint32_t sum0=rotateRight(a, 2) ^ rotateRight(a, 13) ^ rotateRight(a, 22);
        const uint32_t majority=(a & b) ^ (a & c) ^ (b & c);
        const uint32_t temporary2=sum0+majority;
        h=g;
        g=f;
        f=e;
        e=d+temporary1;
        d=c;
        c=b;
        b=a;
        a=temporary1+temporary2;
    }

    state[0]+=a;
    state[1]+=b;
    state[2]+=c;
    state[3]+=d;
    state[4]+=e;
    state[5]+=f;
    state[6]+=g;
    state[7]+=h;
}

void BlackstarSha256::update(const void *dataIn, std::size_t length)
{
    if (finalized) {
        throw std::logic_error("cannot update a finalized SHA-256 context");
    }
    if (length==0) return;
    if (dataIn==NULL) {
        throw std::invalid_argument("cannot hash a null buffer with nonzero length");
    }
    if (length>std::numeric_limits<uint64_t>::max()-totalBytes) {
        throw std::overflow_error("SHA-256 input length overflow");
    }

    const unsigned char *data=static_cast<const unsigned char *>(dataIn);
    totalBytes+=length;
    while (length>0) {
        const std::size_t take=std::min(length, sizeof(buffer)-bufferLength);
        std::memcpy(buffer+bufferLength, data, take);
        bufferLength+=take;
        data+=take;
        length-=take;
        if (bufferLength==sizeof(buffer)) {
            transform(buffer);
            bufferLength=0;
        }
    }
}

std::array<unsigned char, 32> BlackstarSha256::final()
{
    if (finalized) {
        throw std::logic_error("SHA-256 context finalized more than once");
    }
    if (totalBytes>std::numeric_limits<uint64_t>::max()/8U) {
        throw std::overflow_error("SHA-256 input exceeds the supported bit length");
    }
    const uint64_t bitLength=totalBytes*8U;
    buffer[bufferLength++]=0x80U;
    if (bufferLength>56) {
        std::memset(buffer+bufferLength, 0, sizeof(buffer)-bufferLength);
        transform(buffer);
        bufferLength=0;
    }
    std::memset(buffer+bufferLength, 0, 56-bufferLength);
    for (uint ii=0; ii<8; ++ii) {
        buffer[63-ii]=static_cast<unsigned char>((bitLength >> (8U*ii)) & 0xffU);
    }
    transform(buffer);
    finalized=true;

    std::array<unsigned char, 32> result;
    for (uint ii=0; ii<8; ++ii) {
        result[4*ii]=static_cast<unsigned char>(state[ii] >> 24);
        result[4*ii+1]=static_cast<unsigned char>(state[ii] >> 16);
        result[4*ii+2]=static_cast<unsigned char>(state[ii] >> 8);
        result[4*ii+3]=static_cast<unsigned char>(state[ii]);
    }
    return result;
}

std::string BlackstarSha256::hex(const std::array<unsigned char, 32> &digest)
{
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (std::array<unsigned char, 32>::const_iterator it=digest.begin(); it!=digest.end(); ++it) {
        output << std::setw(2) << static_cast<unsigned int>(*it);
    }
    return output.str();
}

std::string blackstarMerkleSha256(const std::vector<BlackstarMemorySegment> &segments, uint64_t threadN)
{
    uint64_t totalSize=0;
    for (std::vector<BlackstarMemorySegment>::const_iterator it=segments.begin(); it!=segments.end(); ++it) {
        if (it->size>std::numeric_limits<uint64_t>::max()-totalSize) {
            throw std::overflow_error("Merkle SHA-256 input length overflow");
        }
        if (it->size>0 && it->data==NULL) {
            throw std::invalid_argument("null Merkle SHA-256 segment");
        }
        totalSize+=it->size;
    }

    const uint64_t chunkCount=totalSize==0 ? 1 : 1+(totalSize-1)/merkleChunkSize;
    std::vector<std::array<unsigned char, 32> > chunkDigests(chunkCount);
    const int threads=static_cast<int>(std::min<uint64_t>(
            std::min<uint64_t>(std::max<uint64_t>(1, threadN), chunkCount),
            static_cast<uint64_t>(std::numeric_limits<int>::max())));

    #pragma omp parallel for schedule(static) num_threads(threads)
    for (int64_t chunk=0; chunk<static_cast<int64_t>(chunkCount); ++chunk) {
        const uint64_t offset=static_cast<uint64_t>(chunk)*merkleChunkSize;
        const uint64_t length=offset<totalSize ? std::min(merkleChunkSize, totalSize-offset) : 0;
        BlackstarSha256 sha;
        sha.update(shaDomain, sizeof(shaDomain)-1);
        const char chunkDomain[]=" chunk";
        sha.update(chunkDomain, sizeof(chunkDomain)-1);
        updateUint64LE(sha, static_cast<uint64_t>(chunk));
        updateUint64LE(sha, length);
        updateLogicalRange(sha, segments, offset, length);
        chunkDigests[chunk]=sha.final();
    }

    BlackstarSha256 root;
    root.update(shaDomain, sizeof(shaDomain)-1);
    const char rootDomain[]=" root";
    root.update(rootDomain, sizeof(rootDomain)-1);
    updateUint64LE(root, totalSize);
    updateUint64LE(root, merkleChunkSize);
    updateUint64LE(root, chunkCount);
    for (uint64_t ii=0; ii<chunkCount; ++ii) {
        root.update(chunkDigests[ii].data(), chunkDigests[ii].size());
    }
    return BlackstarSha256::hex(root.final());
}

std::string blackstarFileMerkleSha256(const std::string &path, uint64_t threadN, uint64_t *sizeOut)
{
    const int fileDescriptor=open(path.c_str(), O_RDONLY);
    if (fileDescriptor<0) {
        throw std::runtime_error("could not open " + path + ": " + std::strerror(errno));
    }

    struct stat fileStat;
    if (fstat(fileDescriptor, &fileStat)!=0 || !S_ISREG(fileStat.st_mode) || fileStat.st_size<0) {
        const std::string error=std::strerror(errno);
        close(fileDescriptor);
        throw std::runtime_error("could not stat regular file " + path + ": " + error);
    }
    const uint64_t size=static_cast<uint64_t>(fileStat.st_size);
    if (size>std::numeric_limits<std::size_t>::max()) {
        close(fileDescriptor);
        throw std::runtime_error("file is too large to map in this process: " + path);
    }
    if (sizeOut!=NULL) *sizeOut=size;

    void *mapping=NULL;
    if (size>0) {
        mapping=mmap(NULL, static_cast<std::size_t>(size), PROT_READ, MAP_PRIVATE, fileDescriptor, 0);
        if (mapping==MAP_FAILED) {
            const std::string error=std::strerror(errno);
            close(fileDescriptor);
            throw std::runtime_error("could not map " + path + ": " + error);
        }
    }

    std::vector<BlackstarMemorySegment> segments;
    segments.push_back(BlackstarMemorySegment(mapping, size));
    std::string digest;
    try {
        digest=blackstarMerkleSha256(segments, threadN);
    } catch (...) {
        if (size>0) munmap(mapping, static_cast<std::size_t>(size));
        close(fileDescriptor);
        throw;
    }
    if (size>0) munmap(mapping, static_cast<std::size_t>(size));
    close(fileDescriptor);
    return digest;
}

std::string blackstarHexEncode(const std::string &value)
{
    static const char digits[]="0123456789abcdef";
    std::string encoded;
    encoded.reserve(value.size()*2);
    for (std::string::const_iterator it=value.begin(); it!=value.end(); ++it) {
        const unsigned char byte=static_cast<unsigned char>(*it);
        encoded.push_back(digits[byte >> 4]);
        encoded.push_back(digits[byte & 0x0f]);
    }
    return encoded;
}

bool blackstarHexDecode(const std::string &encoded, std::string &value)
{
    if ((encoded.size() & 1U)!=0) return false;
    value.clear();
    value.reserve(encoded.size()/2);
    for (std::size_t ii=0; ii<encoded.size(); ii+=2) {
        const int high=hexValue(encoded[ii]);
        const int low=hexValue(encoded[ii+1]);
        if (high<0 || low<0) {
            value.clear();
            return false;
        }
        value.push_back(static_cast<char>((high << 4) | low));
    }
    return true;
}

bool blackstarIsSha256(const std::string &value)
{
    if (value.size()!=64) return false;
    for (std::string::const_iterator it=value.begin(); it!=value.end(); ++it) {
        if (hexValue(*it)<0) return false;
    }
    return true;
}

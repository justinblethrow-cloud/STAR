#ifndef CODE_BlackstarSha256
#define CODE_BlackstarSha256

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class BlackstarSha256 {
public:
    BlackstarSha256();

    void update(const void *data, std::size_t length);
    std::array<unsigned char, 32> final();

    static std::string hex(const std::array<unsigned char, 32> &digest);

private:
    void transform(const unsigned char block[64]);

    uint32_t state[8];
    uint64_t totalBytes;
    unsigned char buffer[64];
    std::size_t bufferLength;
    bool finalized;
};

struct BlackstarMemorySegment {
    const unsigned char *data;
    uint64_t size;

    BlackstarMemorySegment(const void *dataIn, uint64_t sizeIn)
        : data(static_cast<const unsigned char *>(dataIn)), size(sizeIn) {}
};

// Domain-separated tree hashing permits deterministic parallel hashing of the
// multi-gigabyte STAR index arrays without adding an external crypto library.
std::string blackstarMerkleSha256(const std::vector<BlackstarMemorySegment> &segments, uint64_t threadN);
std::string blackstarFileMerkleSha256(const std::string &path, uint64_t threadN, uint64_t *sizeOut = NULL);

std::string blackstarHexEncode(const std::string &value);
bool blackstarHexDecode(const std::string &encoded, std::string &value);
bool blackstarIsSha256(const std::string &value);

#endif

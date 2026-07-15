#include "BlackstarSha256.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

namespace {
void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}

std::string sha256(const std::string &value)
{
    BlackstarSha256 sha;
    sha.update(value.data(), value.size());
    return BlackstarSha256::hex(sha.final());
}

void writeAll(int descriptor, const unsigned char *data, std::size_t size)
{
    while (size>0) {
        const ssize_t written=write(descriptor, data, size);
        if (written<0) {
            if (errno==EINTR) continue;
            throw std::runtime_error(std::string("write failed: ")+std::strerror(errno));
        }
        data+=written;
        size-=static_cast<std::size_t>(written);
    }
}
}

int main(int argc, char **argv)
{
    try {
        if (argc==3 && std::string(argv[1])=="--file") {
            std::cout << blackstarFileMerkleSha256(argv[2], 4) << '\n';
            return 0;
        }
        require(argc==1, "usage: testBlackstarSha256 [--file PATH]");
        require(sha256("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
                "empty SHA-256 vector mismatch");
        require(sha256("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                "abc SHA-256 vector mismatch");

        BlackstarSha256 incremental;
        incremental.update("a", 1);
        incremental.update("b", 1);
        incremental.update("c", 1);
        require(BlackstarSha256::hex(incremental.final())==sha256("abc"),
                "incremental SHA-256 mismatch");
        bool finalizedRejected=false;
        try {
            incremental.update("x", 1);
        } catch (const std::logic_error &) {
            finalizedRejected=true;
        }
        require(finalizedRejected, "update after finalization was accepted");

        std::string binary("a\0b\xff", 4);
        const std::string encoded=blackstarHexEncode(binary);
        std::string decoded;
        require(encoded=="610062ff" && blackstarHexDecode(encoded, decoded) && decoded==binary,
                "hex round trip mismatch");
        require(!blackstarHexDecode("0xz1", decoded), "invalid hex was accepted");
        require(blackstarIsSha256(sha256("abc")) && !blackstarIsSha256("abc"),
                "SHA-256 syntax validation mismatch");

        std::vector<unsigned char> data(8*1024*1024+97);
        for (std::size_t ii=0; ii<data.size(); ++ii) data[ii]=static_cast<unsigned char>((ii*37U+11U) & 0xffU);
        std::vector<BlackstarMemorySegment> contiguous;
        contiguous.push_back(BlackstarMemorySegment(data.data(), data.size()));
        const std::string oneThread=blackstarMerkleSha256(contiguous, 1);
        const std::string eightThreads=blackstarMerkleSha256(contiguous, 8);
        require(oneThread==eightThreads, "Merkle identity depends on thread count");

        std::vector<BlackstarMemorySegment> segmented;
        segmented.push_back(BlackstarMemorySegment(data.data(), 17));
        segmented.push_back(BlackstarMemorySegment(data.data()+17, 4*1024*1024+3));
        segmented.push_back(BlackstarMemorySegment(NULL, 0));
        segmented.push_back(BlackstarMemorySegment(data.data()+4*1024*1024+20,
                                                    data.size()-(4*1024*1024+20)));
        require(blackstarMerkleSha256(segmented, 4)==oneThread,
                "Merkle identity depends on segment boundaries");

        char fileTemplate[]="/tmp/blackstar-sha256-test.XXXXXX";
        const int descriptor=mkstemp(fileTemplate);
        require(descriptor>=0, "mkstemp failed");
        try {
            writeAll(descriptor, data.data(), data.size());
            require(close(descriptor)==0, "close failed");
            uint64_t fileSize=0;
            require(blackstarFileMerkleSha256(fileTemplate, 6, &fileSize)==oneThread,
                    "file and memory Merkle identities differ");
            require(fileSize==data.size(), "file identity reported the wrong size");
        } catch (...) {
            close(descriptor);
            unlink(fileTemplate);
            throw;
        }
        unlink(fileTemplate);

        std::vector<BlackstarMemorySegment> empty;
        require(blackstarIsSha256(blackstarMerkleSha256(empty, 2)),
                "empty Merkle identity is malformed");
    } catch (const std::exception &error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
    std::cout << "BlackSTAR SHA-256 tests passed\n";
    return 0;
}

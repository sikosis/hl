#include "io/input.h"

#include <array>
#include <cerrno>
#include <fstream>
#include <istream>
#include <ostream>
#include <streambuf>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace hl::io {
namespace {

constexpr std::size_t kBufferSize = 256 * 1024;

class DescriptorInputBuffer final : public std::streambuf {
public:
    explicit DescriptorInputBuffer(int descriptor)
        : descriptor_(descriptor)
    {
        setg(buffer_.data(), buffer_.data(), buffer_.data());
    }
//---------------------------------------------------------------------------------------------------------------------------------//

protected:
    int_type underflow() override
    {
        ssize_t count;
        do {
            count = read(descriptor_, buffer_.data(), buffer_.size());
        } while (count < 0 && errno == EINTR);
        if (count <= 0) {
            return traits_type::eof();
        }
        setg(buffer_.data(), buffer_.data(), buffer_.data() + count);
        return traits_type::to_int_type(*gptr());
    }
//---------------------------------------------------------------------------------------------------------------------------------//

private:
    int descriptor_;
    std::array<char, kBufferSize> buffer_ {};
};

const char* decompressorForPath(const std::string& path)
{
    if (path.size() >= 3 && path.substr(path.size() - 3) == ".gz") return "gzip";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".bz2") return "bzip2";
    if (path.size() >= 3 && path.substr(path.size() - 3) == ".xz") return "xz";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".zst") return "zstd";
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".zstd") return "zstd";
    return nullptr;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool visitCompressed(
    const std::string& path,
    const char* command,
    std::ostream& error,
    const StreamHandler& handler)
{
    int descriptors[2];
    if (pipe(descriptors) != 0) {
        error << "hl: could not create decompressor pipe for '" << path << "'\n";
        return false;
    }
    const pid_t child = fork();
    if (child < 0) {
        close(descriptors[0]);
        close(descriptors[1]);
        error << "hl: could not start decompressor for '" << path << "'\n";
        return false;
    }
    if (child == 0) {
        close(descriptors[0]);
        if (dup2(descriptors[1], STDOUT_FILENO) < 0) _exit(127);
        close(descriptors[1]);
        if (std::string_view(command) == "zstd") {
            execlp(command, command, "-q", "-c", "-d", "--", path.c_str(), static_cast<char*>(nullptr));
        } else {
            execlp(command, command, "-c", "-d", "--", path.c_str(), static_cast<char*>(nullptr));
        }
        _exit(127);
    }

    close(descriptors[1]);
    DescriptorInputBuffer buffer(descriptors[0]);
    std::istream input(&buffer);
    const bool handled = handler(input);
    close(descriptors[0]);
    int status = 0;
    pid_t result;
    do {
        result = waitpid(child, &status, 0);
    } while (result < 0 && errno == EINTR);
    if (!handled || result < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        error << "hl: could not decompress '" << path << "'\n";
        return false;
    }
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool writePath(
    const std::string& path,
    std::istream& standardInput,
    std::ostream& error,
    const StreamHandler& handler)
{
    if (path == "-") {
        if (!handler(standardInput)) {
            error << "hl: could not process stdin\n";
            return false;
        }
        return true;
    }

    const char* decompressor = decompressorForPath(path);
    if (decompressor != nullptr) {
        return visitCompressed(path, decompressor, error, handler);
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error << "hl: could not open '" << path << "'\n";
        return false;
    }

    if (!handler(input)) {
        error << "hl: could not process '" << path << "'\n";
        return false;
    }
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace

bool copyStream(std::istream& input, std::ostream& output)
{
    std::array<char, kBufferSize> buffer {};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize bytesRead = input.gcount();
        if (bytesRead > 0) {
            output.write(buffer.data(), bytesRead);
            if (!output) {
                return false;
            }
        }
    }

    return input.eof();
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool visitInputs(
    const std::vector<std::string>& paths,
    std::istream& standardInput,
    std::ostream& error,
    const StreamHandler& handler)
{
    bool succeeded = true;
    if (paths.empty()) {
        return writePath("-", standardInput, error, handler);
    }

    for (const std::string& path : paths) {
        if (!writePath(path, standardInput, error, handler)) {
            succeeded = false;
        }
    }
    return succeeded;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool writeInputs(
    const std::vector<std::string>& paths,
    std::istream& standardInput,
    std::ostream& output,
    std::ostream& error)
{
    const StreamHandler copy = [&](std::istream& input) {
        return copyStream(input, output);
    };
    return visitInputs(paths, standardInput, error, copy);
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace hl::io

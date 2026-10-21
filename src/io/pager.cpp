#include "io/pager.h"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <streambuf>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace hl::io {

class Pager::Buffer final : public std::streambuf {
public:
    Buffer(int descriptor, bool& closedEarly)
        : descriptor_(descriptor)
        , closedEarly_(closedEarly)
    {
    }
//---------------------------------------------------------------------------------------------------------------------------------//

protected:
    std::streamsize xsputn(const char* data, std::streamsize size) override
    {
        std::streamsize written = 0;
        while (written < size) {
            const ssize_t result = write(
                descriptor_, data + written, static_cast<std::size_t>(size - written));
            if (result > 0) {
                written += result;
                continue;
            }
            if (result < 0 && errno == EINTR) {
                continue;
            }
            if (result < 0 && errno == EPIPE) {
                closedEarly_ = true;
            }
            break;
        }
        return written;
    }
//---------------------------------------------------------------------------------------------------------------------------------//

    int overflow(int character) override
    {
        if (character == traits_type::eof()) {
            return traits_type::not_eof(character);
        }
        const char value = static_cast<char>(character);
        return xsputn(&value, 1) == 1 ? character : traits_type::eof();
    }
//---------------------------------------------------------------------------------------------------------------------------------//

private:
    int descriptor_;
    bool& closedEarly_;
};

Pager::Pager() = default;
//---------------------------------------------------------------------------------------------------------------------------------//

Pager::~Pager()
{
    finish();
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool Pager::start(const std::string& command, std::string& error)
{
    int descriptors[2];
    if (pipe(descriptors) != 0) {
        error = std::strerror(errno);
        return false;
    }

    const pid_t child = fork();
    if (child < 0) {
        error = std::strerror(errno);
        close(descriptors[0]);
        close(descriptors[1]);
        return false;
    }
    if (child == 0) {
        close(descriptors[1]);
        if (dup2(descriptors[0], STDIN_FILENO) < 0) {
            _exit(127);
        }
        close(descriptors[0]);
        if (std::getenv("LESS") == nullptr) {
            setenv("LESS", "-R", 1);
        }
        execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }

    close(descriptors[0]);
    std::signal(SIGPIPE, SIG_IGN);
    writeDescriptor_ = descriptors[1];
    processId_ = static_cast<int>(child);
    buffer_ = std::make_unique<Buffer>(writeDescriptor_, closedEarly_);
    output_ = std::make_unique<std::ostream>(buffer_.get());
    error.clear();
    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//

std::ostream& Pager::output()
{
    return *output_;
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool Pager::finish()
{
    if (writeDescriptor_ < 0) {
        return true;
    }
    output_->flush();
    output_.reset();
    buffer_.reset();
    close(writeDescriptor_);
    writeDescriptor_ = -1;

    int status = 0;
    pid_t result;
    do {
        result = waitpid(static_cast<pid_t>(processId_), &status, 0);
    } while (result < 0 && errno == EINTR);
    processId_ = -1;
    return result >= 0 && (closedEarly_ || (WIFEXITED(status) && WEXITSTATUS(status) == 0));
}
//---------------------------------------------------------------------------------------------------------------------------------//

bool Pager::closedEarly() const
{
    return closedEarly_;
}
//---------------------------------------------------------------------------------------------------------------------------------//

} // namespace hl::io

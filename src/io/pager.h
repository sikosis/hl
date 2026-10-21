#ifndef HL_IO_PAGER_H
#define HL_IO_PAGER_H

#include <memory>
#include <ostream>
#include <string>

namespace hl::io {

class Pager {
public:
    Pager();
    ~Pager();

    Pager(const Pager&) = delete;
    Pager& operator=(const Pager&) = delete;

    bool start(const std::string& command, std::string& error);
    std::ostream& output();
    bool finish();
    bool closedEarly() const;

private:
    class Buffer;

    int writeDescriptor_ = -1;
    int processId_ = -1;
    bool closedEarly_ = false;
    std::unique_ptr<Buffer> buffer_;
    std::unique_ptr<std::ostream> output_;
};

} // namespace hl::io

#endif

#include "io.h"
#include <unistd.h>

namespace scgi {
    FileReadBuffer::FileReadBuffer(int d, std::size_t chunk_size)
            : fd(d), chunk_(chunk_size), buffer_(chunk_size) { }

    std::streambuf::int_type FileReadBuffer::underflow() {
        if (gptr() < egptr())  // buffer not exhausted
            return traits_type::to_int_type(*gptr());
        if (fd < 0) traits_type::eof();
        ssize_t n = read(fd, buffer_.data(), chunk_);
        if (n <= 0) return traits_type::eof();
        char *base = &buffer_.front();
        char *start = base;
        setg(base, start, start + n);
        return traits_type::to_int_type(*gptr());
    }

    FileWriteBuffer::FileWriteBuffer(int d, std::size_t chunk_size)
            : fd(d), chunk_(chunk_size), buffer_(chunk_size) { }

    std::streambuf::int_type FileWriteBuffer::overflow(
            std::streambuf::int_type ch) {
        if (fd < 0) traits_type::eof();
        if (ch == traits_type::eof()) {
            sync();
            return traits_type::eof();
        }
        buffer_[count_++] = ch;
        if (count_ >= chunk_) return sync();
        return ch;
    }

    int FileWriteBuffer::sync() {
        ssize_t part = 1;
        size_t count = 0;
        while (count < count_) {
            part = write(fd, &buffer_[count], count_ - count);
            if (part <= 0) return traits_type::eof();
            count += static_cast<size_t>(part);
        }
        count_ = 0;
        return 0;
    }
}

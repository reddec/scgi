#include "io.h"
#include <unistd.h>
#include <netdb.h>
#include <memory>
#include <cstring>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/stat.h>

namespace scgi {
    FileReadBuffer::FileReadBuffer(int d, std::size_t chunk_size)
            : fd(d), chunk_(chunk_size), buffer_(chunk_size) {
    }

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
            : fd(d), chunk_(chunk_size), buffer_(chunk_size) {
    }

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

    ConnectionManager::~ConnectionManager() {

    }

    void AbstractSocketManager::stop() {
        if (descriptor > 0) {
            ::close(descriptor);
            descriptor = -1;
        }
    }

    AbstractSocketManager::~AbstractSocketManager() {
        stop();
    }

    int AbstractSocketManager::next_descriptor() {
        if (!is_active()) return -1;
        int client = ::accept(descriptor, nullptr, nullptr);
        if (client < 0) {
            perror("accept");
            return -1;
        }
        return client;
    }

    TcpServerManager::TcpServerManager(const std::string &service, const std::string &bind_host, int backlog) {
        addrinfo *info;
        descriptor = socket(AF_INET6, SOCK_STREAM, 0);
        if (descriptor < 0) return;
        if (getaddrinfo(bind_host.c_str(), service.c_str(), nullptr, &info) < 0) {
            perror("getaddrinfo");
            stop();
            return;
        }
        std::unique_ptr<addrinfo, void (*)(addrinfo *)> infoptr(info, freeaddrinfo);
        if (bind(descriptor, info->ai_addr, info->ai_addrlen) < 0) {
            perror("bind");
            stop();
            return;
        }
        int opt = 1;
        if (setsockopt(descriptor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
            perror("setsockopt");
            stop();
            return;
        }
        if (listen(descriptor, backlog) < 0) {
            perror("listen");
            stop();
            return;
        };
    }

    UnixServerManager::UnixServerManager(const std::string &path, int backlog, uint32_t mode) {
        struct sockaddr_un address;
        descriptor = socket(AF_UNIX, SOCK_STREAM, 0);
        if (descriptor < 0) {
            perror("socket\n");
            return;
        }
        memset(&address, 0, sizeof(struct sockaddr_un));
        address.sun_family = AF_UNIX;
        std::memmove(address.sun_path, path.c_str(), std::min(path.size() + 1, sizeof(address.sun_path)));
        if (bind(descriptor, (struct sockaddr *) &address, sizeof(struct sockaddr_un)) < 0) {
            perror("bind\n");
            stop();
            return;
        }

        if (listen(descriptor, backlog) < 0) {
            perror("listen\n");
            stop();
            return;
        }

        if (chmod(path.c_str(), mode) < 0) {
            perror("chmod");
            stop();
            return;
        }
    }

    ConnectionManager::Ptr TcpServerManager::create(const std::string &service, std::string const &bind_host,
                                                    int backlog) {
        return std::make_shared<TcpServerManager>(service, bind_host, backlog);
    }

    ConnectionManager::Ptr UnixServerManager::create(const std::string &path, int backlog, uint32_t mode) {
        return std::make_shared<UnixServerManager>(path, backlog, mode);
    }

    void UnixServerManager::stop() {
        if (is_active()) {
            AbstractSocketManager::stop();
            if (unlink(path_.c_str()) < 0)
                perror("unlink");
        }
    }
}

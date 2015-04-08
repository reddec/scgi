#include "scgi.h"
#include <string>
#include <unistd.h>
#include <sstream>
#include <netdb.h>

#ifndef  SCGI_VERSION
#define SCGI_VERSION "0.0.0"
#endif

namespace scgi {

    const std::string Request::HeaderQuery = "QUERY_STRING";
    const std::string Request::HeaderContentLength = "CONTENT_LENGTH";
    const std::string Request::HeaderPath = "PATH_INFO";
    const std::string Request::HeaderMethod = "REQUEST_METHOD";

    const std::string Request::ContentTypeTextPlain = "text/plain";
    const std::string Request::ContentTypeTextHtml = "text/html";
    const std::string Request::ContentTypeApplicationJson = "application/json";
    const std::string Request::ContentTypeApplicationXml = "application/xml";

    const std::string Request::ResponseHeaderContentType = "Content-Type";

    const std::string  Request::StatusMessageOK = "OK";
    const std::string  Request::StatusMessageNotFound = "Not Found";
    const std::string  Request::StatusMessageInternalError = "Internal Server Error";

    Request::Request(int fd, uint64_t id)
            : id_(id),
              sock(fd),
              r_input(fd),
              r_output(fd),
              input_(&r_input),
              output_(&r_output) {
        size_t header_length, total = 0;
        std::string key, value;
        // Parse SCGI header size
        input_ >> header_length;
        // Skip delimiter
        input_.ignore();
        // Read SCGI key-values
        while (total < header_length && !input_.eof()) {
            std::getline(input_, key, '\0');
            std::getline(input_, value, '\0');
            headers[key] = value;
            total += 2 + key.size() + value.size();
        }
        input_.ignore();//comma
        // Parse URL query
        std::string query_str = headers[HeaderQuery];
        std::string::size_type li, pi = 0, sep;
        do {
            li = query_str.find('&', pi);
            sep = query_str.find('=', pi);
            key = query_str.substr(pi, sep - pi);
            value = query_str.substr(sep + 1, li - sep - 1);
            query[Utils::url_decode(key)] = Utils::url_decode(value);
            pi = li + 1;
        } while (li != std::string::npos);
        // Cache useful headers
        content_size_ =
                static_cast<size_t>(std::atol(headers[HeaderContentLength].c_str()));
        path_ = headers[HeaderPath];
        method_ = headers[HeaderMethod];
    }

    Request::~Request() {
        output_.flush();
        close(sock);
    }

    SimpleAcceptor::SimpleAcceptor(const std::string &service,
                                   const std::string &ip, int backlog) {
        addrinfo *info;
        valid = false;
        sock = socket(AF_INET6, SOCK_STREAM, 0);
        if (sock < 0) return;
        if (getaddrinfo(ip.c_str(), service.c_str(), nullptr, &info) < 0) {
            perror("getaddrinfo");
            return;
        }
        std::unique_ptr<addrinfo, void (*)(addrinfo *)> infoptr(info, freeaddrinfo);
        if (bind(sock, info->ai_addr, info->ai_addrlen) < 0) {
            perror("bind");
            return;
        }
        int opt = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
            perror("setsockopt");
            return;
        }
        listen(sock, backlog);
        valid = true;
    }

    RequestPtr SimpleAcceptor::accept() {
        if (!operator bool()) return nullptr;
        int client = ::accept(sock, nullptr, nullptr);
        if (client < 0) {
            perror("accept");
            return nullptr;
        }
        return std::make_shared<Request>(client, request_id_++);
    }


    void Request::begin_response(int code, std::string const &message) {
        output_ << "Status: " << code << " " << message << "\r\n";
        for (auto &kv:response_headers) {
            output_ << kv.first << ": " << kv.second << "\r\n";
        }
        output_ << "\r\n";
    }

    void Request::set_response_type(std::string const &type) {
        response_headers[ResponseHeaderContentType] = type;
    }


    std::string Utils::url_decode(const std::string &s) {
        std::stringstream ss;
        char hex[3];
        hex[2] = '\0';
        for (size_t i = 0; i < s.size(); ++i) {
            char c = s[i];
            if (c == '%') {
                hex[0] = i + 1 < s.size() && std::isalnum(s[i + 1]) ? s[i + 1] : '0';
                hex[1] = i + 2 < s.size() && std::isalnum(s[i + 1]) ? s[i + 2] : '0';
                ss << (char) std::stoi(hex, nullptr, 16);
                i += 2;
            } else
                ss << c;
        }
        return ss.str();
    }

    const std::string &version() {
        static std::string version(SCGI_VERSION);
        return version;
    }
};
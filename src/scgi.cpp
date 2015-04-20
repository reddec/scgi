#include "scgi.h"
#include <string>
#include <unistd.h>
#include <sstream>
#include <netdb.h>

#ifndef  SCGI_VERSION
#define SCGI_VERSION "0.0.0"
#endif

namespace scgi {

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
        std::string query_str = headers[header::query];
        std::string::size_type li, pi = 0, sep;
        do {
            li = query_str.find('&', pi);
            sep = query_str.find('=', pi);
            key = query_str.substr(pi, sep - pi);
            value = query_str.substr(sep + 1, li - sep - 1);
            query[http::url_decode(key)] = http::url_decode(value);
            pi = li + 1;
        } while (li != std::string::npos);
        // Cache useful headers
        content_size_ =
                static_cast<size_t>(std::atol(headers[header::content_length].c_str()));
        path_ = headers[header::path];
        method_ = headers[header::method];
        valid = true;
    }

    Request::~Request() {
        output_.flush();
        close(sock);
    }

    SimpleAcceptor::SimpleAcceptor(std::shared_ptr<ConnectionManager> connection_manager) :
            connection_manager_(connection_manager) {
    }

    RequestPtr SimpleAcceptor::accept() {
        if (!connection_manager_) return nullptr;
        int client = connection_manager_->next_descriptor();
        if (client < 0) return nullptr;
        return std::make_shared<Request>(client, request_id_++);
    }


    void Request::begin_response(int code, std::string const &message) {
        output_ << "Status: " << code << " " << message << "\r\n";
        for (auto &kv:response_headers) {
            output_ << kv.first << ": " << kv.second << "\r\n";
        }
        output_ << "\r\n";
    }

    void Request::begin_response(http::Status status, std::string const &message) {
        begin_response((int) status, message);
    }

    void Request::set_response_type(std::string const &type) {
        response_headers[http::header::content_type] = type;
    }

    bool Request::parse_data(std::unordered_map<std::string, std::string> &result, http::EncodingType encodingType) {
        if (content_size() <= 0 || !is_valid() || input_.eof()) return false;
        //Test supported
        if (encodingType != http::EncodingType::x_www_form_urlencoded) return false;
        switch (encodingType) {
            case http::EncodingType::x_www_form_urlencoded: {
                std::vector<char> buffer(content_size() + 1);
                buffer.back() = '\0';
                input_.read(buffer.data(), buffer.size() - 1);
                std::stringstream ss(buffer.data());
                http::parse_http_urlencoded_form(ss, result);
                break;
            };
            default:
                return false;
        }
        return true;
    }


    const std::string &version() {
        static std::string version(SCGI_VERSION);
        return version;
    }


};
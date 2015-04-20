#ifndef SCGI
#define SCGI

#include <memory>
#include <unordered_map>
#include <iostream>
#include <cctype>
#include <cstdint>
#include <string>
#include <functional>
#include <set>
#include "io.h"
#include "http.h"

namespace scgi {

    typedef std::unordered_map<std::string, std::string> Headers;

    /**
     * Version of SCGI library
     */
    const std::string &version();

    /**
     * Collection of helpfull utils
     */
    struct Utils {

        /**
         * Read chars from `in` stream to `out` stream if `func` returns true.
         * Stops when `func` return false or count of written char equal `max`
         *
         * Returns count of written bytes
         */
        template<class Functor>
        static size_t read_allowed(std::istream &in, std::ostream &out, const Functor &func,
                                   size_t max = std::string::npos) {
            char c;
            size_t reads = 0;
            while (!in.eof() && reads < max) {
                in.get(c);
                if (in.eof() || !func(c)) break;
                out << (char) c;
                ++reads;
            }
            return reads;
        }

        /**
         * Skip chars from `in` stream while `func` returns true
         *
         * Returns count of written bytes
         */
        template<class Functor>
        static size_t skip(std::istream &in, const Functor &func) {
            char c;
            size_t reads = 0;
            while (!in.eof()) {
                in.get(c);
                if (in.eof() || !func(c)) break;
                ++reads;
            }
            return reads;
        }


    };

    namespace header {
        /**
         * Base SCGI headers
         */
        static const std::string query = "QUERY_STRING";
        static const std::string content_length = "CONTENT_LENGTH";
        static const std::string path = "PATH_INFO";
        static const std::string method = "REQUEST_METHOD";
    }

    /**
     * SCGI request class
     */
    struct Request {

        //Response headers which will be sent on `begin_response` function
        std::unordered_map<std::string, std::string> response_headers;
        //Incoming request headers
        std::unordered_map<std::string, std::string> headers;
        //Parsed request query options from URI
        std::unordered_map<std::string, std::string> query;

        /**
         * Allocates I/O streams for `fd` descriptor. Automatically closes descriptor in destructor.
         * Sets request id to `id`
         */
        Request(int fd, uint64_t id = 0);

        /**
         * Content length from request headers. Cached value.
         */
        inline long content_size() const {
            return content_size_;
        }

        /**
         * Status of request. Invalid state may be caused by wrong parsing of bad descriptor
         */
        inline bool is_valid() const {
            return valid && sock >= 0;
        }

        /**
         * Request ID. May be useful for future identification.
         */
        inline uint64_t id() const {
            return id_;
        }

        /**
         * Request path relative to bound script. Cached value. Starts from /
         */
        inline std::string path() const {
            return path_;
        }

        /**
         * Request HTTP method. Cached value
         */
        inline std::string method() const {
            return method_;
        }

        /**
         * Request file(socket) descriptor
         */
        inline int descriptor() const {
            return sock;
        }

        /**
         * Send HTTP headers and status. Use it before writing any data
         */
        void begin_response(int code = (int) http::Status::OK, const std::string &message = http::status_message::ok);

        /**
         * Send HTTP headers and status with predefined status code. Use it before writing any data
         */
        void begin_response(http::Status status, const std::string &message = http::status_message::ok);

        /**
         * Set Content-Type header in response.
         */
        void set_response_type(const std::string &type = http::content_type::text_plain);


        /**
         * Parse content into result.
         */
        bool parse_data(std::unordered_map<std::string, std::string> &result,
                        http::EncodingType encodingType = http::EncodingType::x_www_form_urlencoded);

        /**
         * Write data to remote side. Returns buffered output stream
         */
        template<class T>
        inline std::ostream &operator<<(const T &obj) {
            output << obj;
            return output_;
        }

        /**
         * Read data from remote side. Returns buffered input stream
         */
        template<class T>
        inline std::istream &operator>>(T &obj) {
            input >> obj;
            return input_;
        }

        /**
         * Buffered input stream
         */
        inline std::istream &input() {
            return input_;
        }

        /**
         * Buffered output stream
         */
        inline std::ostream &output() {
            return output_;
        }

        /**
         * Close descriptor
         */
        ~Request();

    private:
        uint64_t id_;
        std::string path_, method_;
        int sock;
        FileReadBuffer r_input;
        FileWriteBuffer r_output;
        std::istream input_;
        std::ostream output_;
        bool valid = false;
        size_t content_size_;

    };

    typedef std::shared_ptr<Request> RequestPtr;

    /**
     * Single threaded TCP SCGI acceptor based on shared pointers
     */
    struct SimpleAcceptor {

        /**
         * Create server TCP6 socket, bind (by `ip` and tcp `service` as port) and listen with fixed `backlog` size.
         * By default it bounds to all interfaces
         */
        SimpleAcceptor(std::shared_ptr<ConnectionManager> connection_manager);

        /**
        * Get active connection manager
        */
        inline std::shared_ptr<ConnectionManager> connection_manager() const {
            return connection_manager_;
        }

        /**
         * Accept new TCP connection and wraps it to SCGI Request. Returns nullptr on error or in invalid state
         */
        RequestPtr accept();

    private:
        std::shared_ptr<ConnectionManager> connection_manager_;
        uint64_t request_id_ = 0;
    };
}
#endif  // SCGI

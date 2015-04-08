#ifndef IO
#define IO

#include <streambuf>
#include <vector>

namespace scgi {
    /**
     * Reader from file descriptor. If descriptor less then 0 or `read` returns less or equal 0, EOF will be set.
     * This class doesn't close descriptor automatically
     */
    struct FileReadBuffer : public std::streambuf {

        /**
         * Initialize internal buffer for descriptor `d`. Single portion of incoming data has size `chunk_size`
         */
        explicit FileReadBuffer(int d, std::size_t chunk_size = 8192);

        /**
         * Get active file descriptor
         */
        inline int descriptor() const { return fd; }

    private:
        int_type underflow();

        FileReadBuffer(const FileReadBuffer &) = delete;

        FileReadBuffer &operator=(const FileReadBuffer &) = delete;

        int fd;
        std::size_t chunk_;
        std::vector<char> buffer_;
    };

    /**
     *  Writer to file descriptor. If descriptor less then 0 or `write` returns less or equal 0, EOF will be set.
     *  his class doesn't close descriptor automatically
     */
    struct FileWriteBuffer : public std::streambuf {
    public:

        /**
         * Initialize internal buffer for descriptor `d`. Single portion of outgoing data has size`chunk_size`
         */
        explicit FileWriteBuffer(int d, std::size_t chunk_size = 8192);

        /**
         *  Get active file descriptor
         */
        inline int descriptor() const { return fd; }

    private:
        FileWriteBuffer(const FileWriteBuffer &) = delete;

        FileWriteBuffer &operator=(const FileWriteBuffer &) = delete;

        int_type overflow(int_type ch);

        int sync();

        int fd;
        std::size_t chunk_, count_ = 0;
        std::vector<char> buffer_;
    };
}
#endif  // IO

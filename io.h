#ifndef IO
#define IO

#include <streambuf>
#include <vector>
#include <memory>

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
        inline int descriptor() const {
            return fd;
        }

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
        inline int descriptor() const {
            return fd;
        }

    private:
        FileWriteBuffer(const FileWriteBuffer &) = delete;

        FileWriteBuffer &operator=(const FileWriteBuffer &) = delete;

        int_type overflow(int_type ch);

        int sync();

        int fd;
        std::size_t chunk_, count_ = 0;
        std::vector<char> buffer_;
    };

    /**
    * Connection manager for new requests. For example via Unix or Tcp socket
    */
    struct ConnectionManager {
        /**
        * Returns current sate of connectionn manager
        */
        virtual bool is_active() = 0;

        /**
        * Wait for new file descriptor. Returns -1 on error
        */
        virtual int next_descriptor() = 0;

        virtual ~ConnectionManager();

        typedef std::shared_ptr<ConnectionManager> Ptr;
    };


    /**
    * Abstract socket functional. ::close and ::accept
    */
    struct AbstractSocketManager : public ConnectionManager {

        /**
        * Accept new client or returns -1 on error
        */
        virtual int next_descriptor() override;

        /**
        * Close descriptor
        */
        virtual void stop();

        /**
        * Calls stop()
        */
        virtual  ~AbstractSocketManager();

        /**
        * Returns true if descriptor has been created and wasn't closed yet
        */
        virtual inline bool is_active() override {
            return descriptor > 0;
        }

        /**
         * Set accept timeout or -1 (uint64_t max) for infinity loop.
         * Return true if operation done. Otherwise prints error and returns false;
         */
        bool set_accept_timeout(uint64_t milliseconds = -1);

        inline uint64_t accept_timeout() const { return timeout_; }

    protected:
        uint64_t timeout_ = -1;
        int descriptor = -1;
    };

    /**
    * Simple blocking TCP IPv6 server
    */
    struct TcpServerManager : public AbstractSocketManager {

        /**
        * Create server socket, bind it to `bind_host` and port `service` with listen queue `backlog`
        */
        TcpServerManager(const std::string &service, const std::string &bind_host = "::", int backlog = 100);


        static std::shared_ptr<TcpServerManager> create(const std::string &service, const std::string &bind_host = "::",
                                                        int backlog = 100);

    };


    /**
    * Simple blocking UNIX socket server
    */
    struct UnixServerManager : public AbstractSocketManager {

        /**
        * Create UNIX server socket, bind it to `path` with listen queue `backlog`
        */
        UnixServerManager(const std::string &path, int backlog = 100, uint32_t mode = 0777);

        inline const std::string &path() const { return path_; }

        static std::shared_ptr<UnixServerManager> create(const std::string &path, int backlog = 100,
                                                         uint32_t mode = 0777);

        virtual void stop();

    private:
        std::string path_;
    };


}
#endif  // IO

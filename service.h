//
// Created by RedDec on 10.04.15.
//

#ifndef _AUTH_SERVICE_H_
#define _AUTH_SERVICE_H_

#include <functional>
#include <scgi/scgi.h>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/value.h>
#include <unordered_map>
#include <chrono>

namespace scgi {
    namespace service {

        /**
         * Format std::chrono time_point to C-style of date.
         * TODO: Replace to put_time (std/C++11) when it will be implemented in most compilers
         */
        static inline std::string format_time(const std::chrono::system_clock::time_point &timepoint) {
            std::time_t timet = std::chrono::system_clock::to_time_t(timepoint);
            char buf[100];
            size_t count = std::strftime(buf, sizeof(buf), "%c", localtime(&timet));
            return std::string(buf, count);
        }

        /**
         * Base class of service processor
         */
        struct ServiceHandler {

            /**
             * Functor type of processor and preprocessor. Return false will be interpreted as internal error
             */
            typedef std::function<bool(scgi::RequestPtr, const Json::Value &)> MethodType;

            /**
             * Description of each published method
             */
            struct MethodDescription {
                /*
                 * Method name (param `method` in request payload)
                 */
                std::string name;
                /**
                 * Type of method response.
                 * TODO: Validate it on server side
                 */
                Json::ValueType returnType;
                /**
                 * Map of minimal required arguments
                 */
                std::unordered_map<std::string, Json::ValueType> required_params;
                /**
                 * Processor and pre-processor. If pre-processor return false, returns internal error
                 */
                MethodType processor, check_before;

                /**
                 * Validate incoming message for value type, method name, required params.
                 */
                bool validate(const Json::Value &value);

                /**
                 * Set response type.
                 * Returns self instance
                 */
                MethodDescription &set_return_type(Json::ValueType retype);

                /**
                 * Add required param in request
                 * Returns self instance
                 */
                MethodDescription &set_param(const std::string &name, Json::ValueType pType);

                /**
                 * Set method processor
                 * Returns self instance
                 */
                MethodDescription &set_processor(const MethodType &processor_);

                /**
                 * Set pre-processor class member
                 * Returns self instance
                 */
                template<class MemberFunction, class ClassType>
                MethodDescription &set_check_before(MemberFunction func, ClassType *clsObj) {
                    check_before = std::bind(func, clsObj, std::placeholders::_1, std::placeholders::_2);
                    return *this;
                }

                /**
                 * Set pre-processor
                 * Returns self instance
                 */
                MethodDescription &set_check_before(const MethodType &processor_);

                /**
                 * Set method processor from class member
                 * Returns self instance
                 */
                template<class MemberFunction, class ClassType>
                MethodDescription &set_processor(MemberFunction func, ClassType *clsObj) {
                    processor = std::bind(func, clsObj, std::placeholders::_1, std::placeholders::_2);
                    return *this;
                }

                /**
                 * Serialize method description to JSON.
                 * Return false if target object hasn't type `Json::objectType`
                 */
                bool serialize(Json::Value &dest) const;
            };

            /**
             * Share pointer for service handler
             */
            typedef std::shared_ptr<ServiceHandler> Ref;

            /**
             * Process incoming request and tries to call required method handler
             */
            bool process_request(scgi::RequestPtr request, const Json::Value &value);

            /**
             * Send JSON description of service
             */
            void send_service_description(scgi::RequestPtr request, const std::string &prefix) const;

            /**
             * Serialize methods to JSON object
             */
            void get_methods_description(Json::Value &result) const;

            /**
             * Just stub for inheritance
             */
            virtual ~ServiceHandler();

        protected:
            /**
             * Register new method of service with specified `name`
             */
            MethodDescription &register_method(const std::string &name);


        private:
            std::unordered_map<std::string, MethodDescription> methods;
        };


        /**
         * Send error message (500) with specified message.
         * Always returns false
         */
        bool send_error(scgi::RequestPtr request, const std::string &message);

        /**
         * Send JSON reponse
         */
        void send(scgi::RequestPtr request, const Json::Value &value);

        /**
         * Serialize obj to JSON by method `.serialize(Json::Value &v)`
         */
        template<class Obj>
        inline static void send(scgi::RequestPtr request, const Obj &obj) {
            Json::Value value;
            obj.serialize(value);
            send(request, value);
        }

        /**
         * Serialize obj to JSON by constructor
         */
        template<class Obj>
        inline static void send_json(scgi::RequestPtr request, const Obj &obj) {
            Json::Value value(obj);
            send(request, value);
        }

        /**
         * Manage services. Connection managing is based on scgi::SimpleAcceptor. Single threaded
         * TODO: Add different strategies (and way for adding new) for multithreading
         */
        struct ServiceManager {

            /**
             * Container for services on each prefix. Prefix '/' or '' is highly NOT RECOMMENDED because of
             * publishing common info
             */
            std::unordered_map<std::string, ServiceHandler::Ref> handlers;

            /**
             * Initialize service manager based on provided connection manager.
             * Highly recommended use non-blocking mode (ex: accept timeout sets to 1 second) otherwise
             * you can not use idle function (for example: catch SIGINT signal)
             */
            ServiceManager(scgi::ConnectionManager::Ptr connection_manager);

            /**
             * Create new service shared pointer and register it to provided prefix `path`.
             * Returns service shared pointer
             */
            template<class ClassType, class ...Args>
            std::shared_ptr<ClassType> add_handler(const std::string &path, const Args &...args) {
                if (path.empty() || path == "/")return false;
                auto ptr = std::make_shared<ClassType>(args...);
                handlers[path] = ptr;
                return ptr;
            }

            /**
             * Adds new service to specified `prefix`.
             * Returns false if prefix is '/' or ''
             */
            bool add_handler(const std::string &path, ServiceHandler::Ref service);

            /**
             * Run accept loop. If accept was not successull, runs idle function
             */
            void run();

            /**
             * Set function on idle
             */
            inline void set_on_idle(const std::function<void()> idle) {
                on_idle_ = idle;
            }

            /**
             * Get function on idle
             */
            inline const std::function<void()> &on_idle() const {
                return on_idle_;
            }

            /**
             * Set stopping flag. On next accept service manager will be stopped.
             */
            inline void stop() {
                stopped = true;
            }

            /**
             * Set verbose output on error
             */
            inline void set_debug(bool enable) {
                debug_ = enable;
            }

            /**
             * Is debug mode enabled
             */
            inline bool is_debug() const {
                return debug_;
            }

            /**
             * Is service manager stopped
             */
            inline bool is_stopped() const {
                return stopped;
            }

            /**
             * Stop service manager.
             * Used virtual for future inheritance
             */
            virtual  ~ServiceManager();

        protected:

            /**
             * Process request. Tries find payload (from body, payload param or query params) and call handler.
             * Otherwise send error.
             */
            virtual void process_request(ServiceHandler::Ref handler, scgi::RequestPtr request);

            /**
             * Send error message with details (if debug enabled)
             */
            void send_error(scgi::RequestPtr request, const std::string &message,
                            scgi::http::Status code = scgi::http::Status::InternalError,
                            const std::string &code_message = scgi::http::status_message::internal_error) const;

            /**
             * Send service description. If  full enabled, sends methods description too.
             */
            void send_service_description(scgi::RequestPtr request, bool full = false);

        private:

            /**
             * Find handler (and process request) or show service info
             */
            void find_handler(scgi::RequestPtr request);

            /**
             * Disable coping.
             */
            ServiceManager(const ServiceManager &) = delete;

            ServiceManager &operator=(const ServiceManager &) = delete;

            /**
             * Internal JSON parser
             * FIXME: Make this thread-local
             */
            Json::Reader reader;

            std::function<void()> on_idle_;
            bool stopped = false;
            bool debug_ = false;
            scgi::SimpleAcceptor acceptor;
        };
    }
}
#endif //_AUTH_SERVICE_H_

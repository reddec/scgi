//
// Created by Red Dec on 10.04.15.
//

#include <chrono>
#include "io/io.h"
#include "service.h"

namespace scgi {
    namespace service {
        ServiceManager::ServiceManager(io::ConnectionManager::Ptr connection_manager) : acceptor(connection_manager) {
        }

        void ServiceManager::find_handler(scgi::RequestPtr request) {
            std::string path = request->path();
            if (path.empty()) path = "/";
            auto handlerIter = handlers.find(path);
            if (handlerIter != handlers.end()) {
                if (request->query.find("info") != request->query.end()) {
                    (*handlerIter).second->send_service_description(request, path);
                } else
                    process_request((*handlerIter).second, request);
            } else if (request->query.find("info") != request->query.end()) {
                send_service_description(request, request->query["info"] == "full");
            } else {
                send_error(request, "Service on " + path + " notfound", scgi::http::Status::NotFound,
                           scgi::http::status_message::not_found);
            }
        }

        void ServiceManager::send_error(scgi::RequestPtr request, const std::string &message,
                                        scgi::http::Status code,
                                        const std::string &code_message) const {
            request->begin_response((int) code, code_message);
            if (debug_) {
                request->output() << message << std::endl;
                request->output() << "Path : " << request->path() << std::endl;
                request->output() << "Method : " << request->method() << std::endl;
                request->output() << "Content-Size : " << request->content_size() << std::endl;
                request->output() << "*****************************************" << std::endl;
                for (auto &kv:request->headers)
                    request->output() << kv.first << " : " << kv.second << std::endl;

            }
        }

        void ServiceManager::process_request(ServiceHandler::Ref handler, scgi::RequestPtr request) {
            Json::Value data;
            if (request->content_size() > 0) {
                std::vector<char> buffer(request->content_size() + 1);
                request->input().read(buffer.data(), buffer.size() - 1);
                if (!reader.parse(&buffer.front(), &buffer.back(), data)) {
                    send_error(request, "Failed to parse message");
                    return;
                }
            } else {
                auto dataIter = request->query.find("payload");
                if (dataIter != request->query.end()) {
                    if (!reader.parse((*dataIter).second, data)) {
                        send_error(request, "Failed to parse message");
                        return;
                    }
                } else {
                    //Last chance - represent query as JSON string object
                    for (auto &kv:request->query)
                        data[kv.first] = kv.second;
                }
            }
            try {
                if (!handler->process_request(request, data)) send_error(request, "Internal service error");
            } catch (std::exception &ex) {
                send_error(request, ex.what());
            }
            catch (...) {
                send_error(request, "Unknown error");
            }
        }

        ServiceManager::~ServiceManager() {
            stop();
        }

        void ServiceManager::run() {
            if (!acceptor.connection_manager()) return;
            try {
                scgi::RequestPtr request;
                for (request = acceptor.accept(); !stopped; request = acceptor.accept()) {
                    if (request && request->is_valid()) {
                        if (debug_) {
                            std::clog << "Request to " << request->path() << " method " << request->method() <<
                            std::endl;
                        }
                        find_handler(request);
                    } else {
                        if (on_idle_) on_idle_();
                    }
                    request = nullptr;
                }
            } catch (std::exception &ex) {
                std::cerr << "STD exception: " << ex.what() << std::endl;
            } catch (...) {
                std::cerr << "Unknown exception" << std::endl;
            }
        }

        ServiceHandler::MethodDescription &ServiceHandler::register_method(const std::string &name) {
            methods[name] = MethodDescription{name};
            return methods[name];
        }

        bool ServiceHandler::process_request(scgi::RequestPtr request, const Json::Value &value) {
            if (!value.isObject()) {
                send_error(request, "Request data is not object");
                std::clog << "Request data is not object" << std::endl;
                return false;
            }
            std::string method = value["method"].asString();
            auto methodIter = methods.find(method);
            if (methodIter == methods.end()) {
                send_error(request, "Method [" + method + "] not found");
                std::clog << "Method " << method << " not found" << std::endl;
                return false;
            }
            MethodDescription &mthd = (*methodIter).second;
            if (!mthd.validate(value)) {
                send_error(request, "Invalid arguments");
                std::clog << "Invalid arguments" << std::endl;
                return false;
            }
            if (mthd.check_before && !mthd.check_before(request, value)) {
                std::clog << "Check before failed" << std::endl;
                return false;
            }
            if (!mthd.processor) {
                std::clog << "Processor not found" << std::endl;
                return false;
            }
            return mthd.processor(request, value);
        }

        bool send_error(scgi::RequestPtr request, const std::string &message) {
            request->begin_response((int) scgi::http::Status::InternalError,
                                    scgi::http::status_message::internal_error);
            request->output() << "Error: " << message << std::endl;
            return false;
        }

        void send(scgi::RequestPtr request, const Json::Value &value) {
            request->set_response_type(scgi::http::content_type::application_json);
            request->begin_response();
            request->output() << value.toStyledString();
        }

        ServiceHandler::~ServiceHandler() { }

        bool ServiceHandler::MethodDescription::validate(const Json::Value &value) {
            if (!value.isObject() ||
                !value.isMember("method") ||
                value["method"].asString() != name)
                return false;
            for (auto &kv:required_params) {
                if (!value.isMember(kv.first) || value[kv.first].type() != kv.second) return false;
            }
            return true;
        }

        ServiceHandler::MethodDescription &ServiceHandler::MethodDescription::set_return_type(Json::ValueType retype) {
            returnType = retype;
            return *this;
        }

        ServiceHandler::MethodDescription &ServiceHandler::MethodDescription::set_param(const std::string &name,
                                                                                        Json::ValueType pType) {
            required_params[name] = pType;
            return *this;
        }

        ServiceHandler::MethodDescription &ServiceHandler::MethodDescription::set_processor(
                const MethodType &processor_) {
            processor = processor_;
            return *this;
        }


        ServiceHandler::MethodDescription &ServiceHandler::MethodDescription::set_check_before(
                ServiceHandler::MethodType const &processor_) {
            check_before = processor_;
            return *this;
        }


        bool ServiceHandler::MethodDescription::serialize(Json::Value &dest) const {
            static std::unordered_map<int, std::string> names = {
                    {Json::ValueType::objectValue, "object"},
                    {Json::stringValue,            "string"},
                    {Json::arrayValue,             "array"},
                    {Json::booleanValue,           "boolean"},
                    {Json::intValue,               "integer"},
                    {Json::nullValue,              "null"},
                    {Json::realValue,              "real"},
                    {Json::uintValue,              "uint"}};
            if (!dest.isObject())return false;
            Json::Value params_data;
            dest["name"] = name;
            dest["returnType"] = names[returnType];
            for (auto &param:required_params)
                params_data[param.first] = names[param.second];
            dest["params"] = params_data;
            dest["x-processor-exists"] = (bool) processor;
            dest["x-pre-processor-exists"] = (bool) check_before;
            return true;
        }

        void ServiceHandler::get_methods_description(Json::Value &mthds) const {
            for (auto &kv: methods) {
                Json::Value desc;
                if (kv.second.serialize(desc))
                    mthds.append(desc);
                else
                    std::cerr << "Failed serialize description for method " << kv.first << std::endl;
            }
        }

        void ServiceHandler::send_service_description(scgi::RequestPtr request, const std::string &prefix) const {
            Json::Value info;
            Json::Value mthds;
            info["path"] = prefix;
            info["time"] = format_time(std::chrono::system_clock::now());
            get_methods_description(mthds);
            info["methods"] = mthds;
            send(request, info);
        }

        void ServiceManager::send_service_description(scgi::RequestPtr request, bool full) {
            Json::Value info;
            Json::Value services_data;
            info["time"] = format_time(std::chrono::system_clock::now());
            if (!full)
                for (auto &kv:handlers) services_data.append(kv.first);
            else {
                for (auto &kv:handlers) {
                    Json::Value methods;
                    kv.second->get_methods_description(methods);
                    info[kv.first] = methods;
                }
            }
            info["services"] = services_data;
            send(request, info);
        }

        bool ServiceManager::add_handler(const std::string &path, ServiceHandler::Ref service) {
            if (path.empty() || path == "/")return false;
            handlers[path] = service;
            return true;
        }
    }
}
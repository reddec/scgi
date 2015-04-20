//
// Created by RedDec <net.dev@mail.ru> on 09.04.15.
//

#ifndef SCGI_HTTP_H
#define SCGI_HTTP_H

#include <iostream>
#include <cstring>
#include <vector>
#include <map>
#include <sstream>

namespace scgi {
    namespace http {
        /**
         * Base HTTP Headers
         */
        namespace header {
            static const std::string content_disposition = "Content-Disposition";
            static const std::string content_type = "Content-Type";
        }

        /**
         * Base content-types
         */
        namespace content_type {
            static const std::string text_plain = "text/plain";
            static const std::string text_html = "text/html";
            static const std::string application_json = "application/json";
            static const std::string application_xml = "application/xml";
        }

        /**
         * Base HTTP status messages
         */
        namespace status_message {
            static const std::string ok = "OK";
            static const std::string not_found = "Not Found";
            static const std::string internal_error = "Internal Server Error";
        }

        /**
         * Base HTTP status code
         */
        enum class Status : int {
            OK = 200,
            NotFound = 404,
            InternalError = 500
        };


        /**
         * Supported form data encoding
         */
        enum class EncodingType {
            x_www_form_urlencoded
        };

        /**
         * Decode string in URL-Percent (with + extension) format
         */
        template<class CharSqeuence>
        static inline std::string url_decode(const CharSqeuence &s, size_t size) {
            std::stringstream ss;
            char hex[3];
            hex[2] = '\0';
            for (size_t i = 0; i < size; ++i) {
                char c = s[i];
                if (c == '%') {
                    hex[0] = i + 1 < size && std::isalnum(s[i + 1]) ? s[i + 1] : '0';
                    hex[1] = i + 2 < size && std::isalnum(s[i + 1]) ? s[i + 2] : '0';
                    ss << (char) std::stoi(hex, nullptr, 16);
                    i += 2;
                } else if (c == '+') {
                    ss << ' ';
                } else
                    ss << c;
            }
            return ss.str();
        }

        static inline std::string url_decode(const std::string &s) {
            return url_decode(s.data(), s.size());
        }


        /**
         * Skip items from sequence `seq` while function `functor` returns true and `offset` less then `length`.
         * Returns first valid index or `length`
         */
        template<class CharSequence, class Functor, typename SizeType = size_t>
        static inline SizeType skip_some(const CharSequence &seq, const Functor &functor, const SizeType length,
                                         SizeType offset) {
            for (; offset < length; ++offset) if (!functor(seq[offset]))break;
            return offset;
        }

        /**
        * Parse HTTP-like headers from `in` stream to map `target` with max line size as `LINE_SIZE`.
        * Return bytes read.
        */
        template<class Map, size_t LINE_SIZE = 8192>
        static inline size_t parse_http_headers(std::istream &in, Map &target, size_t max_items = 65535) {
            char buffer[LINE_SIZE];
            size_t line_size, reads = 0, offset, items = 0;
            std::string key;
            std::string value;
            while (!in.eof() && items < max_items) {
                in.getline(buffer, LINE_SIZE);
                line_size = strnlen(buffer, LINE_SIZE);
                reads += line_size + (in.eof() ? 0 : 1);
                if (line_size <= 2) break;
                if (buffer[line_size - 1] == '\r') {
                    buffer[line_size - 1] = '\0';
                    --line_size;
                }

                //Get key
                for (offset = 0; offset < line_size; ++offset) {
                    if (buffer[offset] == ':') {
                        key = std::string(buffer, offset);
                        break;
                    }
                }
                //Get value
                offset = skip_some(buffer, std::iswspace, line_size, offset + 1);
                if (offset == line_size)value = "";
                else
                    value = std::string(&buffer[offset], line_size - offset);
                target[key] = value;
                ++items;
            }
            return reads;
        }

        /**
         * Parse HTTP `line` with line size `line_size`. Standalone items are pushed back into `list` and
         * named pair into `map`.
         *
         * Line example:
         *
         *   item1; name=value; yet another item; name2=value
         *
         * Produces:
         *
         *   list: item1, yet another item
         *   map: {name: value, name2: value}
         */
        template<class CharSequence, class List, class Map>
        static inline void parse_http_line(const CharSequence &line, size_t line_size, List &list, Map &map) {
            size_t i, prev = 0, sep = 0;
            size_t a, b;
            size_t last_index = line_size - 1;
            char c;
            for (i = 0; i < line_size; ++i) {
                c = line[i];
                if (c == ';' || i == last_index) {
                    if (sep > prev && sep < i) {
                        //K=V
                        a = sep + 1;
                        b = (c != ';' ? i + 1 : i);
                        if (line[sep + 1] == '"' && sep + 2 < line_size &&
                            i > 0 && line[i - (c == ';' ? 1 : 0)] == '"') {
                            ++a;
                            --b;
                        }
                        map[std::string(&line[prev], sep - prev)] = std::string(&line[a], b - a);
                    } else {
                        list.push_back(std::string(&line[prev], i - prev));
                    }
                    i = skip_some(line, std::iswspace, line_size, i + 1);
                    prev = i;
                } else if (c == '=') {
                    sep = i;
                }
            }
        }

        template<class IndexedArray, class List, class Map>
        static inline void parse_http_line(const IndexedArray &line, List &list, Map &map) {
            parse_http_line(line, line.size(), list, map);
        }

        template<class List, class Map>
        static inline void parse_http_line(const std::string &line, List &list, Map &map) {
            parse_http_line(line.data(), line.size(), list, map);
        }

        /**
         * Read from `in` stream int `buffer` till line with `bound` content found or read `maxSize` bytes.
         * Returns actual bytes read
         */
        template<class Container, char EndOfLine = '\n', char Trim = '\r'>
        static inline size_t read_to_line(std::istream &in, const std::string &bound, Container &buffer,
                                          size_t maxSize) {
            size_t line_size = 0, reads = 0;
            size_t bound_size = bound.size();
            size_t offset = buffer.size();
            size_t last_line_begin = offset;
            char c;
            if (!bound.empty()) {
                while (!in.eof() && reads < maxSize) {
                    in.get(c);
                    if (in.eof()) break;
                    ++reads;
                    if (c == EndOfLine) {

                        if (buffer.back() == Trim) --line_size;
                        if (line_size == bound_size &&
                            strncmp(&buffer[offset], bound.data(), line_size) == 0) {
                            buffer.resize(last_line_begin);
                            break;
                        }
                        last_line_begin = buffer.size();
                        if (buffer.back() == Trim) --last_line_begin;
                        line_size = 0;
                        offset = buffer.size() + 1; //+1 for EOL
                    } else
                        ++line_size;
                    buffer.push_back(c);
                }
            }
            return reads;
        }

        /**
         * Parse HTTP multipart/form-data request.
         * `in` - Input body
         * `map` - Data content with value
         * `boundary` - Multipart form boundary line
         * `max-size` - Maximum total size of all parts
         * `skip_preambule` - Skip first part
         */
        template<class Map>
        static inline void parse_http_multipart_form(std::istream &in, Map &map, const std::string &boundary,
                                                     size_t max_size = 65535, bool skip_preambule = true) {
            size_t part_size = max_size;
            std::vector<char> buffer;
            std::vector<std::string> options;
            std::string name;
            std::map<std::string, std::string> headers;

            if (skip_preambule) {
                read_to_line(in, boundary, buffer, boundary.size() + 20);
            }
            while (!in.eof() && part_size > 0) {
                buffer.clear();
                headers.clear();
                parse_http_headers(in, headers, 20);
                if (headers.empty() || headers.find(header::content_disposition) == headers.end()) break;
                parse_http_line(headers[header::content_disposition], options, headers);
                if (headers.empty() || headers.find("name") == headers.end())break;
                name = headers["name"];
                read_to_line(in, boundary, buffer, part_size);
                part_size -= buffer.size();
                map[name] = std::string(buffer.data(), buffer.size());
            }
        }

        /**
         * Parse till EOF application/x-www-form-urlencoded form data
         */
        template<class Map>
        static inline size_t parse_http_urlencoded_form(std::istream &in, Map &map) {
            std::string name, value;
            size_t items = 0;
            while (!in.eof()) {
                std::getline(in, name, '=');
                std::getline(in, value, '&');
                map[url_decode(name)] = url_decode(value);
                ++items;
            }
            return items;
        }
    }
}
#endif //SCGI_HTTP_H

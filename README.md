# scgi
SCGI client library for C++11 and linux. Exists packaging for Debian base systems

# Requirements

* C++11 compiler (tested on gcc-4.8)
* UNIX/Linux tcp network subsystem
* Cmake 2.8+

For services support

* libjsoncpp-dev
  * [Source repo](https://github.com/open-source-parsers/jsoncpp) 
  * [Ubuntu package](http://packages.ubuntu.com/search?keywords=jsoncpp&searchon=names&suite=trusty&section=all)

# How to build

# Easy way

Just copy to console and run (tested on Ubuntu 14.04 LTS x86_64; include services)

```
cd /tmp && \
rm -rf scgi && \
git clone --recursive https://github.com/reddec/scgi && \
cd scgi && \
./build_debian.sh && \
sudo dpkg -i Release/scgi-*.deb && \
cd ../
```

# Manual build

## Clone repository

```
git clone https://github.com/reddec/scgi.git
cd scgi
```

## Prepare Cmake files

```
mkdir Release
cd Release
```

```
cmake -DCMAKE_BUILD_TYPE=Release ..
```

or with services (required JsonCpp library with headers)


```
cmake -DWITH_SERVICES -DCMAKE_BUILD_TYPE=Release ..
```

## Build

```
make
```

## For Debian based systems

Create .deb package after building by

```
make package
```

and install via `dpkg -i` or something else

************************

# Direct SCGI request handling

Documentation will be in future...

See `SimpleAcceptor` class

************************

# Services


## Frontend example configuration

### NGinx

With service bound to UNIX socket `/tmp/myservice.sock`

```
http {
   server {
	location ~ ^/myservice(?<path_info>/.*) { 
         root /data/www;
         include /etc/nginx/scgi_params;
         scgi_param PATH_INFO $path_info; 
         scgi_pass unix:/tmp/myservice.sock;
	}
    }
}
```

> **Important**
>
> NGinx does not provide PATH_INFO variable in SCGI context by default.
> 
> So `scgi_param PATH_INFO $path_info` with regular expression in location is work around.

### Lihgttpd

With service bound to UNIX socket `/tmp/myservice.sock`

Enable module

```
server.modules = (
	...
    "mod_scgi",
    ...
)
```

Add backend

```
scgi.server = (
  "/myservice" =>
   ( 
     "server1" => (
         "socket" => "/tmp/myservice.sock",
         "check-local" => "disable"
     )
   )
)
```

## Simple backend service

### Target

CRUD in memory some text data.


### Code


```c++

#include <scgi/service.h>
#include <string>
#include <unordered_map>


class DataKeeper : public scgi::service::ServiceHandler {
public:
    DataKeeper() : scgi::service::ServiceHandler() {
        register_method("update")
                .set_param("key", Json::stringValue)
                .set_param("value", Json::stringValue)
                .set_return_type(Json::nullValue)
                .set_processor(&DataKeeper::update, this);
        register_method("get")
                .set_param("key", Json::stringValue)
                .set_return_type(Json::stringValue)
                .set_processor(&DataKeeper::get, this);
        register_method("get_keys")
                .set_return_type(Json::arrayValue)
                .set_processor(&DataKeeper::get_keys, this);
    }

protected:

    bool update(scgi::RequestPtr request, const Json::Value &query) {
        std::string key = query["key"].asString();
        std::string value = query["value"].asString();
        content_[key] = value;
        // Send zero data but with success code (200 OK)
        request->begin_response();
        return true;
    }

    bool get(scgi::RequestPtr request, const Json::Value &query) {
        std::string key = query["key"].asString();
        auto key_iter = content_.find(key);
        if (key_iter == content_.end()) return scgi::service::send_error(request, "Key not found");
        scgi::service::send(request, Json::Value((*key_iter).second));
        return true;
    }

    bool get_keys(scgi::RequestPtr request, const Json::Value &query) {
        Json::Value response;
        for (auto &kv:content_) response.append(kv.first);
        scgi::service::send(request, response);
        return true;
    }

    std::unordered_map<std::string, std::string> content_;
};


bool stopped = false;

void global_finalize(int) {
    stopped = true;
}

int main() {
    // Show used SCGI library version info
    std::cout << "SCGI library: " << scgi::version() << std::endl;
    // Create connection manager. By default exists TCP or UNIX socket connection manager
    auto connection_manager = io::UnixServerManager::create("/tmp/auth");
    // Create new service manager with specified connection manager
    scgi::service::ServiceManager serviceManager(connection_manager);
    // Set non-blocking mode
    connection_manager->set_accept_timeout(1000);
    // Add handlers
    serviceManager.add_handler<DataKeeper>("/data");
    // Close service manager when SIGINT catched
    serviceManager.set_on_idle([&serviceManager]() {
        if (stopped)serviceManager.stop();
    });
    // Show debug info. By default disabled
    serviceManager.set_debug(true);
    // Start loop
    serviceManager.run();
    return 0;
}

```

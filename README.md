# scgi
SCGI client library for C++11 and linux. Exists packaging for Debian base systems

# Requirements

* C++11 compiler (tested on gcc-4.8)
* UNIX/Linux tcp network subsystem
* Cmake 2.8+

# How to build

# Easy way

Just copy to console and run (tested on Ubuntu 14.04 LTS x86_64)

```
cd /tmp && rm -rf scgi && git clone https://github.com/reddec/scgi && cd scgi && ./build_debian.sh && sudo dpkg -i Release/scgi-*.deb && cd ../
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
cmake -DCMAKE_BUILD_TYPE=Release ..
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



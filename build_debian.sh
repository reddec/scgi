#!/bin/bash
mkdir Release
cd Release
cmake -DWITH_SERVICES=1 -DCMAKE_BUILD_TYPE=Release ..
make
make package
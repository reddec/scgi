cmake_minimum_required(VERSION 2.8.4)
project(scgi)
set(VERSION_MAJOR "1")
set(VERSION_MINOR "1")
set(VERSION_PATCH "9")
set(VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
set(DESCRIPTION "SCGI client library for C++11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -O3 -march=native")
aux_source_directory(./ SRC_LIST)

# Share library
add_library(${PROJECT_NAME}-SharedLib SHARED ${SRC_LIST})
set_target_properties(${PROJECT_NAME}-SharedLib PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
target_compile_definitions(${PROJECT_NAME}-SharedLib PUBLIC SCGI_VERSION="${VERSION}")

# Static library
add_library(${PROJECT_NAME}-StaticLib STATIC ${SRC_LIST})
set_target_properties(${PROJECT_NAME}-StaticLib PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
target_compile_definitions(${PROJECT_NAME}-StaticLib PUBLIC SCGI_VERSION="${VERSION}")

install(FILES scgi.h io.h http.h DESTINATION /usr/include/scgi/)
install(TARGETS ${PROJECT_NAME}-SharedLib ${PROJECT_NAME}-StaticLib DESTINATION /usr/lib/)

# Setup DEBIAN control files
set(CPACK_COMPONENTS_ALL_IN_ONE_PACKAGE 1)
set(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${VERSION_PATCH})
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${DESCRIPTION})
set(CPACK_PACKAGE_DESCRIPTION "https://github.com/reddec/scgi")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Red Dec <net.dev@mail.ru>")

include(CPack)
cmake_minimum_required(VERSION 2.8.4)
project(scgi)
set(VERSION_MAJOR "1")
set(VERSION_MINOR "3")
set(VERSION_PATCH "1")
set(VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
set(DESCRIPTION "SCGI client library for C++11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -O3 -march=native")

set(HEADERS_LIST src/http.h src/scgi.h)
set(SRC_LIST src/http.cpp src/scgi.cpp src/io/io.cpp)
set(LIBS )
set(RUNTIME_DEPS )

if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE MATCHES "[Dd][Ee][Bb][Uu][Gg]")
    message("debug mode")
    set(VERSION "${VERSION}-debug")
endif()

if(WITH_SERVICES)
    list(APPEND SRC_LIST src/service.cpp src/patterns.cpp)
    list(APPEND HEADERS_LIST src/service.h src/patterns.h)
    list(APPEND LIBS jsoncpp)
    list(APPEND RUNTIME_DEPS libjsoncpp-dev) # dev - because of required headers
endif()

# Share library
add_library(${PROJECT_NAME}-SharedLib SHARED ${SRC_LIST})
set_target_properties(${PROJECT_NAME}-SharedLib PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
target_compile_definitions(${PROJECT_NAME}-SharedLib PUBLIC BUILD_VERSION="${VERSION}")
target_link_libraries(${PROJECT_NAME}-SharedLib ${LIBS})

# Static library
add_library(${PROJECT_NAME}-StaticLib STATIC ${SRC_LIST})
set_target_properties(${PROJECT_NAME}-StaticLib PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
target_compile_definitions(${PROJECT_NAME}-StaticLib PUBLIC BUILD_VERSION="${VERSION}")
target_link_libraries(${PROJECT_NAME}-StaticLib ${LIBS})

install(FILES ${HEADERS_LIST} DESTINATION /usr/include/scgi/)
install(FILES src/io/io.h DESTINATION /usr/include/scgi/io/) # Copy from out project. Point for discussion
install(TARGETS ${PROJECT_NAME}-SharedLib ${PROJECT_NAME}-StaticLib DESTINATION /usr/lib/)

# Setup DEBIAN control files
set(CPACK_COMPONENTS_ALL_IN_ONE_PACKAGE 1)
set(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${VERSION_PATCH})
set(CPACK_DEBIAN_PACKAGE_DEPENDS ${RUNTIME_DEPS})
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${DESCRIPTION})
set(CPACK_PACKAGE_DESCRIPTION "https://github.com/reddec/scgi")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Red Dec <net.dev@mail.ru>")

include(CPack)
cmake_minimum_required(VERSION 3.17)
project(vertx-cpp)

set(CMAKE_CXX_STANDARD 20)
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)
include_directories(${PROJECT_INCLUDE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(/home/nerull/dev/repo/cpp/hazelcast-cpp-client/hazelcast-cpp-client/cpp/Linux_64/hazelcast/include )

set(LIBRARIES_SEARCH_PATHS ${EXTRA_LIBRARY})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")

add_library(hazelcast STATIC IMPORTED)
set_property(TARGET hazelcast PROPERTY IMPORTED_LOCATION /home/nerull/dev/repo/cpp/hazelcast-cpp-client/hazelcast-cpp-client/cpp/Linux_64/hazelcast/lib/libHazelcastClient3.12_64.a)

add_library(evpp STATIC IMPORTED)
set_property(TARGET evpp PROPERTY IMPORTED_LOCATION /usr/local/lib/libevpp_static.a)

add_library(evpp_boost STATIC IMPORTED)
set_property(TARGET evpp_boost PROPERTY IMPORTED_LOCATION /usr/local/lib/libevpp_boost_static.a)

add_executable(vertx_cpp_exe main.cpp
        src/vertx/hazelcast_cluster.cpp src/vertx/event_bus.hpp src/vertx/net_server.hpp src/vertx/uuid.hpp)

target_link_libraries(vertx_cpp_exe ${LIBRARIES_SEARCH_PATHS}  -lpthread -lrt -lssl -lcrypto -lc CONAN_PKG::glog CONAN_PKG::libevent evpp_boost evpp
        hazelcast CONAN_PKG::asio CONAN_PKG::rapidjson CONAN_PKG::concurrentqueue CONAN_PKG::fmt -ltcmalloc)
#-ltcmalloc


add_executable(vertx_cpp_bench src/benchmark.cpp
        src/vertx/hazelcast_cluster.cpp src/vertx/event_bus.hpp src/vertx/net_server.hpp src/vertx/uuid.hpp)

target_link_libraries(vertx_cpp_bench ${LIBRARIES_SEARCH_PATHS}  -lpthread -lrt -lssl -lcrypto -lc CONAN_PKG::glog CONAN_PKG::libevent evpp_boost evpp
        hazelcast CONAN_PKG::asio CONAN_PKG::rapidjson CONAN_PKG::concurrentqueue CONAN_PKG::fmt -ltcmalloc benchmark)
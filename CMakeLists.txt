cmake_minimum_required(VERSION 3.17)
project(vertx-cpp)

set(CMAKE_CXX_STANDARD 20)
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)
#set(_HZ_ENABLE OFF)
set(DCHECK_ALWAYS_ON ON)

IF(DCHECK_ALWAYS_ON)
    add_compile_definitions(DCHECK_ALWAYS_ON)
ENDIF(DCHECK_ALWAYS_ON)

include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/include)
IF(_HZ_ENABLE)
include_directories(${PROJECT_SOURCE_DIR}/../hazelcast-cpp-client/hazelcast-cpp-client/cpp/Linux_64/hazelcast/include )
add_compile_definitions(_HZ_ENABLE)
ENDIF(_HZ_ENABLE)


IF(_HZ_ENABLE)
add_library(hazelcast STATIC IMPORTED)
set_property(TARGET hazelcast PROPERTY IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/../hazelcast-cpp-client/hazelcast-cpp-client/cpp/Linux_64/hazelcast/lib/libHazelcastClient3.12_64.a)
set(EXTRA_LIBRARY ${EXTRA_LIBRARY} hazelcast)
ENDIF(_HZ_ENABLE)

set(LIBRARIES_SEARCH_PATHS ${EXTRA_LIBRARY})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")

add_library(evpp STATIC IMPORTED)
set_property(TARGET evpp PROPERTY IMPORTED_LOCATION /usr/local/lib/libevpp_static.a)

add_library(evpp_boost STATIC IMPORTED)
set_property(TARGET evpp_boost PROPERTY IMPORTED_LOCATION /usr/local/lib/libevpp_boost_static.a)

IF(_HZ_ENABLE)
    add_executable(vertx_cpp_exe main.cpp  src/vertx/hazelcast_cluster.cpp src/vertx/event_bus_hazeclast.hpp src/vertx/net_server.hpp src/vertx/uuid.hpp src/vertx/http_server.hpp)
    add_library(vertx_cpp SHARED main.cpp  src/vertx/hazelcast_cluster.cpp src/vertx/event_bus_hazeclast.hpp src/vertx/net_server.hpp src/vertx/uuid.hpp src/vertx/http_server.hpp )
else()
    add_executable(vertx_cpp_exe main.cpp src/vertx/event_bus.hpp src/vertx/net_server.hpp src/vertx/uuid.hpp src/vertx/http_server.hpp)
    add_library(vertx_cpp SHARED main.cpp src/vertx/event_bus.hpp src/vertx/net_server.hpp src/vertx/uuid.hpp src/vertx/http_server.hpp)
ENDIF(_HZ_ENABLE)

target_link_libraries(vertx_cpp_exe ${LIBRARIES_SEARCH_PATHS}  -lpthread -lrt -lssl -lcrypto -lc CONAN_PKG::glog CONAN_PKG::libevent evpp_boost evpp
        CONAN_PKG::asio CONAN_PKG::rapidjson CONAN_PKG::concurrentqueue CONAN_PKG::fmt -ltcmalloc)
#-ltcmalloc


add_executable(vertx_cpp_bench src/benchmark.cpp)

target_link_libraries(vertx_cpp_bench ${LIBRARIES_SEARCH_PATHS}  -lpthread -lrt -lssl -lcrypto -lc CONAN_PKG::glog CONAN_PKG::libevent evpp_boost evpp
        CONAN_PKG::asio CONAN_PKG::rapidjson CONAN_PKG::concurrentqueue CONAN_PKG::fmt -ltcmalloc benchmark)

target_link_libraries(vertx_cpp ${LIBRARIES_SEARCH_PATHS}  -lpthread -lrt -lssl -lcrypto -lc CONAN_PKG::glog CONAN_PKG::libevent evpp_boost evpp
        CONAN_PKG::asio CONAN_PKG::rapidjson CONAN_PKG::concurrentqueue CONAN_PKG::fmt)
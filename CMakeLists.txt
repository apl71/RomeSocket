cmake_minimum_required(VERSION 3.10)

project(RomeSocket VERSION 0.1)

set(CMAKE_CXX_STANDARD 17)

SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall -DNDEBUG")

set(SOURCES
	source/RomeSocket.cpp)

## 静态库
add_library(rocket STATIC ${SOURCES})
## 动态库
add_library(rocket_shared SHARED ${SOURCES})

## 指定动态库名称
set_target_properties(rocket_shared PROPERTIES OUTPUT_NAME "rocket")
set_target_properties(rocket PROPERTIES CLEAN_DIRECT_OUTPUT 1)
set_target_properties(rocket_shared PROPERTIES CLEAN_DIRECT_OUTPUT 1)

target_include_directories(rocket PUBLIC include)
target_include_directories(rocket_shared PUBLIC include)

if (UNIX)
	install(TARGETS rocket rocket_shared DESTINATION /usr/lib)
elseif (WIN32)
	target_link_libraries(rocket -lws2_32)
	install(TARGETS rocket rocket_shared DESTINATION C:\\Windows\\System32)
endif()

## examples
add_executable(ssl_server example/ssl_server.cpp)
target_include_directories(ssl_server PUBLIC include)
target_link_directories(ssl_server PUBLIC build /usr/lib)
target_link_libraries(ssl_server rocket :libssl.so.3 :libcrypto.so.3)

add_executable(ssl_client example/ssl_client.cpp)
target_include_directories(ssl_client PUBLIC include)
target_link_directories(ssl_client PUBLIC build /usr/lib)
target_link_libraries(ssl_client rocket :libssl.so.3 :libcrypto.so.3)

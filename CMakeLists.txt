cmake_minimum_required(VERSION 3.10)

project(RomeSocket LANGUAGES C CXX VERSION 0.1)

set(CMAKE_C_COMPILER "gcc")
set(CMAKE_CXX_COMPILER "g++")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

#SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_BUILD_TYPE "Release")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall -DNDEBUG")
SET(CMAKE_C_FLAGS_DEBUG "$ENV{CFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_C_FLAGS_RELEASE "$ENV{CFLAGS} -O3 -Wall -DNDEBUG")

include_directories(include)

set (SOURCES
	source/thread_pool.cpp
	source/romesocket.cpp
    source/layer.c
    source/client.c)

## 静态库
add_library(rocket STATIC ${SOURCES})
## 动态库
add_library(rocket_shared SHARED ${SOURCES})
## 客户端库
add_library(rocketc SHARED source/client.c source/layer.c)

## 指定动态库名称
set_target_properties(rocket_shared PROPERTIES OUTPUT_NAME "rocket")
set_target_properties(rocket PROPERTIES CLEAN_DIRECT_OUTPUT 1)
set_target_properties(rocket_shared PROPERTIES CLEAN_DIRECT_OUTPUT 1)

target_link_libraries(rocket PUBLIC pthread)
target_link_libraries(rocket_shared PUBLIC pthread)

## 指定公共头文件
set_target_properties(rocket_shared PROPERTIES PUBLIC_HEADER "include/romesocket.hpp;include/thread_pool.hpp;include/layer.h")
set_target_properties(rocket PROPERTIES PUBLIC_HEADER "include/romesocket.hpp;include/thread_pool.hpp;include/layer.h")
set_target_properties(rocketc PROPERTIES PUBLIC_HEADER "include/client.h;include/layer.h")

install(TARGETS rocket rocket_shared rocketc
				LIBRARY DESTINATION /usr/local/lib
				PUBLIC_HEADER DESTINATION /usr/local/include/romesocket)
# if (UNIX)
# 	install(TARGETS rocket rocket_shared DESTINATION /usr/lib)
# elseif (WIN32)
# 	target_link_libraries(rocket -lws2_32)
# 	install(TARGETS rocket rocket_shared DESTINATION C:\\Windows\\System32)
# endif()

## 测试程序编译
set (THREAD_POOL_TEST_1_SOURCES
	example/thread_pool_test_1.cpp
	source/thread_pool.cpp)

add_executable(thread_pool_test_1 ${THREAD_POOL_TEST_1_SOURCES})

target_link_libraries(thread_pool_test_1 PUBLIC pthread)

## echo服务器测试
set (ECHO_TEST_SOURCES
	example/echo_server.cpp
	source/romesocket.cpp
	source/thread_pool.cpp
    source/layer.c
    source/client.c)

add_executable(echo_server ${ECHO_TEST_SOURCES})
target_link_libraries(echo_server PUBLIC uring pthread sodium)

set (ECHO_CLIENT_TEST_SOURCES
	example/echo_client.cpp
	source/thread_pool.cpp
    source/client.c
    source/layer.c)

add_executable(echo_client ${ECHO_CLIENT_TEST_SOURCES})

target_link_libraries(echo_client PUBLIC uring pthread sodium)

set (ECHO_TOY_TEST_SOURCES
	example/echo_toy.cpp
    source/client.c
    source/layer.c)

add_executable(echo_toy ${ECHO_TOY_TEST_SOURCES})

target_link_libraries(echo_toy PUBLIC sodium)

## key exchange测试
add_executable(key_exchange example/key_exchange.c)
target_link_libraries(key_exchange PUBLIC sodium)

## 客户端
set (CLIENT_SOURCES
    source/client.c)

## 测试
set(LAYER_TEST_SOURCES
    source/layer.c
    test/layer_test.cpp)
add_executable(layer_test ${LAYER_TEST_SOURCES})
target_link_libraries(layer_test PUBLIC sodium gtest gtest_main pthread)

## random服务器
set(RANDOM_SERVER_SOURCES
    example/random_server.cpp
	source/romesocket.cpp
	source/layer.c
	source/thread_pool.cpp)
add_executable(random_server ${RANDOM_SERVER_SOURCES})
target_link_libraries(random_server PUBLIC sodium pthread uring)

## random客户端
set(RANDOM_CLIENT_SOURCES
    example/random_client.cpp
	source/client.c
	source/layer.c)
add_executable(random_client ${RANDOM_CLIENT_SOURCES})
target_link_libraries(random_client PUBLIC sodium pthread)

## receive服务器
set(RECEIVE_SERVER_SOURCES
    example/receive_server.cpp
	source/romesocket.cpp
	source/layer.c
	source/thread_pool.cpp)
add_executable(receive_server ${RECEIVE_SERVER_SOURCES})
target_link_libraries(receive_server PUBLIC sodium pthread uring)

## receive客户端
set(RECEIVE_CLIENT_SOURCES
    example/receive_client.cpp
	source/client.c
	source/layer.c)
add_executable(receive_client ${RECEIVE_CLIENT_SOURCES})
target_link_libraries(receive_client PUBLIC sodium pthread)
cmake_minimum_required(VERSION 3.10)

project(RomeSocket VERSION 0.1)

set(CMAKE_CXX_STANDARD 11)

add_library(roket SHARED source/RomeSocket.cpp)

target_include_directories(roket PUBLIC include)

install(TARGETS roket DESTINATION /usr/lib)
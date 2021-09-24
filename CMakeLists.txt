cmake_minimum_required(VERSION 3.10)

set(CMAKE_C_COMPILER "gcc")
set(CMAKE_CXX_COMPILER "g++")

project(RomeSocket VERSION 0.1)

set(CMAKE_CXX_STANDARD 11)

add_library(rocket SHARED source/RomeSocket.cpp)

target_include_directories(rocket PUBLIC include)

if (UNIX)
	install(TARGETS rocket DESTINATION /usr/lib)
elseif (WIN32)
	target_link_libraries(rocket -lws2_32)
	install(TARGETS rocket DESTINATION C:\\Windows\\System32)
endif()
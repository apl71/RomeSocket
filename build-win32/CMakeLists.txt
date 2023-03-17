cmake_minimum_required(VERSION 3.10)

project(RomeSocket-win32 LANGUAGES C VERSION 0.1)

set(CMAKE_C_COMPILER "i686-w64-mingw32-gcc")

set(CMAKE_C_STANDARD 11)

SET(CMAKE_BUILD_TYPE "Release")
SET(CMAKE_C_FLAGS_RELEASE "$ENV{CFLAGS} -O3 -Wall -DNDEBUG")

include_directories(../include libsodium-win32/include)
link_directories(libsodium-win32/bin)

## 客户端库
add_library(rocketc SHARED ../source/client.c ../source/layer.c)
target_link_libraries(rocketc sodium-23 ws2_32)
set_target_properties(rocketc PROPERTIES PUBLIC_HEADER "include/client.h;include/layer.h")
set_target_properties(rocketc PROPERTIES LIBRARY_OUTPUT_NAME "rocketc")
set_target_properties(rocketc PROPERTIES PREFIX "" SUFFIX ".dll")
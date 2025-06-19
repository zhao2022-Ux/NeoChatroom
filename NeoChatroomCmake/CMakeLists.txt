cmake_minimum_required(VERSION 3.14)
project(NeoChatroom)

set(CMAKE_CXX_STANDARD 17)

# 打印编译信息
message(STATUS "System: ${CMAKE_SYSTEM}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

# 包含目录
include_directories(
        include
        src/lib
        src/lib/json
        tool/config
        http\ server/server
        src/server/http/server/src
        src/server/http/server/src/server
        src/server/http/server/tool
)

# 源文件
file(GLOB_RECURSE SOURCES
        src/server/http/server/src/*.cpp
        src/server/http/server/src/server/*.cpp
        src/server/http/server/src/server/guide/*.cpp
        src/server/http/server/src/config/*.cpp
        src/server/http/server/src/chat/*.cpp
        src/server/http/server/src/data/*.cpp
        src/server/http/server/tool/*.cpp
        src/lib/json/jsoncpp.cpp
)

# 可执行文件
add_executable(NeoChatroom ${SOURCES}
        src/server/http/server/src/chat/privatechat.cpp
        src/server/http/server/src/chat/privatechat.h
        src/server/http/server/src/chat/paste/cloudpaste.cpp
        src/server/http/server/src/chat/paste/cloudpaste.h)

# 找依赖
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

# 链接库（使用 keyword signature）
target_link_libraries(NeoChatroom
        PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
        Threads::Threads
        SQLite::SQLite3
)

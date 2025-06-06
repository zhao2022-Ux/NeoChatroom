cmake_minimum_required(VERSION 3.14)
project(NeoChatroom)

set(CMAKE_CXX_STANDARD 17)

# 打印编译信息
message(STATUS "System: ${CMAKE_SYSTEM}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

# 包含目录
include_directories(
        include
        json
        tool/config
        http\ server/server
)

# 源文件
file(GLOB_RECURSE SOURCES
        main.cpp
        jsoncpp.cpp
        data/*.cpp
        http\ server/*.cpp
        http\ server/server/*.cpp
        tool/*.cpp
        tool\ log/*.cpp
)

# 可执行文件
add_executable(NeoChatroom ${SOURCES}
        "http server/server/serverconfig.h"
        "http server/server/serverconfig.cpp"
        "http server/ControlServer.cpp"
        "http server/ControlServer.h"
        "http server/server/chatDBmanager.cpp"
        "http server/server/chatDBmanager.h")

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

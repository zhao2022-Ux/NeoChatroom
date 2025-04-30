cmake_minimum_required(VERSION 3.16)
project(NeoChatroom LANGUAGES CXX)

# 启用C++17标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 强制使用现代CMake策略
cmake_policy(SET CMP0091 NEW)  # 正确处理MSVC运行时库
cmake_policy(SET CMP0069 NEW)  # 正确处理INTERPROCEDURAL_OPTIMIZATION

# Windows/MSVC特定配置
if(MSVC)
    # 修复PDB写入冲突（关键修复）
    add_compile_options(/FS /Zi /EHsc /W4 /WX)
    
    # 设置调试信息格式
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "ProgramDatabase")
    
    # 禁用默认的MSVC警告级别覆盖
    string(REGEX REPLACE "/W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    
    # 设置合理的运行时库
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
    # 输出目录设置
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    
    # PDB输出目录
    set(CMAKE_COMPILE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/pdb)
    file(MAKE_DIRECTORY ${CMAKE_COMPILE_PDB_OUTPUT_DIRECTORY})
endif()

# 平台架构检查
if(NOT CMAKE_VS_PLATFORM_NAME)
    set(CMAKE_VS_PLATFORM_NAME "x64")
endif()
message(STATUS "${CMAKE_VS_PLATFORM_NAME} architecture in use")

if(NOT ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64" OR 
        "${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86"))
    message(FATAL_ERROR "${CMAKE_VS_PLATFORM_NAME} arch is not supported!")
endif()

# 配置类型
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

# 全局编译选项
if(MSVC)
    # Debug配置
    set(CMAKE_CXX_FLAGS_DEBUG "/MDd /Zi /Ob0 /Od /RTC1")
    # Release配置
    set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /Ob2 /DNDEBUG")
endif()

# 链接选项
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "/DEBUG /INCREMENTAL")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/INCREMENTAL:NO")

# 解决方案文件夹支持
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# 包含工具函数
include(CMake/Utils.cmake)

# 包含全局设置（如果存在）
include(CMake/GlobalSettingsInclude.cmake OPTIONAL)

# 添加子项目
add_subdirectory(NeoChatroom)

# 安装规则（可选）
install(TARGETS NeoChatroom
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib/static
)
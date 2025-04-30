set(PROJECT_NAME NeoChatroom)

################################################################################
# Source groups
################################################################################
set(
    "include/config.h"
    "include/configLoader.h"
    "include/httplib.h"
    "include/log.h"
    "include/resource.h"
    "include/Server.h"
    "include/tool.h"
)
source_group("头文件" FILES ${})

set(
    "jsoncpp.cpp"
    "keyword.txt"
    "main.cpp"
    "src/configLoader.cpp"
)
source_group("源文件" FILES ${})

set(__data
    "data/datamanage.cpp"
    "include/datamanage.h"
)
source_group("源文件\\data" FILES ${__data})

set(__html
    "html/chatlist.html"
    "html/chatlist.js"
    "html/chatroom.js"
    "html/files/updates.html"
    "html/index.html"
    "html/login.html"
)
source_group("源文件\\html" FILES ${__html})

set(__http_server
    "http server/ctrl.cpp"
    "http server/server.cpp"
)
source_group("源文件\\http server" FILES ${__http_server})

set(__http_server__server
    "http server/server/chatroom.cpp"
    "http server/server/chatroom.h"
    "http server/server/guide.cpp"
    "http server/server/roommanager.cpp"
    "http server/server/roommanager.h"
)
source_group("源文件\\http server\\server" FILES ${__http_server__server})

set(__json
    "json/json-forwards.h"
    "json/json.h"
)
source_group("源文件\\json" FILES ${__json})

set(__tool
    "tool/config/config.cpp"
    "tool/tool.cpp"
)
source_group("源文件\\tool" FILES ${__tool})

set(__tool__log
    "log/log.cpp"
)
source_group("源文件\\tool\\log" FILES ${__tool__log})

set(ALL_FILES
    ${}
    ${}
    ${__data}
    ${__html}
    ${__http_server}
    ${__http_server__server}
    ${__json}
    ${__tool}
    ${__tool__log}
)

################################################################################
# Target
################################################################################
add_executable(${PROJECT_NAME} ${ALL_FILES})

use_props(${PROJECT_NAME} "${CMAKE_CONFIGURATION_TYPES}" "${DEFAULT_CXX_PROPS}")
set(ROOT_NAMESPACE NeoChatroom)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_GLOBAL_KEYWORD "Win32Proj"
)
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION_RELEASE "TRUE"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION_RELEASE "TRUE"
    )
endif()
################################################################################
# Compile definitions
################################################################################
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:Debug>:"
            "_DEBUG"
        ">"
        "$<$<CONFIG:Release>:"
            "NDEBUG"
        ">"
        "_CONSOLE;"
        "UNICODE;"
        "_UNICODE"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:Debug>:"
            "_DEBUG"
        ">"
        "$<$<CONFIG:Release>:"
            "NDEBUG"
        ">"
        "WIN32;"
        "_CONSOLE;"
        "UNICODE;"
        "_UNICODE"
    )
endif()

################################################################################
# Compile and link options
################################################################################
if(MSVC)
    if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
        target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Release>:
                /Oi;
                /Gy
            >
            /permissive-;
            /sdl;
            /W3;
            ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT};
            ${DEFAULT_CXX_EXCEPTION_HANDLING}
        )
    elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
        target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Release>:
                /Oi;
                /Gy
            >
            /permissive-;
            /sdl;
            /W3;
            ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT};
            ${DEFAULT_CXX_EXCEPTION_HANDLING}
        )
    endif()
    if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
        target_link_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Release>:
                /OPT:REF;
                /OPT:ICF
            >
            /DEBUG;
            /SUBSYSTEM:CONSOLE
        )
    elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
        target_link_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Release>:
                /OPT:REF;
                /OPT:ICF
            >
            /DEBUG;
            /SUBSYSTEM:CONSOLE
        )
    endif()
endif()

################################################################################
# Dependencies
################################################################################
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    set(ADDITIONAL_LIBRARY_DEPENDENCIES
        "$<$<CONFIG:Debug>:"
            "libssl;"
            "libcrypto"
        ">"
    )
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE "${ADDITIONAL_LIBRARY_DEPENDENCIES}")


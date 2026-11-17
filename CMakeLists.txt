cmake_minimum_required(VERSION 3.16)
project(SteamIconFix LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 资源脚本是 UTF-8，必须显式指定代码页，否则中文菜单会乱码
if (MINGW)
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} --codepage=65001")
elseif (MSVC)
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /c65001")
endif()

# 版本号唯一来源：version.ini
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/version.ini" _ver_ini)
string(REGEX MATCH "[Vv]ersion=([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ver_match "${_ver_ini}")
if (_ver_match)
    set(APP_VERSION "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
else()
    message(WARNING "version.ini 中没有 Version=x.y.z，回退到 1.0.0")
    set(APP_VERSION "1.0.0")
endif()
message(STATUS "SteamIconFix version: ${APP_VERSION}")

set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GEN_DIR}")

file(WRITE "${GEN_DIR}/version.h"
"#pragma once\n"
"\n"
"#define APP_VERSION_STR  \"${APP_VERSION}\"\n"
"#define APP_VERSION_WSTR L\"${APP_VERSION}\"\n"
)

set(SRC
    src/main.cpp
    src/ui/App.cpp
    src/ui/Actions.cpp
    src/core/Util.cpp
    src/core/Net.cpp
    src/core/Env.cpp
    src/core/Fix.cpp
    res/app.rc
)

add_executable(SteamIconFix WIN32 ${SRC})

target_compile_definitions(SteamIconFix PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _WIN32_WINNT=0x0601
    WINVER=0x0601
)

target_include_directories(SteamIconFix PRIVATE src res "${GEN_DIR}")

if (MSVC)
    target_compile_options(SteamIconFix PRIVATE /utf-8 /W3)
    target_link_options(SteamIconFix PRIVATE /MANIFEST:NO)
endif()

if (MINGW)
    target_compile_options(SteamIconFix PRIVATE -municode -Wall -Wextra -Wno-unused-parameter)
    target_link_options(SteamIconFix PRIVATE -municode -mwindows -static -static-libgcc -static-libstdc++)
endif()

target_link_libraries(SteamIconFix PRIVATE
    comctl32
    comdlg32
    winhttp
    shlwapi
    shell32
    ole32
    uuid
    gdi32
    user32
)

# 产物统一放到 bin
set_target_properties(SteamIconFix PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)

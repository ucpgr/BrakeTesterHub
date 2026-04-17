cmake_minimum_required(VERSION 3.18)
project(BrakeTesterHub LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

find_package(Threads REQUIRED)
find_path(CUPS_INCLUDE_DIR cups/cups.h REQUIRED)
find_library(CUPS_LIBRARY cups REQUIRED)

FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(
    cpp_httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.35.0
)
FetchContent_MakeAvailable(cpp_httplib)

FetchContent_Declare(
    sqlite3
    GIT_REPOSITORY https://github.com/sqlite/sqlite.git
    GIT_TAG version-3.45.1
)
FetchContent_MakeAvailable(sqlite3)
set(SQLITE3_LIB sqlite3)

FetchContent_Declare(
    sqlite_orm
    GIT_REPOSITORY https://github.com/fnc12/sqlite_orm.git
    GIT_TAG v1.9.1
)
FetchContent_MakeAvailable(sqlite_orm)

cmake_policy(SET CMP0077 NEW)
set(LIBSERIAL_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(LIBSERIAL_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LIBSERIAL_PYTHON_ENABLE OFF CACHE BOOL "" FORCE)
set(LIBSERIAL_BUILD_DOCS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    libserial
    GIT_REPOSITORY https://github.com/crayzeewulf/libserial.git
    GIT_TAG master
)
FetchContent_MakeAvailable(libserial)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    src/*.cpp
)

add_executable(BrakeTesterHub ${SRC_FILES})

target_include_directories(BrakeTesterHub PRIVATE
    src
    include
    ${cpp_httplib_SOURCE_DIR}
    ${sqlite_orm_SOURCE_DIR}/include
    ${libserial_SOURCE_DIR}/src
    ${sqlite3_SOURCE_DIR}
    ${CUPS_INCLUDE_DIR}
)

target_link_libraries(BrakeTesterHub PRIVATE
    ${SQLITE3_LIB}
    libserial_static
    nlohmann_json::nlohmann_json
    Threads::Threads
    ${CUPS_LIBRARY}
)

target_compile_definitions(BrakeTesterHub PRIVATE
    BT_STATIC_ROOT="www"
)

cmake_minimum_required(VERSION 3.18)
project(BrakeTesterHub LANGUAGES CXX)

# -------------------------------------------------
# Global config
# -------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# -------------------------------------------------
# Threads (needed early)
# -------------------------------------------------
find_package(Threads REQUIRED)

# -------------------------------------------------
# cpp-httplib (header-only)
# -------------------------------------------------
FetchContent_Declare(
    cpp_httplib
    GIT_REPOSITORY git@github.com:yhirose/cpp-httplib.git
    GIT_TAG        v0.35.0
)

FetchContent_MakeAvailable(cpp_httplib)

# -------------------------------------------------
# SQLite (amalgamation)
# -------------------------------------------------
FetchContent_Declare(
    sqlite3
    GIT_REPOSITORY git@github.com:sqlite/sqlite.git
    GIT_TAG        version-3.45.1
)

FetchContent_MakeAvailable(sqlite3)

# SQLite builds target: sqlite3
set(SQLITE3_LIB sqlite3)

# -------------------------------------------------
# sqlite_orm (header-only)
# -------------------------------------------------
FetchContent_Declare(
    sqlite_orm
    GIT_REPOSITORY git@github.com:fnc12/sqlite_orm.git
    GIT_TAG        v1.9.1
)

FetchContent_MakeAvailable(sqlite_orm)

# -------------------------------------------------
# libserial
# -------------------------------------------------
cmake_policy(SET CMP0077 NEW)

set(LIBSERIAL_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(LIBSERIAL_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LIBSERIAL_PYTHON_ENABLE OFF CACHE BOOL "" FORCE)
set(LIBSERIAL_BUILD_DOCS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    libserial
    GIT_REPOSITORY git@github.com:crayzeewulf/libserial.git
    GIT_TAG        master
)

FetchContent_MakeAvailable(libserial)


# -------------------------------------------------
# Sources
# -------------------------------------------------
file(GLOB SRC_FILES
    src/*.cpp
)

add_executable(BrakeTesterHub ${SRC_FILES})

# -------------------------------------------------
# Includes
# -------------------------------------------------
target_include_directories(BrakeTesterHub PRIVATE
    src
    include
    ${cpp_httplib_SOURCE_DIR}
    ${sqlite_orm_SOURCE_DIR}/include
    ${libserial_SOURCE_DIR}/
    ${sqlite3_SOURCE_DIR}
)

# -------------------------------------------------
# Linking
# -------------------------------------------------
target_link_libraries(BrakeTesterHub PRIVATE
    ${SQLITE3_LIB}
    serial
    Threads::Threads
)

# -------------------------------------------------
# Runtime config
# -------------------------------------------------
target_compile_definitions(BrakeTesterHub PRIVATE
    BT_STATIC_ROOT=\"client/dist\"
)

cmake_minimum_required(VERSION 3.18)
project(BrakeTesterHub LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

option(BT_ENABLE_CUPS "Enable CUPS printing support" ON)

find_package(Threads REQUIRED)
if(BT_ENABLE_CUPS)
    find_path(CUPS_INCLUDE_DIR cups/cups.h REQUIRED)
    find_library(CUPS_LIBRARY cups REQUIRED)
endif()

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

set(SRC_FILES
    src/main.cpp
    src/app/App.cpp
    src/components/CupsPrinterClient.cpp
    src/components/BrakeTesterSerialDispatcher.cpp
    src/components/BrakeTesterCodec.cpp
    src/components/DatabaseSchema.cpp
    src/components/LptListener.cpp
    src/components/LptManager.cpp
    src/components/PrintManager.cpp
    src/components/PrnPatcher.cpp
    src/components/PrnRenderer.cpp
    src/components/PrnValidator.cpp
    src/components/PrnWriter.cpp
    src/components/RenderedDocumentWriter.cpp
    src/repositories/LptRepository.cpp
    src/repositories/PrintSettingsRepository.cpp
    src/repositories/SettingsRepository.cpp
    src/repositories/VehicleRepository.cpp
    src/stores/CurrentTestAxleDataStore.cpp
    src/stores/LptStore.cpp
    src/stores/PrintStatusStore.cpp
    src/stores/PrnPayloadStore.cpp
    src/stores/SelectedVehicleStore.cpp
    src/stores/SerialDeviceStore.cpp
    src/web/BrakeTesterHttpServer.cpp
    src/web/BrakeTesterHttpServer.history.cpp
    src/web/BrakeTesterHttpServer.lpt.cpp
    src/web/BrakeTesterHttpServer.settings.cpp
    src/web/BrakeTesterHttpServer.vehicle.cpp
)

add_executable(BrakeTesterHub ${SRC_FILES})

target_include_directories(BrakeTesterHub PRIVATE
    src
    include
    ${cpp_httplib_SOURCE_DIR}
    ${sqlite_orm_SOURCE_DIR}/include
    ${libserial_SOURCE_DIR}/src
    ${sqlite3_SOURCE_DIR}
)

if(BT_ENABLE_CUPS)
    target_include_directories(BrakeTesterHub PRIVATE ${CUPS_INCLUDE_DIR})
endif()

target_link_libraries(BrakeTesterHub PRIVATE
    ${SQLITE3_LIB}
    libserial_static
    nlohmann_json::nlohmann_json
    Threads::Threads
)

if(BT_ENABLE_CUPS)
    target_link_libraries(BrakeTesterHub PRIVATE ${CUPS_LIBRARY})
endif()

target_compile_definitions(BrakeTesterHub PRIVATE
    BT_STATIC_ROOT="www"
    BT_ENABLE_CUPS=$<IF:$<BOOL:${BT_ENABLE_CUPS}>,1,0>
)

add_custom_target(compile_check_core
    COMMAND ${CMAKE_COMMAND} -E env CXX=${CMAKE_CXX_COMPILER} ${CMAKE_SOURCE_DIR}/scripts/compile_check_core.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running core compile-only checks (no external system deps required)"
)

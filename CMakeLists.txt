cmake_minimum_required(VERSION 3.20)
project(BrakeTesterHub LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(brake_tester_hub
    src/main.cpp
)

target_include_directories(brake_tester_hub PRIVATE include)

target_link_libraries(brake_tester_hub PRIVATE sqlite3 serial)

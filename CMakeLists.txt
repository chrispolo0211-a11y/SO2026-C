cmake_minimum_required(VERSION 3.16)
project(laboratorio_concurrencia LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(laboratorio_concurrencia
    src/main.cpp
    src/memory.cpp
    src/ui.cpp
    src/worker.cpp
)

target_include_directories(laboratorio_concurrencia PRIVATE include)

find_package(Threads REQUIRED)
find_package(Curses REQUIRED)
target_include_directories(laboratorio_concurrencia PRIVATE ${CURSES_INCLUDE_DIRS})
target_link_libraries(laboratorio_concurrencia PRIVATE Threads::Threads ${CURSES_LIBRARIES})
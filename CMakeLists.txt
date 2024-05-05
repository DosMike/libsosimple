cmake_minimum_required(VERSION 3.24)
project(sosimple VERSION 1.0 DESCRIPTION "Easy to use socket library" LANGUAGES CXX)

# defaults

#set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

# library

include_directories(include)

set(lib_src_files
    src/socket_impl.cpp
    src/endpoint.cpp
    src/utilities.cpp
    src/worker_impl.cpp
    )
set(lib_public_headers
    sosimple.hpp
    sosimple/endpoint.hpp
    sosimple/exportzs.hpp
    sosimple/netheaders.hpp
    sosimple/pending.hpp
    sosimple/socket.hpp
    sosimple/utilities.hpp
    sosimple/worker.hpp
    )
add_library(${PROJECT_NAME} SHARED ${lib_src_files})
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(${PROJECT_NAME} PROPERTIES SOVERSION ${PROJECT_VERSION_MAJOR})
set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER "${lib_public_headers}")
target_compile_definitions(${PROJECT_NAME} PUBLIC SOSIMPLE_SO)
target_compile_definitions(${PROJECT_NAME} PRIVATE SOSIMPLE_BUILD)

# example to test

add_executable(${PROJECT_NAME}_example src/main.cpp)
target_link_libraries(${PROJECT_NAME}_example ${PROJECT_NAME})

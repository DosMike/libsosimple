cmake_minimum_required(VERSION 3.24)
project(sosimple VERSION 1.1.0 DESCRIPTION "Easy to use socket library" LANGUAGES CXX)

# defaults

#set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# library

set(lib_src_files
    src/socket_impl.cpp
    src/socket_poller.cpp
    src/endpoint.cpp
    src/utilities.cpp
    src/worker_impl.cpp
    )
set(lib_public_headers
    include/sosimple.hpp
    include/sosimple/endpoint.hpp
    include/sosimple/exports.hpp
    include/sosimple/platforms.hpp
    include/sosimple/pending.hpp
    include/sosimple/socket.hpp
    include/sosimple/utilities.hpp
    include/sosimple/worker.hpp
    )
if(WIN32)
    list(APPEND lib_src_files src/windows_compat.cpp)
    list(APPEND lib_public_headers include/sosimple/windows_compat.hpp)
endif()

if(BUILD_SHARED_LIBS)
    add_library(${PROJECT_NAME} SHARED ${lib_src_files})
    set(THIS_OUTPUT_NAME ${PROJECT_NAME}-shared)
    # tell our feature macro that sosimple is now a shared lib
    target_compile_definitions(${PROJECT_NAME} PUBLIC SOSIMPLE_SO)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SOSIMPLE_BUILD)
else()
    add_library(${PROJECT_NAME} STATIC ${lib_src_files})
    set(THIS_OUTPUT_NAME ${PROJECT_NAME}-static)
endif()

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PUBLIC Ws2_32)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${THIS_OUTPUT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(${PROJECT_NAME} PROPERTIES SOVERSION ${PROJECT_VERSION_MAJOR})
target_sources(${PROJECT_NAME} PUBLIC FILE_SET HEADERS TYPE HEADERS BASE_DIRS include FILES ${lib_public_headers})

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}              COMPONENT Runtime
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}              COMPONENT Runtime
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}              COMPONENT Development
        FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT Development
        )

if(UNIX)
    configure_file(${PROJECT_NAME}.pc.in ${PROJECT_NAME}.pc @ONLY)
    install(FILES ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.pc DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)
endif()

# example to test

# add_executable(${PROJECT_NAME}_example src/main.cpp)
# target_link_libraries(${PROJECT_NAME}_example ${PROJECT_NAME})

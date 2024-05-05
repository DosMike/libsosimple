cmake_minimum_required(VERSION 3.24)
project(sosimple VERSION 1.0 DESCRIPTION "Easy to use socket library" LANGUAGES CXX)

# defaults

#set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

# library

set(lib_src_files
    src/socket_impl.cpp
    src/endpoint.cpp
    src/utilities.cpp
    src/worker_impl.cpp
    )
set(lib_public_headers
    include/sosimple.hpp
    include/sosimple/endpoint.hpp
    include/sosimple/exports.hpp
    include/sosimple/netheaders.hpp
    include/sosimple/pending.hpp
    include/sosimple/socket.hpp
    include/sosimple/utilities.hpp
    include/sosimple/worker.hpp
    )

if(BUILD_SHARED_LIBS)
    add_library(${PROJECT_NAME} SHARED ${lib_src_files})
    # tell our feature macro that sosimple is now a shared lib
    target_compile_definitions(${PROJECT_NAME} PUBLIC SOSIMPLE_SO)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SOSIMPLE_BUILD)
else()
    add_library(${PROJECT_NAME} STATIC ${lib_src_files})
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(${PROJECT_NAME} PROPERTIES SOVERSION ${PROJECT_VERSION_MAJOR})
target_sources(${PROJECT_NAME} PUBLIC FILE_SET HEADERS TYPE HEADERS BASE_DIRS include FILES ${lib_public_headers})

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}                 COMPONENT Runtime
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}                 COMPONENT Runtime
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/${PROJECT_NAME} COMPONENT Development
        FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}    COMPONENT Development
        )

configure_file(${PROJECT_NAME}.pc.in ${PROJECT_NAME}.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.pc DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)

# example to test

# add_executable(${PROJECT_NAME}_example src/main.cpp)
# target_link_libraries(${PROJECT_NAME}_example ${PROJECT_NAME})

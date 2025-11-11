cmake_minimum_required(VERSION 3.5.1...3.29.0)

project(zlib-ng-add-subdirecory-test C)

include(CTest)

set(BUILD_SHARED_LIBS OFF)
set(BUILD_TESTING ON CACHE BOOL "Build test binaries" FORCE)

add_subdirectory(../.. zlib-ng)

add_executable(app main.c)
target_link_libraries(app zlib-ng-static)

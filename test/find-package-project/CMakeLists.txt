cmake_minimum_required(VERSION 3.14...4.2.1)

project(zlib-ng-find-package-test C)

option(CONSUMER_ZLIB_COMPAT "Consume the package under its zlib-compat name" OFF)

# Config mode is mandatory. In compat mode the builtin FindZLIB module would
# otherwise resolve the system zlib instead of the package under test.
if(CONSUMER_ZLIB_COMPAT)
    find_package(ZLIB REQUIRED CONFIG)
    set(ZLIB_TARGET ZLIB::ZLIB)
else()
    find_package(zlib-ng REQUIRED CONFIG)
    set(ZLIB_TARGET zlib-ng::zlib)
endif()

add_executable(app main.c)
if(CONSUMER_ZLIB_COMPAT)
    target_compile_definitions(app PRIVATE CONSUMER_ZLIB_COMPAT)
endif()
target_link_libraries(app ${ZLIB_TARGET})

macro(configure_util_executable target)
    target_include_directories(${target} PRIVATE ${PROJECT_BINARY_DIR} ${PROJECT_SOURCE_DIR})
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
    if(NOT WITH_GZFILEOP)
        target_compile_definitions(${target} PRIVATE -DWITH_GZFILEOP)
        target_sources(${target} PRIVATE ${ZLIB_GZFILE_PRIVATE_HDRS} ${ZLIB_GZFILE_SRCS})
    endif()
endmacro()

add_executable(minigzip minigzip.c)
configure_util_executable(minigzip)
if(NOT DEFINED BUILD_SHARED_LIBS)
    target_link_libraries(minigzip zlib-ng-static)
else()
    target_link_libraries(minigzip zlib-ng)
endif()
if(BASEARCH_S360_FOUND)
    if(WITH_DFLTCC_DEFLATE OR WITH_DFLTCC_INFLATE)
        set_source_files_properties(minigzip.c PROPERTIES COMPILE_DEFINITIONS BUFLEN=262144)
    endif()
endif()
set(MINIGZIP_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:minigzip> PARENT_SCOPE)

add_executable(minideflate minideflate.c)
configure_util_executable(minideflate)
target_link_libraries(minideflate zlib-ng)
set(MINIDEFLATE_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:minideflate> PARENT_SCOPE)

if(INSTALL_UTILS)
    install(TARGETS minigzip minideflate
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
endif()

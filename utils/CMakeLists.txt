macro(configure_util_executable target)
    target_include_directories(${target} PRIVATE ${PROJECT_BINARY_DIR} ${PROJECT_SOURCE_DIR})
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
endmacro()

# minigzip
add_executable(minigzip minigzip.c)
configure_util_executable(minigzip)
if(NOT WITH_GZFILEOP)
    target_compile_definitions(minigzip PRIVATE -DWITH_GZFILEOP)
    target_sources(minigzip PRIVATE ${ZLIB_GZFILE_PRIVATE_HDRS} ${ZLIB_GZFILE_SRCS})
endif()
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

# minideflate
add_executable(minideflate minideflate.c)
configure_util_executable(minideflate)
if(NOT DEFINED BUILD_SHARED_LIBS)
    target_link_libraries(minideflate zlib-ng-static)
else()
    target_link_libraries(minideflate zlib-ng)
endif()
set(MINIDEFLATE_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:minideflate> PARENT_SCOPE)

# makefixed
add_executable(makefixed makefixed.c ${PROJECT_SOURCE_DIR}/inftrees.c)
configure_util_executable(makefixed)
set(MAKEFIXED_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:makefixed> PARENT_SCOPE)

# maketrees
add_executable(maketrees maketrees.c ${PROJECT_SOURCE_DIR}/trees.c ${PROJECT_SOURCE_DIR}/zutil.c)
configure_util_executable(maketrees)
set(MAKETREES_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:maketrees> PARENT_SCOPE)

# makecrct
add_executable(makecrct makecrct.c)
configure_util_executable(makecrct)
set(MAKECRCT_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:makecrct> PARENT_SCOPE)

# Install minigzip and minideflate
if(INSTALL_UTILS)
    install(TARGETS minigzip minideflate RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()

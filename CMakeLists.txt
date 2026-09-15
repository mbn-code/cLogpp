cmake_minimum_required(VERSION 3.14)
project(cLogpp LANGUAGES CXX VERSION 0.3.0
        DESCRIPTION "Zero-dependency, header-only structured logging for modern C++"
        HOMEPAGE_URL "https://github.com/mbn-code/cLogpp")

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ---------------------------------------------------------------------------
# cLog++ is header-only. Consumers just need the include/ directory on their
# path; this target carries that plus the C++17 requirement and threads.
# ---------------------------------------------------------------------------
add_library(clogpp INTERFACE)
add_library(clogpp::clogpp ALIAS clogpp)

target_include_directories(clogpp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(clogpp INTERFACE cxx_std_17)

find_package(Threads REQUIRED)
target_link_libraries(clogpp INTERFACE Threads::Threads)

# GCC < 9 ships std::filesystem in a separate library (used by DailyFileSink).
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(clogpp INTERFACE stdc++fs)
endif()

# ---------------------------------------------------------------------------
# Install: headers plus a CMake package so `find_package(clogpp)` works.
# ---------------------------------------------------------------------------
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS clogpp EXPORT clogppTargets)
install(EXPORT clogppTargets
        NAMESPACE clogpp::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/clogpp)

configure_package_config_file(
    cmake/clogppConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/clogppConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/clogpp)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/clogppConfigVersion.cmake
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/clogppConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/clogppConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/clogpp)

# ---------------------------------------------------------------------------
# Tests, examples and benchmarks: only when cLog++ is the top-level project
# (not when it is pulled in via add_subdirectory / FetchContent).
# ---------------------------------------------------------------------------
set(CLOGPP_IS_TOP_LEVEL OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CLOGPP_IS_TOP_LEVEL ON)
endif()

option(CLOGPP_BUILD_TESTS      "Build cLog++ tests"      ${CLOGPP_IS_TOP_LEVEL})
option(CLOGPP_BUILD_EXAMPLES   "Build cLog++ examples"   ${CLOGPP_IS_TOP_LEVEL})
option(CLOGPP_BUILD_BENCHMARKS "Build cLog++ benchmarks" OFF)
option(CLOGPP_WARNINGS_AS_ERRORS "Treat compiler warnings as errors (tests/examples)" OFF)
set(CLOGPP_SANITIZE "" CACHE STRING
    "Sanitizers for tests/examples, e.g. 'address,undefined' or 'thread' (GCC/Clang only)")

# Strict warnings for the code we build here; the INTERFACE target itself
# stays flag-free so consumers keep their own settings.
add_library(clogpp_dev_flags INTERFACE)
if(MSVC)
    target_compile_options(clogpp_dev_flags INTERFACE /W4 /permissive- /utf-8)
    target_compile_definitions(clogpp_dev_flags INTERFACE _CRT_SECURE_NO_WARNINGS)
    if(CLOGPP_WARNINGS_AS_ERRORS)
        target_compile_options(clogpp_dev_flags INTERFACE /WX)
    endif()
else()
    target_compile_options(clogpp_dev_flags INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
        -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual)
    if(CLOGPP_WARNINGS_AS_ERRORS)
        target_compile_options(clogpp_dev_flags INTERFACE -Werror)
    endif()
    if(CLOGPP_SANITIZE)
        target_compile_options(clogpp_dev_flags INTERFACE
            -fsanitize=${CLOGPP_SANITIZE} -fno-omit-frame-pointer -g)
        target_link_options(clogpp_dev_flags INTERFACE -fsanitize=${CLOGPP_SANITIZE})
    endif()
endif()

if(CLOGPP_BUILD_TESTS)
    enable_testing()
    set(CLOGPP_TESTS
        basic
        sync_mode
        file_sink
        async_flush
        levels
        async_lossless
        json_fields
        rotating_file_sink
        per_sink
        text_formatter
        bound_fields
        scoped_context
        source_location
        thread_id
        overflow_policy
        multithreaded
        sinks
        daily_file_sink
        ring_buffer
        sink_errors
        timestamp
        compat_includes
        windows_headers)
    foreach(t IN LISTS CLOGPP_TESTS)
        add_executable(test_${t} tests/${t}.cpp)
        target_link_libraries(test_${t} PRIVATE clogpp clogpp_dev_flags)
        # Each test writes its scratch files into its own directory so they
        # can run in parallel (ctest -j).
        set(_wd ${CMAKE_CURRENT_BINARY_DIR}/test_work/${t})
        file(MAKE_DIRECTORY ${_wd})
        add_test(NAME ${t} COMMAND test_${t} WORKING_DIRECTORY ${_wd})
        set_tests_properties(${t} PROPERTIES TIMEOUT 120)
    endforeach()
endif()

if(CLOGPP_BUILD_EXAMPLES)
    foreach(ex hello quickstart logger_debug_example context multi_sink text_console)
        add_executable(example_${ex} examples/${ex}.cpp)
        target_link_libraries(example_${ex} PRIVATE clogpp clogpp_dev_flags)
    endforeach()
endif()

if(CLOGPP_BUILD_BENCHMARKS)
    add_executable(benchmark_logger benchmarks/benchmark_logger.cpp)
    target_link_libraries(benchmark_logger PRIVATE clogpp)
endif()

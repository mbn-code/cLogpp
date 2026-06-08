cmake_minimum_required(VERSION 3.14)
project(cLogpp LANGUAGES CXX VERSION 0.2.0)

# ---------------------------------------------------------------------------
# cLog++ is header-only. Consumers just need the include/ directory on their
# path; this target carries that plus the C++17 requirement and threads.
# ---------------------------------------------------------------------------
add_library(clogpp INTERFACE)
add_library(clogpp::clogpp ALIAS clogpp)

target_include_directories(clogpp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_features(clogpp INTERFACE cxx_std_17)

find_package(Threads REQUIRED)
target_link_libraries(clogpp INTERFACE Threads::Threads)

# Only build tests/examples when cLog++ is the top-level project (not when it
# is pulled in via add_subdirectory / FetchContent).
set(CLOGPP_IS_TOP_LEVEL OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CLOGPP_IS_TOP_LEVEL ON)
endif()

option(CLOGPP_BUILD_TESTS      "Build cLog++ tests"      ${CLOGPP_IS_TOP_LEVEL})
option(CLOGPP_BUILD_EXAMPLES   "Build cLog++ examples"   ${CLOGPP_IS_TOP_LEVEL})
option(CLOGPP_BUILD_BENCHMARKS "Build cLog++ benchmarks" OFF)

if(CLOGPP_BUILD_TESTS)
    enable_testing()
    set(CLOGPP_TESTS basic sync_mode file_sink async_flush levels async_lossless json_fields rotating_file_sink)
    foreach(t IN LISTS CLOGPP_TESTS)
        add_executable(test_${t} tests/${t}.cpp)
        target_link_libraries(test_${t} PRIVATE clogpp)
        add_test(NAME ${t} COMMAND test_${t})
    endforeach()
endif()

if(CLOGPP_BUILD_EXAMPLES)
    foreach(ex hello quickstart logger_debug_example)
        add_executable(example_${ex} examples/${ex}.cpp)
        target_link_libraries(example_${ex} PRIVATE clogpp)
    endforeach()
endif()

if(CLOGPP_BUILD_BENCHMARKS)
    add_executable(benchmark_logger benchmarks/benchmark_logger.cpp)
    target_link_libraries(benchmark_logger PRIVATE clogpp)
endif()

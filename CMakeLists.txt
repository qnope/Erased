cmake_minimum_required(VERSION 3.30)
include(FetchContent)

project(erased LANGUAGES CXX)

add_subdirectory(erased)

option(ERASED_BUILD_TESTS "Build Erased tests" ON)
option(ERASED_BUILD_BENCHMARKS "Build Erased benchmarks" ON)

if(ERASED_BUILD_TESTS)
    add_subdirectory(tests)
endif()
if(ERASED_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

FetchContent_Declare(googlebench
    GIT_REPOSITORY https://github.com/google/benchmark
    GIT_TAG v1.9.1
)

set(BENCHMARK_ENABLE_TESTING OFF)

FetchContent_MakeAvailable(googlebench)

add_executable(Benchmarks benchmarks.cpp)
target_link_libraries(Benchmarks PRIVATE erased::erased erased::warnings benchmark::benchmark)


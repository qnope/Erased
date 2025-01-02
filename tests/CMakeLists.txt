FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest
    GIT_TAG v1.15.2
)

FetchContent_MakeAvailable(googletest)

add_executable(Tests tests.cpp)
target_link_libraries(Tests PRIVATE erased::erased erased::sanitizer erased::warnings gtest_main gtest)

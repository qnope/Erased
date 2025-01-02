add_library(erased INTERFACE)

target_compile_features(erased INTERFACE cxx_std_23)
target_include_directories(erased INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_sources(erased
    PUBLIC
        FILE_SET erased_headers
        TYPE HEADERS
        BASE_DIRS ./include/
        FILES include/erased/erased.h
)

set(clang_cxx "$<COMPILE_LANG_AND_ID:CXX,ARMClang,AppleClang,Clang>")
set(gcc_cxx "$<COMPILE_LANG_AND_ID:CXX,GNU>")
set(msvc_cxx "$<COMPILE_LANG_AND_ID:CXX,MSVC>")

add_library(erased_sanitizer INTERFACE)

target_compile_options(erased_sanitizer INTERFACE
    $<${clang_cxx}:-fsanitize=address;-fsanitize=undefined>
    $<${msvc_cxx}:/fsanitize=address;/Zc:preprocessor>
)

target_link_options(erased_sanitizer INTERFACE
    $<${clang_cxx}:-fsanitize=address>
)

target_compile_options(erased_sanitizer INTERFACE
    $<${msvc_cxx}:-D_DISABLE_VECTOR_ANNOTATION;-D_DISABLE_STRING_ANNOTATION>
)

add_library(erased_warnings INTERFACE)

target_compile_options(erased_warnings INTERFACE
    $<${clang_cxx}:-Wall -Wextra -Werror>
    $<${gcc_cxx}:-Wall -Wextra -Werror>
)

add_library(erased::erased ALIAS erased)
add_library(erased::sanitizer ALIAS erased_sanitizer)
add_library(erased::warnings ALIAS erased_warnings)


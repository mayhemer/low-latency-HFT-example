cmake_minimum_required(VERSION 3.20)
project(HFT1 VERSION 0.1.0 LANGUAGES C CXX)

add_executable(HFT1 
    src/book.cpp
    src/ingest.cpp
    src/main.cpp
)

set_property(TARGET HFT1 PROPERTY CXX_STANDARD 23)
set_property(TARGET HFT1 PROPERTY C_EXTENSIONS NO)

# Warnings (portable set; GCC/Clang)
# if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
#     target_compile_options(HFT1 PRIVATE -Wall -Wextra -Wpedantic)
# endif()

# Define DEBUG for Debug builds (and RelWithDebInfo, if you want)
target_compile_definitions(HFT1 PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:RelWithDebInfo>:DEBUG>
)

# Optional: choose optimization levels explicitly (CMake already sets sane defaults)
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(HFT1 PRIVATE
        $<$<CONFIG:Release>:-O2>
        $<$<CONFIG:MinSizeRel>:-Os>
        $<$<CONFIG:RelWithDebInfo>:-O2>
        $<$<CONFIG:Debug>:-O0>
    )
endif()

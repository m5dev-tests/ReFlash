cmake_minimum_required(VERSION 3.15)
project(ReFlash VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(REFLASH_SOURCES
    src/blend_modes.cpp
    src/renderer.cpp
    src/main.cpp
)

if(EMSCRIPTEN)
    add_executable(reflash ${REFLASH_SOURCES})

    set_target_properties(reflash PROPERTIES
        OUTPUT_NAME "reflash"
        SUFFIX ".js"
    )

    target_link_options(reflash PRIVATE
        "-sWASM=1"
        "-sUSE_WEBGL2=1"
        "-sFULL_ES3=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
        "-sEXPORTED_FUNCTIONS=['_init_engine','_load_frame','_set_blend_mode','_set_color_transform','_main']"
    )
else()
    # Native target build
    add_executable(reflash ${REFLASH_SOURCES})

    # Unit test target
    add_executable(test_blend_modes
        src/blend_modes.cpp
        tests/test_blend_modes.cpp
    )

    enable_testing()
    add_test(NAME test_blend_modes COMMAND test_blend_modes)
endif()

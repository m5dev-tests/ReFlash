cmake_minimum_required(VERSION 3.15)
project(ReFlash VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(REFLASH_SOURCES
    src/blend_modes.cpp
    src/renderer.cpp
    src/swf_stream.cpp
    src/swf_parser.cpp
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
        "-sUSE_ZLIB=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
        "-sEXPORTED_FUNCTIONS=['_init_engine','_load_frame','_set_blend_mode','_set_color_transform','_reflash_load_swf','_reflash_get_width','_reflash_get_height','_reflash_get_framerate','_reflash_get_frame_count','_reflash_get_version','_reflash_get_as_version','_reflash_get_bg_color','_malloc','_free','_main']"
    )
else()
    find_package(ZLIB REQUIRED)

    # Native target build
    add_executable(reflash ${REFLASH_SOURCES})
    target_link_libraries(reflash PRIVATE ZLIB::ZLIB)

    # Unit test targets
    add_executable(test_blend_modes
        src/blend_modes.cpp
        tests/test_blend_modes.cpp
    )

    add_executable(test_swf_parser
        src/swf_stream.cpp
        src/swf_parser.cpp
        tests/test_swf_parser.cpp
    )
    target_link_libraries(test_swf_parser PRIVATE ZLIB::ZLIB)

    enable_testing()
    add_test(NAME test_blend_modes COMMAND test_blend_modes)
    add_test(NAME test_swf_parser COMMAND test_swf_parser)
endif()

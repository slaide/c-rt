cmake_minimum_required(VERSION 3.10)

project(MyApp)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "default build type" FORCE)
endif()

message(STATUS "building in ${CMAKE_BUILD_TYPE} mode")

# Variables to match Makefile behavior
option(USEAVX "use avx simd instructions on linux/x64" OFF)
option(HIGH_PRECISION "use float precision instead of fixed point" OFF)
option(DECODE_PARALLEL "decode in parallel using pipelining" OFF)
option(JEMALLOC "use the jemalloc allocator for more consistent but worse best-case performance" OFF)

# Set default compiler
set(CMAKE_C_COMPILER "clang")
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_EXTENSIONS ON)
string(APPEND CMAKE_C_FLAGS " -Wall -Werror -Wpedantic -Wextra -Wno-sequence-point -Wconversion ")

list(APPEND ADDITIONAL_LIBS "vulkan" "pthread")

include_directories(include)

# Add compiler definitions based on settings
if(JEMALLOC)
    add_definitions(-DUSE_JEMALLOC)
    list(APPEND ADDITIONAL_LIBS "jemalloc")
endif()
if(HIGH_PRECISION)
    add_definitions(-DUSE_FLOAT_PRECISION)
endif()
if(DECODE_PARALLEL)
    add_definitions(-DJPEG_DECODE_PARALLEL)
endif()

# Platform-specific settings
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND ADDITIONAL_LIBS "vulkan" "pthread" "xcb" "xcb-util" "m")
    add_definitions(-DVK_USE_PLATFORM_XCB_KHR)
    string(APPEND CMAKE_C_FLAGS " -mssse3 ")

    if(USEAVX)
        string(APPEND CMAKE_C_FLAGS " -mavx2 ")
    endif()
    
    set(PLATFORM_SOURCES src/main_linux.c)

elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    include_directories(/opt/vulkansdk/macOS/include /opt/homebrew/include)
    link_directories(/opt/vulkansdk/macOS/lib /opt/homebrew/lib)
    list(APPEND ADDITIONAL_LIBS "-framework Appkit" "-framework Metal" "-framework MetalKit" "-framework QuartzCore")
    add_definitions(-DVK_USE_PLATFORM_METAL_EXT)
    
    set(PLATFORM_SOURCES src/main_macos.m)
else()
    message(FATAL_ERROR "Invalid platform: ${PLATFORM} (valid options are { linux | macos })")
endif()

# Source files and executable
set(SOURCES src/app.c src/app_mesh.c src/image.c src/huffman.c ${PLATFORM_SOURCES})
add_executable(main ${SOURCES})
target_link_libraries(main ${ADDITIONAL_LIBS})

# Handling different build modes
if(CMAKE_BUILD_TYPE MATCHES Release)
    set_property(GLOBAL PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

string(APPEND CMAKE_C_FLAGS_DEBUG          " -O1 ")
string(APPEND CMAKE_C_FLAGS_RELWITHDEBINFO " -Ofast ")
string(APPEND CMAKE_C_FLAGS_RELEASE        " -O3 ")

string(APPEND CMAKE_C_FLAGS_RELWITHDEBINFO " -ffast-math ")
string(APPEND CMAKE_C_FLAGS_RELEASE        " -ffast-math ")

set(CMAKE_C_FLAGS_DEBUG_ADDITIONAL " -g -fno-omit-frame-pointer -fno-inline ")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    string(APPEND CMAKE_C_FLAGS_DEBUG_ADDITIONAL " -fsanitize=address ")
endif()

string(APPEND CMAKE_C_FLAGS_DEBUG          ${CMAKE_C_FLAGS_DEBUG_ADDITIONAL})
string(APPEND CMAKE_C_FLAGS_RELWITHDEBINFO ${CMAKE_C_FLAGS_DEBUG_ADDITIONAL})

target_compile_definitions(main PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:RelWithDebInfo>:RELEASE>
    $<$<CONFIG:RelWithDebInfo>:DEBUG>
    $<$<CONFIG:Release>:RELEASE>
)

set(RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build")
file(MAKE_DIRECTORY ${RUNTIME_OUTPUT_DIRECTORY})
set_target_properties(main PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${RUNTIME_OUTPUT_DIRECTORY}
)

set(SHADER_SRC shaders/vertshader.vert shaders/fragshader.frag) # List your shader sources here
foreach(SHADER ${SHADER_SRC})
    get_filename_component(SHADER_NAME ${SHADER} NAME_WE) # Extract base name without extension
    set(GLSL_INPUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}")
    set(SPV_OUTPUT_FILE "${RUNTIME_OUTPUT_DIRECTORY}/${SHADER_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPV_OUTPUT_FILE}
        COMMAND glslangValidator --quiet -V -o ${SPV_OUTPUT_FILE} ${GLSL_INPUT_FILE}
        DEPENDS ${SHADER}
        COMMENT "Compiling shader ${SHADER}"
    )
    list(APPEND SPV_FILES ${SPV_OUTPUT_FILE})
endforeach()
add_custom_target(
    shaders DEPENDS ${SPV_FILES}
)
add_dependencies(main shaders)

# Additional functionality such as generating docs, counting loc, disassembling, and profiling can be added in a similar manner using `add_custom_target` or `add_custom_command`.

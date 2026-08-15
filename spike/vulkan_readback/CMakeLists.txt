# Throwaway build for the issue #25 spike. Separate from the plugin's own
# CMakeLists so the probe can be built for Windows and Linux while the real
# build system is still macOS-only — the three-way branch here is the sketch
# that issue #26 turns into the production version.
#
#   cmake -S spike/vulkan_readback -B build-spike && cmake --build build-spike

cmake_minimum_required(VERSION 3.21)
project(spike_vulkan_readback CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk")

add_library(spike_vulkan_readback SHARED
    spike_main.cpp
    spike_opengl.cpp
)

set_target_properties(spike_vulkan_readback PROPERTIES
    PREFIX ""
    SUFFIX ".xpl"
)

target_include_directories(spike_vulkan_readback PRIVATE "${SDK_DIR}")

target_compile_definitions(spike_vulkan_readback PRIVATE
    XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1 XPLM302=1 XPLM303=1
    XPLM400=1 XPLM410=1   # XPLM400 brings the Avionics API this spike probes
)

if(APPLE)
    target_compile_definitions(spike_vulkan_readback PRIVATE APL=1 IBM=0 LIN=0 GL_SILENCE_DEPRECATION)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")
    target_link_libraries(spike_vulkan_readback PRIVATE
        "${SDK_DIR}/Libraries/Mac/XPLM.framework/Versions/Current/XPLM"
        "-framework OpenGL"
    )
elseif(WIN32)
    target_compile_definitions(spike_vulkan_readback PRIVATE APL=0 IBM=1 LIN=0 NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(spike_vulkan_readback PRIVATE
        "${SDK_DIR}/Libraries/Win/XPLM_64.lib"
        opengl32
    )
else()
    target_compile_definitions(spike_vulkan_readback PRIVATE APL=0 IBM=0 LIN=1)
    # Linux plugins link no XPLM library at all — the host resolves those
    # symbols when it loads the .xpl. Adding -Wl,--no-undefined here would
    # break every XPLM call at link time.
    target_link_libraries(spike_vulkan_readback PRIVATE dl)
endif()

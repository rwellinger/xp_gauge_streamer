cmake_minimum_required(VERSION 3.21)
project(xp_gauge_streamer C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTS "Build unit tests" ON)
option(XP_GAUGE_STREAMER_SANITIZE "Enable ASan + UBSan on the unit tests" OFF)

# This plugin links against the macOS XPLM stub frameworks and targets Apple
# Silicon only — a non-Apple configure would fail later with a confusing error.
if(NOT APPLE)
    message(FATAL_ERROR "xp_gauge_streamer targets macOS (Apple Silicon) only.")
endif()

if(XP_GAUGE_STREAMER_SANITIZE)
    set(XP_GAUGE_STREAMER_SANITIZER_FLAGS
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
        -fno-sanitize-recover=all
    )
    message(STATUS "Sanitizer build: ASan + UBSan on unit tests (.xpl plugin is not instrumented)")
endif()

# arm64 only — no Universal Binary. Overridable from the command line so the
# unit tests can build for the host architecture.
if(NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
endif()
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")

# ── civetweb (downloaded by 'make setup' into vendor/civetweb) ─────────────────
# Built as its own static library so the plugin's strict warning flags and
# clang-tidy never apply to third-party C sources.
set(CIVETWEB_DIR ${CMAKE_SOURCE_DIR}/vendor/civetweb)

add_library(civetweb STATIC ${CIVETWEB_DIR}/src/civetweb.c)
set_target_properties(civetweb PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(civetweb SYSTEM PUBLIC ${CIVETWEB_DIR}/include)
target_include_directories(civetweb PRIVATE ${CIVETWEB_DIR}/src)
target_compile_definitions(civetweb PUBLIC
    USE_WEBSOCKET   # button/encoder events travel back to the plugin over WS
    NO_SSL          # loopback-only server; avoids an OpenSSL dependency
    NO_CGI
)
target_compile_options(civetweb PRIVATE -w)

# ── libjpeg-turbo (Homebrew, native arm64/NEON) ────────────────────────────────
# Linked statically so the .xpl stays self-contained — X-Plane loads the plugin
# from the aircraft/plugin folder where no dylib search path is set up.
find_path(TURBOJPEG_INCLUDE_DIR turbojpeg.h
    HINTS /opt/homebrew/opt/jpeg-turbo/include /usr/local/opt/jpeg-turbo/include
)

set(XP_GAUGE_STREAMER_SAVED_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
find_library(TURBOJPEG_LIBRARY NAMES turbojpeg
    HINTS /opt/homebrew/opt/jpeg-turbo/lib /usr/local/opt/jpeg-turbo/lib
)
set(CMAKE_FIND_LIBRARY_SUFFIXES ${XP_GAUGE_STREAMER_SAVED_SUFFIXES})

if(NOT TURBOJPEG_INCLUDE_DIR OR NOT TURBOJPEG_LIBRARY)
    message(FATAL_ERROR "libjpeg-turbo not found. Install it with: brew install jpeg-turbo")
endif()

# ── Version ───────────────────────────────────────────────────────────────────
file(READ "${CMAKE_SOURCE_DIR}/VERSION.txt" XP_GAUGE_STREAMER_VERSION)
string(STRIP "${XP_GAUGE_STREAMER_VERSION}" XP_GAUGE_STREAMER_VERSION)

if(NOT RELEASE)
    set(XP_GAUGE_STREAMER_VERSION "SNAPSHOT")
endif()

# ── Plugin target ─────────────────────────────────────────────────────────────
add_library(xp_gauge_streamer MODULE
    src/avionics_capture.cpp
    src/capture_schedule.cpp
    src/device_registry.cpp
    src/frame_buffer.cpp
    src/frame_pipeline.cpp
    src/http_server.cpp
    src/jpeg_encoder.cpp
    src/main.cpp
    src/plugin_log.cpp
    src/plugin_paths.cpp
    src/settings.cpp
)

set_target_properties(xp_gauge_streamer PROPERTIES
    OUTPUT_NAME "xp_gauge_streamer"
    SUFFIX      ".xpl"
    PREFIX      ""
)

# External deps as SYSTEM so compiler warnings and clang-tidy skip them.
# Own sources under src/ stay on the normal include path and remain linted.
target_include_directories(xp_gauge_streamer SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/sdk
    ${CMAKE_SOURCE_DIR}/sdk/XPLM
    ${CMAKE_SOURCE_DIR}/sdk/XPWidgets
    ${CMAKE_SOURCE_DIR}/vendor
    ${TURBOJPEG_INCLUDE_DIR}
)

target_compile_definitions(xp_gauge_streamer PRIVATE
    APL=1 IBM=0 LIN=0
    XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1 XPLM302=1 XPLM303=1
    XPLM400=1 XPLM410=1   # XPLM400 brings the Avionics API used for the GNS capture
    GL_SILENCE_DEPRECATION
    XP_GAUGE_STREAMER_VERSION="${XP_GAUGE_STREAMER_VERSION}"
)

target_compile_options(xp_gauge_streamer PRIVATE
    -Wall -Wextra -Wno-unused-parameter
    -fvisibility=hidden
)

# Use SDK stub frameworks for linking (X-Plane provides the real ones at runtime).
set(SDK_MAC_LIBS "${CMAKE_SOURCE_DIR}/sdk/Libraries/Mac")
target_link_libraries(xp_gauge_streamer PRIVATE
    "${SDK_MAC_LIBS}/XPLM.framework/Versions/Current/XPLM"
    "${SDK_MAC_LIBS}/XPWidgets.framework/Versions/Current/XPWidgets"
    "-framework OpenGL"
    civetweb
    ${TURBOJPEG_LIBRARY}
)

# ── Tests ─────────────────────────────────────────────────────────────────────
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

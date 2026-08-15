cmake_minimum_required(VERSION 3.21)
project(xp_gauge_streamer C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTS "Build unit tests" ON)
option(XP_GAUGE_STREAMER_SANITIZE "Enable ASan + UBSan on the unit tests" OFF)

# ── Platform ──────────────────────────────────────────────────────────────────
# The one place that branches on the host. Everything below reads these
# variables, so a fourth platform would be an entry here rather than a new
# condition scattered through the file.
#
#   XP_PLATFORM_FOLDER  X-Plane's folder name for this platform's binaries
#   XP_SDK_DEFINES      the SDK's APL/IBM/LIN triple, exactly one of them 1
if(APPLE)
    set(XP_PLATFORM_FOLDER "mac_x64")
    set(XP_SDK_DEFINES APL=1 IBM=0 LIN=0)
elseif(WIN32)
    set(XP_PLATFORM_FOLDER "win_x64")
    set(XP_SDK_DEFINES APL=0 IBM=1 LIN=0)
elseif(UNIX)
    set(XP_PLATFORM_FOLDER "lin_x64")
    set(XP_SDK_DEFINES APL=0 IBM=0 LIN=1)
else()
    message(FATAL_ERROR "Unsupported platform: X-Plane 12 runs on macOS, Windows and Linux.")
endif()

message(STATUS "Building for ${XP_PLATFORM_FOLDER}")

# Reading the machine's own address has no portable spelling: POSIX has
# getifaddrs, Windows has GetAdaptersAddresses. Two files, one header, and the
# choice made here rather than by a preprocessor branch inside either of them.
if(WIN32)
    set(XP_NETWORK_INFO_SOURCE src/network_info_windows.cpp)
else()
    set(XP_NETWORK_INFO_SOURCE src/network_info_posix.cpp)
endif()

if(XP_GAUGE_STREAMER_SANITIZE)
    set(XP_GAUGE_STREAMER_SANITIZER_FLAGS
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
        -fno-sanitize-recover=all
    )
    message(STATUS "Sanitizer build: ASan + UBSan on unit tests (.xpl plugin is not instrumented)")
endif()

if(APPLE)
    # arm64 only — no Universal Binary. Overridable from the command line so the
    # unit tests can build for the host architecture.
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64")
    endif()
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")
endif()

if(MSVC)
    # Static CRT so the .xpl runs without the Visual C++ redistributable — a
    # plugin dropped into an aircraft folder cannot ask the user to install one.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Strict on our own code, silent on third-party sources. Generator expressions
# rather than if(MSVC) so both compilers can be described in one place.
set(XP_WARNING_FLAGS
    $<$<CXX_COMPILER_ID:MSVC>:/W4;/wd4100;/permissive->
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wno-unused-parameter;-fvisibility=hidden>
)
set(XP_NO_WARNING_FLAG $<IF:$<C_COMPILER_ID:MSVC>,/w,-w>)

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
target_compile_options(civetweb PRIVATE ${XP_NO_WARNING_FLAG})
if(WIN32)
    target_link_libraries(civetweb PUBLIC ws2_32)
endif()

# ── libjpeg-turbo (downloaded by 'make setup' into vendor/libjpeg-turbo) ───────
# Built here as a subproject rather than found on the system: three platforms
# means three package managers disagreeing on version, layout and whether a
# static library exists at all. Static linkage keeps the .xpl self-contained —
# X-Plane loads it from a plugin folder with no library search path set up.
#
# ExternalProject rather than add_subdirectory because libjpeg-turbo refuses the
# latter outright (its CMakeLists aborts on it). It therefore configures and
# builds in its own CMake run and installs into the build tree, from where the
# plugin and the tests link one predictable static library.
include(ExternalProject)

set(TURBOJPEG_DIR ${CMAKE_SOURCE_DIR}/vendor/libjpeg-turbo)
set(TURBOJPEG_PREFIX ${CMAKE_BINARY_DIR}/libjpeg-turbo)
set(TURBOJPEG_INCLUDE_DIR ${TURBOJPEG_PREFIX}/include)

if(MSVC)
    set(TURBOJPEG_LIBRARY ${TURBOJPEG_PREFIX}/lib/turbojpeg-static.lib)
else()
    set(TURBOJPEG_LIBRARY ${TURBOJPEG_PREFIX}/lib/libturbojpeg.a)
endif()

ExternalProject_Add(libjpeg_turbo_build
    SOURCE_DIR ${TURBOJPEG_DIR}
    PREFIX ${CMAKE_BINARY_DIR}/libjpeg-turbo-build
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${TURBOJPEG_PREFIX}
        -DCMAKE_INSTALL_LIBDIR=lib   # some distributions would pick lib64
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON  # it ends up inside a shared module
        -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
        -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
        -DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}
        -DENABLE_SHARED=OFF
        -DENABLE_STATIC=ON
        -DWITH_TURBOJPEG=ON
        -DWITH_JAVA=OFF
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release --parallel
    INSTALL_COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR> --config Release
    BUILD_BYPRODUCTS ${TURBOJPEG_LIBRARY}
)

# The library file does not exist until the sub-build has run, so the imported
# target carries the dependency for everyone who links it.
add_library(turbojpeg STATIC IMPORTED GLOBAL)
set_target_properties(turbojpeg PROPERTIES IMPORTED_LOCATION ${TURBOJPEG_LIBRARY})
add_dependencies(turbojpeg libjpeg_turbo_build)
# The source tree's copy of turbojpeg.h is listed alongside the installed one so
# that configuring is enough to compile against it — clang-tidy runs off
# compile_commands.json without ever building the sub-project.
file(MAKE_DIRECTORY ${TURBOJPEG_INCLUDE_DIR})
target_include_directories(turbojpeg SYSTEM INTERFACE ${TURBOJPEG_INCLUDE_DIR} ${TURBOJPEG_DIR}/src)

# ── Dear ImGui (downloaded by 'make setup' into vendor/imgui) ─────────────────
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/vendor/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl2.cpp
)

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
    src/command_catalog.cpp
    src/command_dispatch.cpp
    src/device_registry.cpp
    src/frame_buffer.cpp
    src/frame_pipeline.cpp
    src/gl_entry_points.cpp
    src/http_server.cpp
    src/jpeg_encoder.cpp
    src/main.cpp
    ${XP_NETWORK_INFO_SOURCE}
    src/plugin_log.cpp
    src/plugin_paths.cpp
    src/plugin_ui.cpp
    src/settings.cpp
    ${IMGUI_SOURCES}
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
    ${IMGUI_DIR}
)

target_compile_definitions(xp_gauge_streamer PRIVATE
    ${XP_SDK_DEFINES}
    XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1 XPLM302=1 XPLM303=1
    XPLM400=1 XPLM410=1   # XPLM400 brings the Avionics API used for the GNS capture
    XP_GAUGE_STREAMER_VERSION="${XP_GAUGE_STREAMER_VERSION}"
)

target_compile_options(xp_gauge_streamer PRIVATE ${XP_WARNING_FLAGS})

target_link_libraries(xp_gauge_streamer PRIVATE civetweb turbojpeg)

if(APPLE)
    target_compile_definitions(xp_gauge_streamer PRIVATE
        GL_SILENCE_DEPRECATION
        IMGUI_ENABLE_OSX_DEFAULT_CLIPBOARD_FUNCTIONS # copying the URL is the window's point
    )
    # SDK stub frameworks for linking; X-Plane provides the real ones at runtime.
    set(SDK_MAC_LIBS "${CMAKE_SOURCE_DIR}/sdk/Libraries/Mac")
    target_link_libraries(xp_gauge_streamer PRIVATE
        "${SDK_MAC_LIBS}/XPLM.framework/Versions/Current/XPLM"
        "${SDK_MAC_LIBS}/XPWidgets.framework/Versions/Current/XPWidgets"
        "-framework OpenGL"
        "-framework ApplicationServices" # ImGui's macOS clipboard implementation
    )
elseif(WIN32)
    target_compile_definitions(xp_gauge_streamer PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    set(SDK_WIN_LIBS "${CMAKE_SOURCE_DIR}/sdk/Libraries/Win")
    target_link_libraries(xp_gauge_streamer PRIVATE
        "${SDK_WIN_LIBS}/XPLM_64.lib"
        "${SDK_WIN_LIBS}/XPWidgets_64.lib"
        opengl32
        iphlpapi   # GetAdaptersAddresses, the Windows side of network_info
    )
else()
    # Linux plugins link no XPLM library: the host resolves those symbols when
    # it loads the .xpl. Do not add -Wl,--no-undefined here — it would turn
    # every XPLM call into a link error.
    find_package(OpenGL REQUIRED)
    find_package(Threads REQUIRED)
    target_link_libraries(xp_gauge_streamer PRIVATE OpenGL::GL Threads::Threads ${CMAKE_DL_LIBS})
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Standalone test executable. Links Catch2 amalgamated source directly.
# Never links the X-Plane SDK — domain-logic tests only. The same SDK-free
# translation units used by the plugin target are compiled in here so the
# tests exercise the production code, not a re-implementation.

set(CATCH2_DIR ${CMAKE_SOURCE_DIR}/vendor/catch2)

add_executable(xp_gauge_streamer_tests
    ${CATCH2_DIR}/catch_amalgamated.cpp
    test_smoke.cpp
)

target_include_directories(xp_gauge_streamer_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_include_directories(xp_gauge_streamer_tests SYSTEM PRIVATE
    ${CATCH2_DIR}
    ${CMAKE_SOURCE_DIR}/vendor
)

target_compile_definitions(xp_gauge_streamer_tests PRIVATE
    XP_GAUGE_STREAMER_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

target_compile_options(xp_gauge_streamer_tests PRIVATE -Wall -Wextra -Wno-unused-parameter)

if(XP_GAUGE_STREAMER_SANITIZE)
    target_compile_options(xp_gauge_streamer_tests PRIVATE
        ${XP_GAUGE_STREAMER_SANITIZER_FLAGS} -O1 -g
    )
    target_link_options(xp_gauge_streamer_tests PRIVATE
        ${XP_GAUGE_STREAMER_SANITIZER_FLAGS}
    )
endif()

# Tests run on the host arch only — the plugin's arm64-only setting must not
# leak into a test binary built on an Intel machine.
set_target_properties(xp_gauge_streamer_tests PROPERTIES
    OSX_ARCHITECTURES "${CMAKE_HOST_SYSTEM_PROCESSOR}"
)

set_target_properties(xp_gauge_streamer_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

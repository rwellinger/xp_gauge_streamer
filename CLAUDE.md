# CLAUDE.md

Project guidance for Claude Code and other AI assistants. Cursor reads the same
content from [`.cursor/rules/xp-gauge-streamer.mdc`](.cursor/rules/xp-gauge-streamer.mdc).

## Project Overview

**xp_gauge_streamer** (shown in-sim as **Welly's Gauge Streamer**) is a C++17
X-Plane 12 plugin for **macOS (arm64 only), Windows x64 and Linux x64**. It captures the
default Laminar avionics displays — the GNS430/530 and the airliner CDU — streams
MJPEG to a browser frontend, and forwards button/knob presses back into the sim
over a WebSocket. The ToLiss Airbus MCDU and the Zibo 737 FMC, which X-Plane
does not render itself, are read from the aircraft's datarefs and sent as text
instead.

## Commands

```bash
make setup     # Download X-Plane SDK, civetweb, Catch2, nlohmann/json, Dear ImGui, libjpeg-turbo
make build     # Configure + compile → build/xp_gauge_streamer.xpl
make test      # Run the Catch2 unit tests
make install   # Copy plugin, web/ and default settings into X-Plane (code-signs on macOS)
make format    # clang-format over src/
make lint      # clang-tidy (bugprone-* and performance-* are errors)
make sanitize  # Build + run unit tests under ASan + UBSan in build-sanitize/
make release VERSION=x.y.z   # Tag + push release (commits VERSION.txt)
```

The plugin itself must still be validated in X-Plane 12. Unit tests cover domain
logic only — they never link the X-Plane SDK.

## Architecture

Everything lives in the `xp_gauge_streamer` namespace. Modules coordinate through
X-Plane's XPLM API; network threads must never call XPLM.

- **`main.cpp`** — Plugin entry points (`XPluginStart` / `Stop` / `Enable` /
  `Disable`). Starts capture, pipeline, text screen source, HTTP server, and command
  dispatch. Flight loop `follow_viewers` enables framebuffer readback only while
  `active_stream_count() > 0`, and screen text reading only while
  `active_screen_count() > 0`.
- **`avionics_capture`** — Registers draw callbacks per framebuffer device;
  copies raw RGB (bottom-up) on the main thread into a `FrameSink`. Expensive readback is
  gated by `set_readback_enabled`.
- **`frame_pipeline`** — One encoder thread per enabled device; JPEG via
  libjpeg-turbo. `publish_frame` copies and returns; `latest_jpeg` serves the
  newest encoded frame to the HTTP layer.
- **`text_screen_source`** — ToLiss and Zibo draw their CDU themselves, so
  there is no framebuffer to read: a flight loop reads the screen text from the
  aircraft's datarefs (`AirbusFBW/MCDU<n>…`, `laminar/B738/fmc<n>/…`,
  re-binding after each aircraft load) and keeps the newest screen as JSON.
  **`text_screen`** (SDK-free) paints layers into a 24×14 grid;
  **`text_screen_formats`** (SDK-free) holds each aircraft's layer table and
  glyph substitutions.
- **`device_presence`** — Which devices the loaded aircraft has; each source
  reports its own from the main thread, any thread reads.
- **`http_server`** — Embedded civetweb: static `web/`, MJPEG `/stream/<slug>`,
  server-sent events `/screen/<slug>` for text devices, JSON `/devices`,
  WebSocket `/control`. No authentication; binds per `settings.cfg`.
- **`command_dispatch`** — Queues presses from network threads; a flight loop
  resolves (lazily — aircraft commands appear only once it loads) and executes
  them on the main thread. Only whitelisted button names from
  `command_catalog` are accepted.
- **`device_registry`** — Static table of devices (`slug`, `type`,
  `command_prefix`, `ScreenSource`, enabled flag). SDK-free `DeviceId` so unit
  tests can link it. The command prefix follows the unit, not the model:
  `sim/GPS/g430n1_` and `sim/GPS/g430n2_` for the GNS pair, `sim/FMS/` and
  `sim/FMS2/` for the CDUs, `AirbusFBW/MCDU1`/`2` for the ToLiss family (A319,
  A320neo, A321, A340 — developed against the A319), `laminar/B738/button/fmc1_`
  /`fmc2_` for the Zibo 737. Text devices also carry a `dataref_prefix` — the
  same as the command prefix for ToLiss, `laminar/B738/fmc<n>/` for the Zibo.
  Devices unknown to X-Plane take ids from 100 up.
- **`plugin_ui`** — Dear ImGui settings window (address, port, device presence).
- **`settings`** — Reads/writes `<X-Plane>/Output/xp_gauge_streamer/settings.cfg`
  so updates do not wipe user config.

**Frontend.** Plain HTML/JS/CSS under `web/` — no framework, no build step.
Bezel layouts live in `web/bezels/<type>.json` (`button`, `rocker`, `knob`,
`grid`), with optional `labelSize`, `\n` line breaks in labels, `crop` to drop
unused black margin from a capture, and `shortcut` to bind a physical key
(single-character captions bind themselves). The screen node comes from
`device.js`: an `<img>` on `/stream/<slug>` for framebuffer devices, or
`text_screen.js` — a 24×14 cell grid fed by `EventSource` on `/screen/<slug>` —
when `/devices` reports `"screen": "text"`.

**Threading invariant.** Capture and XPLM calls stay on the main thread. Encoding
and HTTP run off-thread. Button presses cross that boundary only via
`press_button` → dispatch queue.

## Build Details

- **CMake 3.21+**, C++17. macOS 12.0+ **arm64 only** — no Universal Binary, so
  Intel Macs cannot load the plugin. Windows 10+ x64 (MSVC 2022), Linux x64 (GCC 11+).
- Output is `build/xp_gauge_streamer.xpl` (`build/Release/` on Windows)
- `sdk/` and `vendor/` are populated by `make setup`, not committed
- libjpeg-turbo is built from vendored source via ExternalProject and linked
  **statically** so the `.xpl` is self-contained. Its own CMakeLists refuses
  `add_subdirectory`. SIMD needs NASM on x64; Apple Silicon uses NEON.
- **Only macOS is used in anger.** Windows was verified in a cloud VM without a
  home network; Linux is best effort — CI proves it compiles, nothing more.
- The `.xpl` is intentionally not ASan-instrumented — use Instruments.app against
  the running X-Plane process for in-sim memory analysis

## Code Quality

All implementation in this repo must follow clean-code best practices. This
applies to every change, now and in the future:

- **Single responsibility**: each function does one thing; each module owns one concern.
- **Meaningful names**: variables, functions and types read as plain English (or the
  project's consistent language) — no abbreviations, no cryptic suffixes.
- **Small functions, shallow nesting**: prefer early returns and helpers over deeply
  nested conditionals.
- **DRY**: extract shared logic rather than copying it; but don't abstract
  speculatively without a concrete need.
- **Encapsulation**: keep statics/internals private to their translation unit; expose
  only what the header promises.
- **Separation of concerns**: UI code never touches file I/O directly; data modules
  never draw.
- **Minimal comments**: let the code explain itself. Add a comment only when the *why*
  is non-obvious (invariant, workaround, surprising constraint). Don't comment what the
  code already says.
- **No speculative generality**: don't build abstractions for hypothetical future
  needs — match the existing codebase style.
- **Boundaries only for validation**: trust internal code; validate at the edges (user
  input, external APIs, file parsing).
- **Platform differences live in their own translation units or in a single leaf
  seam — never in domain code.** `opengl.hpp` is the only place that includes a GL
  header, `gl_entry_points` the only place that resolves them, and `network_info`
  is two files behind one header chosen by CMake. This is the rule that keeps the
  port from rotting: a new `#ifdef` in a module that does real work is a defect.

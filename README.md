# xp_gauge_streamer

![Build](https://github.com/rwellinger/xp_gauge_streamer/actions/workflows/build.yml/badge.svg)

Native X-Plane 12 plugin for **macOS (Apple Silicon)** that captures the default
Laminar GNS430/530 display and streams it to a web frontend, where it can be
operated by click or touch instead of via the cockpit popout.

Status: build system only — see [issue #1](../../issues/1) for the feature work.

**Requirements:** macOS 12.0+ (arm64) · X-Plane 12 · CMake 3.21+ · Homebrew

## Build

```bash
make setup    # X-Plane SDK, civetweb, Catch2 → sdk/ + vendor/; libjpeg-turbo via Homebrew
make build    # Configure + compile → build/xp_gauge_streamer.xpl
make test     # Run the Catch2 unit tests
make install  # Code-sign and copy the plugin into X-Plane
```

`make help` lists every target. Neither `sdk/` nor `vendor/` is committed — both
are populated by `make setup`.

`make install` deploys to the path in the `XPLANE_ROOT` variable at the top of
the `Makefile`; adjust it for your installation. The folder name `mac_x64` is
X-Plane's name for 64-bit macOS plugins and applies to arm64 binaries too.

## Code quality

```bash
make format    # clang-format over src/
make lint      # clang-tidy (bugprone-* and performance-* are errors)
make sanitize  # unit tests under ASan + UBSan
```

The `.xpl` plugin is deliberately not ASan-instrumented — ASan inside the X-Plane
process is fragile on macOS arm64. Use Instruments.app against the running
X-Plane process for in-sim memory analysis.

## Layout

```
src/       plugin sources (X-Plane SDK)
tests/     Catch2 unit tests — never link the SDK, domain logic only
sdk/       X-Plane SDK headers + stub frameworks   (make setup)
vendor/    civetweb, Catch2                        (make setup)
```

## Dependencies

| Dependency    | Version | Source              | Purpose                            |
|---------------|---------|---------------------|------------------------------------|
| X-Plane SDK   | 4.3.0   | `make setup`        | Plugin API, Avionics capture       |
| civetweb      | 1.16    | `make setup`        | Embedded HTTP server + WebSocket   |
| libjpeg-turbo | latest  | Homebrew            | JPEG encoding (arm64/NEON)         |
| Catch2        | 3.15.3  | `make setup`        | Unit tests                         |

civetweb is used over mongoose because its MIT license matches this project's.
libjpeg-turbo is linked statically so the `.xpl` stays self-contained.

## Release

```bash
make release VERSION=0.2.0   # commits VERSION.txt, tags, pushes
```

Pushing a `v*` tag runs lint + build on CI and publishes a ZIP to the releases page.

## License

MIT — see [LICENSE](LICENSE).

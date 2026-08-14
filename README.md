# xp_gauge_streamer

![Build](https://github.com/rwellinger/xp_gauge_streamer/actions/workflows/build.yml/badge.svg)

Native X-Plane 12 plugin for **macOS (Apple Silicon)** that captures the default
Laminar GNS430/530 display and streams it to a web frontend, where it can be
operated by click or touch instead of via the cockpit popout.

Status: the GNS screen streams to the browser — see
[issue #1](../../issues/1) for the remaining feature work.

**Requirements:** macOS 12.0+ (arm64) · X-Plane 12 · CMake 3.21+ · Homebrew

## Watching the stream

**Plugins → xp_gauge_streamer → Stream settings** shows the address to type on
the tablet, which devices this aircraft has, and the port. Or open
`http://localhost:8080/` on this machine and `http://<mac-ip>:8080/` from a
tablet in the same network. A single device
stream is at `/stream/<slug>`, with the slugs `gns430_1`, `gns430_2`,
`gns530_1` and `gns530_2`.

The page asks the plugin which units the loaded aircraft actually has and
offers only those — a panel with a GNS530 and a second GNS430 reports
`gns530_1` and `gns430_2`, not `gns430_1`. The chosen unit lives in the URL
hash, so a tablet can bookmark just its own screen.

Around the stream sits the bezel: FPL, MENU, CLR, ENT, CRSR, D→, PROC, MSG,
OBS, CDI, VNAV, range, page and knob keys. It is drawn by the frontend, not
captured — X-Plane hands out the GNS screen only, never its frame. Presses
travel over the WebSocket and reach the sim as the unit's own commands.

The connection state is always visible, and the page reconnects on its own
after an X-Plane restart or a WLAN dropout — a dead frontend that looks alive
is worse in flight than a visible error.

Capturing only runs while a stream is open. With no viewer, the plugin costs
X-Plane nothing.

### Endpoints

| Endpoint          | Purpose                                                     |
|-------------------|-------------------------------------------------------------|
| `/`               | Start page, lists the units this aircraft has                |
| `/stream/<slug>`  | MJPEG stream of one unit                                     |
| `/devices`        | JSON: slug, name and whether the unit produces frames        |
| `/control`        | WebSocket: `{"device": "gns530_1", "button": "fpl"}`         |

Button names follow X-Plane's `sim/GPS/g430n*_` commands — `fpl`, `menu`,
`clr`, `ent`, `cursor`, `zoom_in`, `page_up`, … Only whitelisted names are
accepted; anything else is dropped and logged. Presses are queued and executed
on X-Plane's main thread, never from the network thread.

### Security

**The server has no authentication.** It binds to `0.0.0.0` on purpose — a
tablet as a second screen is the point of this plugin — so anyone on the same
network can watch the GNS, and from phase 6 on also operate it. Use it on a
network you trust, or restrict the server to this machine:

```
# <X-Plane>/Output/xp_gauge_streamer/settings.cfg
bind_address = 127.0.0.1
port = 8080
```

`make install` puts the default file there and never overwrites an existing
one; the plugin also writes it on first start if it is missing. The port can
also be set in the plugin window. Settings live under `Output/` so a plugin
update leaves them alone. Changes take effect on the next X-Plane start.

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
config/    default settings.cfg — installed into <X-Plane>/Output/
web/       frontend (index.html, app.js, style.css) — no framework, no build step
tests/     Catch2 unit tests — never link the SDK, domain logic only
sdk/       X-Plane SDK headers + stub frameworks   (make setup)
vendor/    civetweb, nlohmann/json, Dear ImGui, Catch2  (make setup)
```

## Dependencies

| Dependency    | Version | Source              | Purpose                            |
|---------------|---------|---------------------|------------------------------------|
| X-Plane SDK   | 4.3.0   | `make setup`        | Plugin API, Avionics capture       |
| civetweb      | 1.16    | `make setup`        | Embedded HTTP server + WebSocket   |
| libjpeg-turbo | latest  | Homebrew            | JPEG encoding (arm64/NEON)         |
| nlohmann/json | 3.12.0  | `make setup`        | Parsing control messages           |
| Dear ImGui    | 1.92.8  | `make setup`        | In-sim settings window             |
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

### Welly's Gauge Streamer for X-Plane 12

Native plugin for **macOS (Apple Silicon)**, **Windows (x64)** and **Linux (x64)**. Streams the default Laminar avionics displays — GNS430/530 and the airliner CDU — to a browser on a tablet or second screen, and sends key presses back into the sim. The ToLiss Airbus MCDU and the Zibo 737 FMC are served as text read from the aircraft's own datarefs.

### What's New in v1.1.0

  - **Zibo 737-800 FMC** — the Zibo draws its FMC in its own scripts and offers X-Plane no avionics device to capture, so until now it showed nothing. Like the ToLiss MCDU, its screen is now read as text from the aircraft's `laminar/B738/fmc1/…` and `fmc2/…` datarefs and drawn in the browser — sharp at any size and next to free for the sim. Both units are available as `zibo_fmc_1` (captain) and `zibo_fmc_2` (first officer). See [Zibo 737 FMC](https://github.com/rwellinger/xp_gauge_streamer#zibo-737-fmc).
  - **737 bezel** — six line selects down each side; INIT REF, RTE, CLB, CRZ, DES, MENU, LEGS, DEP ARR, HOLD, PROG and EXEC; N1 LIMIT, FIX, PREV PAGE and NEXT PAGE; number pad and letters with SP, DEL, `/` and CLR. All 69 FMC keys of the aircraft are wired. From a keyboard, letters and digits type themselves, space is SP, Delete is DEL and Backspace is CLR.
  - **The FMC's colours and symbols** — white, green, magenta, cyan titles and inverse highlights as the 737 draws them; entry boxes and the degree sign appear as such.
  - **Inverse video on text screens** — `/screen/<slug>` gains the colour letter `i` (black on white). Clients that do not know it fall back to white text.
  - The ToLiss MCDU now runs on the same shared text screen code as the Zibo. Nothing changes in how it looks or behaves.

### What's New in v1.0.0

  - First release: GNS430/530 and the airliner CDU streamed as MJPEG, with bezels drawn by the frontend and presses forwarded over a WebSocket.
  - **ToLiss MCDU** — A319, A320neo, A321 and A340, read as text from the `AirbusFBW/MCDU1…` and `MCDU2…` datarefs, with an Airbus bezel.
  - Installs into `xplaunchData/Plugins/` when XPLaunch is in use, otherwise into `Resources/plugins/`.


### Features

  - Selection page at `http://<your-ip>:8080/` — one tile per unit, showing whether the loaded aircraft has it
  - One page per unit at `/device/<slug>`, so a tablet can bookmark just its own screen
  - GNS430 / GNS530 (pilot and copilot) and the airliner CDU (captain and first officer), captured from X-Plane's own framebuffer
  - ToLiss MCDU and Zibo 737 FMC as text, without framebuffer readback
  - Bezels drawn in the browser from JSON — keys, rockers and knobs, operated by click, touch or keyboard
  - Capture runs only while somebody watches; with no open stream the plugin costs the sim nothing
  - Reconnects on its own after an X-Plane restart or a WLAN dropout
  - Only whitelisted commands reach the sim; presses run on X-Plane's main thread
  - In-sim settings window (**Plugins → Welly's Gauge Streamer → Stream settings**) with the address to open, the port, and the units of the loaded aircraft


### Installation

  Download `xp_gauge_streamer.zip` from the release assets, unzip it, and copy the `xp_gauge_streamer` folder into your X-Plane `Resources/plugins/` directory. The ZIP contains all three platform binaries (`mac_x64/`, `lin_x64/`, `win_x64/`) together with the `web/` frontend — X-Plane 12 loads the right binary automatically. See the [README](README.md) for setup.


### Requirements

  - macOS 12.0+ on Apple Silicon (Intel Macs are not supported), Windows 10/11 x64, or Linux x64
  - X-Plane 12
  - A browser on a device in the same network


### Known Limitations

  - Only macOS is flown by the developer. Windows was verified in a cloud VM without a home network; Linux is best effort — compiled by CI, never loaded in the sim
  - No authentication — anyone in the network who can reach the port can press keys
  - Windows asks once whether to allow the port through the firewall; declining it makes the tablet reach nothing
  - Aircraft with their own FMC other than ToLiss and Zibo bind no avionics device and show as absent
  - A black CDU, MCDU or FMC screen usually means the aircraft has no electrical power yet

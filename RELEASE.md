### Welly's Gauge Streamer for X-Plane 12

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


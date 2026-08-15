# Spike: avionics readback under Vulkan (issue #25)

A throwaway plugin that does exactly what `avionics_capture` does — register an
avionics after-draw callback, read the framebuffer through a pixel buffer object
— and reports what happened. It answers whether the port to Windows and Linux
can proceed at all. **Not part of the plugin, never shipped.**

## Getting a binary

The three builds come from the manual workflow **Spike — Vulkan readback**
(Actions tab → Run workflow). It produces one artifact per platform:
`spike25-win_x64`, `spike25-lin_x64`, `spike25-mac_x64`.

macOS locally:

```bash
cmake -S spike/vulkan_readback -B build-spike -DCMAKE_BUILD_TYPE=Release
cmake --build build-spike
codesign --force --sign - build-spike/spike_vulkan_readback.xpl
```

## Running it

Put the `.xpl` in its own plugin folder, keeping X-Plane's platform folder name:

```
X-Plane 12/Resources/plugins/spike_vulkan_readback/win_x64/spike_vulkan_readback.xpl
                                                  /lin_x64/…
                                                  /mac_x64/…
```

Then load an aircraft that has one of the six stock units — the **stock B737-800**
is the safe choice, because it binds both CDUs and needs no add-on. Give the
avionics power (GPU or APU, or the CDU screen stays black), let it draw for a few
seconds, quit X-Plane.

Hand back two files:

- `X-Plane 12/Log.txt` — every line starting `[spike25]`
- `X-Plane 12/spike25_frame.ppm` — the 50th captured frame

## Reading the result

The log answers the four questions from issue #25:

| Log line | Means |
|---|---|
| `first callback from …` | the callback fires under Vulkan at all |
| `GL entry points resolved (…)` | the six GL 1.5 functions exist — via `wglGetProcAddress` on Windows, `dlsym` on Linux |
| `GL 1.5 entry points unavailable` | **No-Go** for that platform |
| `viewport WxH at origin X,Y` | the capture geometry matches what macOS sees |
| `glReadPixels: glGetError = 0x0502` | `GL_INVALID_OPERATION` — the framebuffer is multisampled and the port needs a resolve blit (+6 h, second spike) |
| `glMapBuffer returned null` | the readback never lands in host memory — **No-Go** |
| `… frames, average N us` | the frame-time cost; tens of microseconds is fine, milliseconds means the capture rate has to become a per-platform setting |

The `.ppm` must show the unit's screen: not black, not the 3D view, and the same
way up as on macOS. Compare it against a macOS run of the same aircraft — that is
what the third build in the matrix is for.

## Scope

One device only: whichever unit's callback fires first becomes the subject, and
the other five are ignored so the timing series belongs to one unit. 100 frames,
then it stops measuring and says so.

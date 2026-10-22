# Docked research workspace

The desktop launchers use a docked layout with project/object tools on the left,
the slide canvas in the center, image/overview/display controls on the right,
and measurements/analysis/jobs below the canvas. Annotation tools remain above
the workspace. Drag tabs to rearrange panels and splitters to resize them.
The Reset layout button restores the default arrangement. Multiple native
platform windows are not enabled; floating panels stay inside the main window.

Dear ImGui is pinned to docking commit
`4806a1924ff6181180bf5e4b8b79ab4394118875`, the dereferenced
[`v1.91.9b-docking` tag](https://github.com/ocornut/imgui/tree/4806a1924ff6181180bf5e4b8b79ab4394118875).
The DockBuilder calls are confined to `cpp/gui_workspace.hpp`; this internal
API must be reviewed when upgrading the pin. The upstream
[docking guide](https://github.com/ocornut/imgui/wiki/Docking) documents tab
dragging and identifies DockBuilder as an internal API.

The embedded Python/C++ `show`/`run_gui` API retains its compact layout by
default for existing callers. Enable docking explicitly:

```python
import qupathxx as q

options = q.GuiOptions()
options.docked_workspace = True
options.layout_path = q.default_workspace_layout_path()
report = q.show(project, options=options)
```

An empty `layout_path` keeps layout state within that viewer invocation.
Nonempty paths must end in `.ini`. `reset_layout=True` ignores previously
stored settings and restores the default arrangement. Supplying layout/reset
options with the compact layout is an error. `GuiReport` exposes
`docked_workspace`, `layout_loaded`, `layout_saved`, `layout_error`, and the
logical outer panel rectangles `browser_bounds`, `display_bounds`,
`measurement_bounds` and `canvas_bounds` for docked runs. `canvas_visible`
distinguishes a selected/visible canvas from one hidden behind another dock tab.
Hidden canvases skip tile uploads, geometry and canvas-cache submission while
background commands and the independent overview continue. Finite settlement
waits for the visible view and overview, rather than forcing a hidden canvas
to upload its outstanding tile results.

Both launchers accept `--compact`, `--layout preferences.ini` and
`--reset-layout`. Normal docked launches use `workspace-v1.ini` under
`%APPDATA%/qupathxx` on Windows, or `$XDG_CONFIG_HOME/qupathxx` (falling back to
`$HOME/.config/qupathxx`) elsewhere. Windows falls back to
`%USERPROFILE%/AppData/Roaming` when APPDATA is absent. If no absolute config
root is available, the default path is empty. Smoke launches use in-memory
preferences unless an explicit `--layout` path is supplied.

Layout preferences are separate from project files. They contain ImGui window
and docking settings with a `qupathxx workspace layout v1` marker, bounded to
1 MiB. Loading does not execute scripts. Preference loading is synchronous at
viewer setup; saving uses an atomic replacement at normal viewer return.
Preference errors are reported without closing or modifying the project.
Unrecognized/oversized files are preserved until an explicit reset; reset
authorizes replacing them. The parent directory is created on save when needed.
Two viewers sharing one preference path use the last successful writer; layouts
are user preferences, not a transactional multi-writer project resource.
LF and CRLF files are accepted. Layout saving/reset cannot replace a current
project, recovery source or image source, including a project named with an
`.ini` extension. A tabbed-away canvas does not publish an uninitialized view
over the stored project camera.

Resizing a dock panel preserves the source point at the canvas center and the
zoom. It cancels unfinished drawing gestures, invalidates screen-dependent
render caches and leaves a synchronized project view unchanged. Image/workspace
switching retains the existing project-owned view and selection contracts;
layout settings never become project history commands or dirty the project.

The GUI replay surface adds `GuiInputTarget.WINDOW` for normalized main-window
coordinates and `RESET_LAYOUT` for the actual reset button. Existing canvas,
overview, image-coordinate and named-button replay continues to use their
rendered widget positions.

## Validation and timing

Fourteen focused Windows cases cover actual splitter dragging and separate
save/reload/reset (including a Unicode path), exact canvas pixels and cache
equivalence, resize, image switching, empty projects, history, hidden-canvas
settlement, preference failures/source protection and standalone launch.
The Windows native suite passes 34 cases; a local minimal Linux build passes
33. Strict typing and generated-stub checks include the new API.

The [Aperio navigation samples](benchmarks/docked-aperio-navigation-windows.json)
record p95 CPU frame work of 4.00/22.28 ms for two 180-frame views. The
[large synthetic navigation samples](benchmarks/docked-large-navigation-windows.json)
record 4.14/21.39 ms. Both use 1440 x 900 logical windows, Mesa llvmpipe and
real wheel/overview replay. Main tile payload peaks below 20 MiB, with
no pending tiles at exit. The overview's separate payload/state is recorded.
The small single-level Aperio fixture remains unavailable under the default
thumbnail decode budget, as specified by the [thumbnail policy](thumbnails.md).

These are observations from the active development host, with asynchronous
read scheduling and variable upload counts. They are not a demonstrated
speedup, GPU-completion measurement or acceptance of M0's proposed hardware
performance targets. CPU frame work excludes explicit swap/vsync and capture;
driver calls can block. Reproduce with the installed GUI wheel:

```text
python -I tools/benchmark_viewer.py build/fixtures/CMU-1-Small-Region.svs --width 1440 --height 900 --frames 180 --navigate --docked --output build/docked-viewer.json
```

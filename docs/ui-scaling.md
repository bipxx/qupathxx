# Monitor-aware UI scaling

The viewer samples GLFW window content scale and framebuffer density before
each rendered frame. Font metrics and style sizes use
`content_scale / framebuffer_density * user_multiplier`; font rasterization
uses framebuffer density. Thus a 200% Windows monitor with one framebuffer
pixel per window coordinate uses twice-sized controls, while a 2x Retina
framebuffer keeps normal logical controls and rasterizes sharper glyphs.
This follows GLFW's distinction between
[window content scale and framebuffer size](https://www.glfw.org/docs/3.4/window_guide.html#window_scale).
The larger axis is used for the isotropic UI when reported axes differ.

`GuiOptions.ui_scale` sets the user multiplier, default `1`, range `0.5..3`.
Both desktop launchers accept `--ui-scale 1.5`. The UI minus/plus buttons adjust
the multiplier in 25 percentage-point steps; Auto UI restores `1`.
These controls work in compact and docked workspaces. The multiplier belongs
to the viewer invocation, is not persisted in layout preferences and never
enters project history. `GuiReport.ui_scale` reports the effective logical
scale, alongside `monitor_scale`, `framebuffer_scale` and `font_rebuilds`.

Style dimensions are rebuilt from the original theme, avoiding cumulative
rounding when moving between scales. Font textures rebuild on the GL owner
thread only when effective scale or density changes.
The OpenGL backend owns texture creation in its `NewFrame`, after device
initialization, avoiding a duplicate startup upload and an untracked texture.
Cached canvas overlays are explicitly invalidated even if OpenGL reuses a font
texture identifier.
Unfinished drawing gestures are cancelled on scale changes. Existing camera
resize handling preserves the source point at the canvas center and zoom;
tile LOD continues to use actual framebuffer density independently of UI size.

Invalid user multipliers fail before window creation. Monitor and framebuffer
reports are defensively bounded to `0.5..4`, with invalid values falling back
to `1`; the resulting logical UI scale is bounded to `0.5..6`.
`GuiInputKind.SET_UI_SCALE` supplies a multiplier in `x` for deterministic
between-frame replay. Repeating the same value does not rebuild fonts.

The automated campaign covers Windows/Retina scale arithmetic, native GL font
rebuilds, repeated changes, camera and project invariants, routed image input,
and exact cached-versus-immediate framebuffer equality. Replaying a user
multiplier does not emulate OS monitor delivery. Physical mixed-DPI monitor
moves, macOS Retina and fractional-density hardware acceptance remain open.
The [responsive workspace](responsive-workspace.md) addresses the clipped
inspector labels and toolbar hint observed at 200% in a 1440 x 900 window.
Field labels stack, long buttons wrap and the toolbar offers Help when its
full hint does not fit. Physical monitor delivery and full high-DPI hardware
acceptance remain open.

To collect a navigation sample with live scale changes from an installed GUI
wheel, run:

```text
python -I tools/benchmark_viewer.py image.svs --width 1440 --height 900 --frames 180 --navigate --docked --scale-cycle --output build/ui-scaling.json
```

The JSON records artifact/source identities, effective scale, font rebuild
counts, p95 and maximum CPU frame work, and GPU payload. It does not measure
GPU completion latency or establish a speedup over another build.


## DPI font baseline

These records cover packet 65, before responsive controls. Current responsive
control evidence is described in the [responsive workspace](responsive-workspace.md).

The installed Windows software-GL suite passed 1,307 tests with six skips.
A final 47-case docking/scaling/provenance run covers compact scaled resize,
accumulated small scale changes, exact overlay pixels and both launchers. Windows native tests pass 34, local Ubuntu minimal tests pass 33,
and source-exact stubs plus strict positive/seven-error negative typing pass.
The installed-workflow check opens a separate-layout docked workspace at 125%
and verifies exactly one initial font build.
The [local artifact record](benchmarks/ui-scaling-local-windows-linux.json)
retains test/log hashes, wheel identity and the matching installed workflow.

The [Aperio sample](benchmarks/ui-scaling-aperio-navigation-windows.json)
records 5.05/24.15 ms p95 and 33.20/49.93 ms maximum CPU frame work for two
180-frame views. The [large synthetic sample](benchmarks/ui-scaling-large-navigation-windows.json)
records 4.31/18.66 ms p95 and 29.94/24.09 ms maximum. Both replay navigation
and three live scale changes. Each run rebuilds fonts exactly four times
(initial plus changes), finishes with no pending tiles and stays below 18 MiB
of main tile GPU payload. The overview payload is reported separately; the
single-level Aperio overview remains explicitly unavailable at its default
source-decode budget. Artifact and source hashes match the installed check.

These are active-development-host llvmpipe observations, not controlled cold
runs, a demonstrated speedup, GPU completion latency or acceptance of the
16.7 ms hardware frame budget. Hardware and broader M0/M2 performance gates
remain open.


The [corrected CI run](https://github.com/bipxx/qupathxx/actions/runs/39796416658)
completed successfully across all five configurations. Windows software-GL
Python passes 1,306 with seven skips and 34 native tests; Windows headless
passes 1,033 with 280 skips and 33 native tests; Linux headless passes 1,027
with 286 skips and 33 native tests. Minimal Linux and ASan/UBSan each pass 33.
The [CI artifact record](benchmarks/ui-scaling-ci-windows-linux.json) verifies
wheel/native/source identities, stubs and typing logs, standalone CLI checks,
and the standalone ZIP against its wheel without Python or Mesa payloads.
The aggregate run reports completed/success; two job summaries were still stale
at capture, so their successful build, artifact and Complete job steps are
retained explicitly rather than rewriting those reported statuses.

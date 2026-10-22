# Tiled viewer implementation and evidence

Dense and OpenSlide documents share an asynchronous tiled rendering path. Both
CLIs accept PPM or OpenSlide-supported images. No whole-slide pixel allocation
is required to fit or navigate a slide.

The planner selects a level using zoom and framebuffer scale. Coarse fallback
tiles load first when affordable, with central detail tiles prioritized next.
The main view and overview each request at most 128 tiles, sharing decoded/GPU
entries for identical regions. When the available pyramid cannot cover the
view within that limit, the canvas explicitly asks the user to zoom in. Native
tests exercise trillion-pixel metadata without allocating large images.

Geometry stays double precision until final screen conversion. Integer tile
origins use level-zero coordinates with rounded shared edges for noninteger
factors. Additional vendor fixtures are needed to verify all resampling cases.

The controller consumes results on the application thread and rejects obsolete
generations. GPU textures use a 64 MiB payload LRU, independently of the 64 MiB
decoded cache and 64 MiB work budget. These caps exclude driver/reader overhead.
Uploads stop after eight tiles or after crossing a four-millisecond admission
deadline; a single driver call cannot be interrupted. Stable views reuse plans.
OpenGL allocation and destruction stay on the context thread; workers shut down
before context destruction. Immutable uploaded tiles are reused between views.

The scale bar uses horizontal calibration. Mean RGB is explicitly requested on a
native task; changing selection suppresses its result. Closing cancels between
read blocks and joins the task. Decoding and repeated measurement calculation
stay outside the frame loop.

## Validation and benchmark

Four native suites cover core, readers, scheduling and tile planning. Thirty-two
Python tests pass with `QUPATHXX_TEST_GUI=1` and the identified Aperio fixture.
The GUI checks render known constant RGB pixels and verify exact framebuffer
pixels, upload counts, GPU payload and repeated viewer cleanup. A real Aperio
run uploaded 108 tiles with 26,346,960 bytes of GPU payload and no pending tiles;
its captured image was visually inspected.

```powershell
$env:QUPATHXX_TEST_GUI = "1"
$env:QUPATHXX_APERIO_FIXTURE = (Resolve-Path build/fixtures/CMU-1-Small-Region.svs).Path
.venv/Scripts/python -m pytest -q
.venv/Scripts/python tools/benchmark_viewer.py $env:QUPATHXX_APERIO_FIXTURE
```

On the Windows 11/Python 3.12 development host, a 1440 x 900 logical window and
180-frame static-fit workload reported 16.94 ms p95 CPU frame work on first view
and 1.87 ms on repeat view. Both used the pinned small Aperio fixture, uploaded
108 tiles, used 26,346,960 bytes of GPU payload and finished without pending
tiles. These are observations, not a cross-machine guarantee or demonstrated
speedup. CPU timing includes planning/upload/draw submission and excludes explicit
swap, vsync and capture. Driver calls can still block. It does not measure GPU
completion or interactive motion latency.

## Remaining acceptance work

Overview navigation and RGB display controls are implemented. Tests replay
pointer pan, wheel zoom, rectangle drawing and overview clicks through ImGui's
input queue. The initial 2560 x 1440 synthetic pyramid workload is implemented;
vendor cold reads, per-monitor DPI changes, prolonged cache
pressure and the 100,000-object culling benchmark remain open. ICC transforms,
fluorescence controls and docking/layout work need separate implementation.

## Overview and display controls

The overview shares tiles with the main view and outlines its visible region.
Click or drag to center the main view at an image point without changing zoom.
Its full-image tile plan is stable across navigation. If the source lacks enough
pyramid levels for bounded coverage, it explicitly reports a limited overview.
This is tiled sampling rather than a separate whole-image thumbnail allocation.

Display settings select RGB, grayscale or a single RGB channel, with black/white
window and gamma controls. These settings transform raw bytes with a 256-entry
lookup table only during GPU upload. The identity path uploads raw bytes directly.
Changing settings discards display textures but preserves raw decoded cache
entries. Measurements and source metadata do not change. This remains RGB8
display, not a fluorescence channel model or quantitative stain transformation.

Native window resizing, minimize/restore behavior and the reduced-work owner
loop are covered by the [window lifecycle contract](window-lifecycle.md).
Per-monitor DPI transitions and hardware/OS input delivery remain separate gates.

The Python API exposes DisplaySettings, GuiOptions and deterministic GuiInput
records. Replay uses the same ImGui events as GLFW input. Coordinates are
normalized within the canvas or overview; events start at frame 2, after layout.
Reports include final viewport state, actual framebuffer dimensions and OpenGL
renderer/version. Replay validates the interaction handler; it does not test
operating-system mouse delivery or DPI transition behavior.

Generate the larger fixture and exercise navigation:

```powershell
.venv/Scripts/python tools/generate_large_fixture.py
.venv/Scripts/python tools/benchmark_viewer.py build/fixtures/large-pyramid.tif --navigate --frames 180
```

The generator writes a 100352 x 2048, nine-level RGB TIFF using one reusable
256 x 256 tile. It never allocates the full image. Constant RGB (40,80,160) gives
an independent pixel expectation at every level; the recipe is in the fixture
manifest. Its highly compressible pixels test large coordinates and navigation,
not representative tissue decoding throughput. Keep real vendor benchmarks as
a separate requirement.

The committed [Windows navigation record](benchmarks/large-navigation-windows.json)
uses an actual 2560 x 1440 framebuffer on NVIDIA GeForce RTX 4090, OpenGL 3.2,
driver 610.88. Two runs each performed 31 generations with p95 CPU frame work of
16.97 and 16.93 ms. GPU payload peaked at 9,728,256 and 9,318,656 bytes. Both ended
with zero pending tiles. Asynchronous timing changes how many intermediate tiles
finish before they become obsolete, so upload counts need not match between runs.
This meets the proposed 33 ms CPU frame target on this synthetic workload only;
it does not establish the vendor-slide or 100,000-object performance gates.

# Tiled viewer implementation and evidence

Dense and OpenSlide documents share an asynchronous tiled rendering path. Both
CLIs accept PPM or OpenSlide-supported images. No whole-slide pixel allocation
is required to fit or navigate a slide.

The planner selects a level using zoom and framebuffer scale. Coarse fallback
tiles load first when affordable, with central detail tiles prioritized next.
Requests are capped at 192 tiles. When the available pyramid cannot cover the
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

M2 remains open for overview navigation, channel/display controls and layout
polish. Exercise actual pointer pan/zoom/drawing, resize, minimize and per-monitor
DPI changes. Benchmark a large multilevel vendor slide, cold reads, rapid
generation changes and sustained cache pressure. Establish the plan's
2560 x 1440 workload and 100,000-object culling benchmark. ICC transforms and
fluorescence controls need separate implementation.

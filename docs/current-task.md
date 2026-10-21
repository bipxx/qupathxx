# Current implementation task

Goal: implement the project plan end to end and verify optimized performance.
The full M0 through M7 scope remains active. The current packet adds overview,
RGB display controls and interaction/performance evidence; M2 remains open.

Current sources include the tile planner, scheduler-backed GUI controller,
bounded GPU cache, scale bar, background mean measurement, overview and RGB
channel/window/gamma controls. Evidence: 39 Python tests with real OpenGL
framebuffer and pointer replay checks; four native suites with and without
OpenSlide; GUI wheel installation checks; real viewer framebuffer inspected.
Performance scope is recorded in [tiled-viewer.md](tiled-viewer.md).

The generated 100352 x 2048 nine-level fixture passes navigation checks on an
actual 2560 x 1440 framebuffer. Committed navigation benchmarks show 31
generations, roughly 17 ms p95 CPU frame work and under 10 MiB GPU tile payload.
These highly compressible synthetic pixels do not substitute for vendor stress.

Next implement M3 command-based object edits and undo/redo, followed by robust
geometry, hierarchy and classes. Probe the geometry dependency before committing
to polygon topology behavior. Preserve remaining M2 work: OS/DPI delivery, resize/
minimize, docking, fluorescence/ICC, prolonged cache pressure and vendor benchmarks.
Keep readers, scheduler, planner, GUI and installed package tests passing.

Outstanding M0 decisions: geometry dependency probe, persistence schema probe,
project license choice (asked asynchronously; no license selected yet), broader
malformed-image fixtures and numerical comparison tolerances. Benchmark host and
renderer are recorded for the current workload. Record decisions with evidence.

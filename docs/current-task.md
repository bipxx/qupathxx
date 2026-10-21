# Current implementation task

Goal: implement the project plan end to end and verify optimized performance.
The full M0 through M7 scope remains active. The current packet implements the
rectangular command model and bounded undo/redo. M2 and M3 remain open.

Current sources include tiled rendering, overview, RGB display controls, indexed
rectangular objects, atomic command batches, parent/classes, subtree deletion,
monotonic revisions and bounded undo/redo shared by Python/ImGui. Evidence:
49 Python tests including command/hierarchy and GUI keyboard history replay;
five native suites with and without OpenSlide; installed GUI wheel checks.
Performance scope is recorded in [tiled-viewer.md](tiled-viewer.md).

The generated 100352 x 2048 nine-level fixture passes navigation checks on an
actual 2560 x 1440 framebuffer. Committed navigation benchmarks show 31
generations, roughly 17 ms p95 CPU frame work and under 10 MiB GPU tile payload.
These highly compressible synthetic pixels do not substitute for vendor stress.

The 100000-object document benchmark records approximately 203 ms batch creation,
117–120 ms undo/redo, and 447 bytes of journal payload for a rename. This does
not satisfy the 100000-overlay rendering gate. See document-commands.md.

Next probe and integrate robust geometry for polygon holes, multipart regions,
ellipses, lines and points. Extend the command model to detection/cell objects,
class registries, multi-selection and spatial queries, retaining atomic undo.
Preserve remaining M2 work: OS/DPI delivery, resize/minimize, docking,
fluorescence/ICC, prolonged cache pressure and vendor benchmarks. M4–M7 remain.
Keep reader, scheduler, planner, GUI and installed package checks passing.

Outstanding M0 decisions: geometry dependency probe, persistence schema probe,
project license choice (asked asynchronously; no license selected yet), broader
malformed-image fixtures and numerical comparison tolerances. Benchmark host and
renderer are recorded for the current workload. Record decisions with evidence.

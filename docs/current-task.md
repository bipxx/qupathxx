# Current implementation task

Goal: implement the project plan end to end and verify optimized performance.
The full M0 through M7 scope remains active. M2 and M3 remain open.

Current sources include tiled rendering, overview, RGB display controls, atomic
commands, bounded undo/redo, general immutable ROIs, cell/nucleus objects and spatial queries. GEOS
provides polygon topology; ellipse metrics/masks use analytic geometry. Python
and ImGui share the native model. See geometry-spatial.md and document-commands.md.

Evidence: 80 Python tests including all shape tools and GUI history replay;
seven native suites with optional geometry/readers enabled and disabled;
installed GUI wheel checks with packaged runtime libraries and licenses.
The 100000-object document benchmark records approximately 249 ms batch creation,
121 ms undo, 117 ms redo and 783 bytes of history payload for a rename. The spatial
benchmark covers 1000 region queries and a cached narrow GUI view of 828 visible
objects. This does not satisfy the all-100000-overlays or complex-shape gate.
The existing large-pyramid navigation evidence remains scoped to synthetic pixels.

Detection/cell kinds and optional nuclei are implemented; see object-kinds.md.
Next implement class registries/colors and
multi-selection, then vertex editing and Boolean clipping. Preserve all unfinished
M2 work: OS/DPI delivery, resize/minimize, docking, fluorescence/ICC, prolonged
cache pressure and vendor benchmarks. Persistence, interchange, analysis,
classifiers, scripting and packaging work under M4-M7 remains active.

Outstanding M0 decisions: persistence schema probe, project license choice
(asked asynchronously; no license selected yet), broader malformed-image
fixtures and numerical QuPath comparison tolerances. GEOS dependency selection
and packaging are now recorded with source identity and independent tests.

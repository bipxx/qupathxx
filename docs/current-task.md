# Current implementation task

Goal: implement the project plan end to end and verify optimized performance.
The full M0 through M7 scope remains active. M2 and M3 remain open.

Current sources include tiled rendering, overview, RGB display controls, atomic
commands, bounded undo/redo, general immutable ROIs, cell/nucleus objects, a shared class registry and spatial queries. GEOS
provides polygon topology; ellipse metrics/masks use analytic geometry. Python
and ImGui share the native model. See geometry-spatial.md and document-commands.md.

Evidence: 91 Python tests including all shape tools and GUI history replay;
eight native suites with optional geometry/readers enabled and disabled;
installed GUI wheel checks with packaged runtime libraries and licenses.
The 100000-object document benchmark records approximately 230 ms batch creation,
116 ms undo, 126 ms redo and 815 bytes of history payload for a rename. The spatial
benchmark covers 1000 region queries and a cached narrow GUI view of 828 visible
objects. This does not satisfy the all-100000-overlays or complex-shape gate.
The existing large-pyramid navigation evidence remains scoped to synthetic pixels.

Detection/cell kinds, optional nuclei and undoable class definitions/colors are
implemented; see object-kinds.md and classes.md. Next implement multi-selection,
then vertex editing and Boolean clipping. Preserve all unfinished
M2 work: OS/DPI delivery, resize/minimize, docking, fluorescence/ICC, prolonged
cache pressure and vendor benchmarks. Persistence, interchange, analysis,
classifiers, scripting and packaging work under M4-M7 remains active.

Outstanding M0 decisions: persistence schema probe, project license choice
(asked asynchronously; no license selected yet), broader malformed-image
fixtures and numerical QuPath comparison tolerances. GEOS dependency selection
and packaging are now recorded with source identity and independent tests.

# qupathxx

A new C++17 bioimage analysis application with nanobind Python bindings and a
Dear ImGui desktop interface. Inspired by QuPath's workflows and the native-core
architecture in `../naparixx`. This is an initial implementation, not a complete
QuPath port or a compatible replacement for Java QuPath.

## Implemented

- Native tissue microarray grids support undoable core edits, missing flags,
  case IDs, project persistence, per-core analysis and CSV exports through
  Python and ImGui; see [tissue microarrays](docs/tissue-microarrays.md).

- Docked desktop launchers place project/objects, image/display and measurements
  around the slide canvas, with separate layout preferences. Embedded callers
  can opt in; see the [docked workspace](docs/docked-workspace.md).

- Monitor-aware fonts and controls support live UI size changes while preserving
  the slide camera; fields and buttons adapt to narrow panes. See
  [UI scaling](docs/ui-scaling.md) and the [responsive workspace](docs/responsive-workspace.md).

- Complete bounded thumbnails are available in C++, Python and the native CLI.
  The asynchronous ImGui overview rejects stale image results and exposes bounded
  source budgets through desktop controls, Python and CLI options; see the
  [thumbnail policy and measurements](docs/thumbnails.md).
- Linux headless candidates can bundle GEOS and OpenSlide with verified relocated
  Python/native CLI loading; see [runtime packaging](docs/linux-runtime-bundle.md).

- Native resize preserves the view center and overlay pixels. Minimized windows
  suspend drawing while managed commands, saves and autosave continue; see
  [window lifecycle and measurements](docs/window-lifecycle.md).

- Automated native and installed-wheel checks cover Windows headless/GUI and
  Linux headless/minimal/sanitizer configurations. Development artifacts and
  their limits are described in [CI and distribution](docs/ci-distribution.md).
  Accepted Gaussian/calibrated-cell revisions also have audited headless
  [manylinux builder and fresh-consumer evidence](docs/manylinux-qualification.md#current-accepted-gaussian-and-calibrated-cell-engine).

- Pinned QuPath runtime references check geometry, sampling, OD/stains and a real
  native/QuPath round trip. Quantization and the explicit loss-aware thin-ellipse matching
  policy are recorded; see [QuPath comparisons](docs/qupath-comparisons.md).
  A separate [cell-plugin corpus](docs/qupath-cell-comparisons.md) records
  segmentation and feature differences, including unresolved acceptance gates.

- Managed live viewers accept typed commands from Python producer threads, with
  owner-side edits, background analysis, notifications, saves and bounded queues.
  See [live viewer sessions](docs/live-viewer.md) and the
  [live workflow example](examples/live_workflow.py).

- NumPy region/float/label views retain native ownership; RGB/RGBA imports copy
  strided uint8 inputs into owned native buffers. See
  [NumPy exchange](docs/numpy-arrays.md) and the
  [complete region workflow example](examples/region_workflow.py). Public typed
  declarations ship with the wheel; see [Python API](docs/python-api.md).

- Native headless project batches run ROI, tissue and cell workflows with per-image
  checkpoints, verified restart, failure policies and embedded CSV. C++, Python
  jobs and the packaged CLI share the implementation; see
  [project batch processing](docs/project-batch.md).

- Native ROI stain estimation uses bounded deterministic sampling, reusable model
  identities, Python jobs and ImGui estimate/apply controls. See
  [stain estimation](docs/stain-estimation.md).

- Native random forests train through an optional pinned OpenCV backend and
  expose portable model bytes, strict feature schemas and tree votes to Python.
  The [object workflow](docs/object-classifiers.md) adds feature snapshots,
  background jobs, undoable labels and ImGui train/preview/apply/model-file
  controls, retained cohorts/application audits, and project/recovery persistence.
  The [pixel buffer API](docs/pixel-classifiers.md) adds multiscale features,
  portable resolution-bound recipes, labelled-ROI worker training and bounded
  source-aware label/vote masks and exact, undoable class contour publication.
  The [F5 pixel panel](docs/pixel-classifier-ui.md)
  adds worker training, model files, bounded class-colored overlays and contour
  publication. [Native pixel artifacts](docs/pixel-classifier-archives.md) retain
  training cohorts and prediction masks in projects/recovery. F5 keeps and
  browses records, reuses their models and previews historical masks without
  restoring live publication authority. Native/Python and F5
  [prediction windows](docs/pixel-prediction-windows.md) bound masks to a requested
  or visible natural-grid region while preserving feature context.
  A [native/Python block producer](docs/pixel-stream.md) consumes full targets
  with bounded output storage. [QPS1 files](docs/pixel-stream-file.md) store complete
  streams atomically with indexed, verified historical access. The native/Python
  [background export job](docs/pixel-stream-export.md) also powers F5 stream export
  with progress and cancellation. A separate [saved-file inspector](docs/pixel-stream-browser.md)
  opens, verifies and previews individual historical blocks without changing the current image.
  The standalone [pixel commands](docs/pixel-stream-cli.md) export from saved projects
  and verify QPS1 files through `qupathxx_batch`.
  [Schema-10 project links](docs/pixel-stream-records.md) retain bounded external-file
  descriptors, with explicit verified reopen and relinking in the inspector.
  [Hash-only input scans](docs/pixel-input-digests.md) compare captured source
  inputs with verified streams without running forest inference.
  [Resumable project pixel batches](docs/project-pixel-batches.md) export pinned
  models across project targets and reuse fully verified image bundles.
  Research validation and large-batch
  frame budgets remain unfinished;
  see also the
  [classifier engine contract](docs/classifier-engine.md).

- Cached reader generations and open-time file observations persist in analysis
  provenance; Python and ImGui compare them with native file fingerprints through
  atomic association exports. See [reader source association](docs/reader-source-association.md).

- Project-wide ROI means and tissue component CSV use immutable native snapshots,
  historical calibration and background atomic export in Python and ImGui; see
  [measurement tables](docs/measurement-tables.md).

- Native file SHA-256 observations provide bounded streaming, progress/cancellation,
  stable metadata checks and atomic JSON through Python and ImGui; see
  [file fingerprints](docs/file-fingerprints.md).

- Cell tables join live names, classes and geometry to historical measurements,
  including explicit missing values; see [joined cell tables](docs/cell-object-tables.md).

- Capture-time source, calibration and native build provenance survives ROI,
  tissue and cell archives, project/recovery storage and rechecks; see
  [capture provenance](docs/analysis-capture-provenance.md).

- Published cell objects retain saved feature identity through edits, Undo/Redo,
  project/recovery storage and the measurement inspector; see [cell feature links](docs/cell-feature-links.md).

- Project-wide cell measurement CSV preserves project/image identity across
  archived runs; see [project cell tables](docs/project-cell-tables.md).
- Native cell measurement CSV with immutable capture, background atomic export,
  Python and ImGui controls; see [cell tables](docs/cell-tables.md).
- Capture-time cell calibration provides sampled areas in square micrometers and
  centroid coordinates in micrometers, including opt-in CSV v2 and native project
  exports; see [calibrated cell features](docs/calibrated-cell-features.md).
  Inclusive [physical nucleus-area filters](docs/cell-area-limits.md) are available
  through native/Python, CLI, batch and desktop paths; engineering validation is
  recorded separately from segmentation acceptance.
- Lossless cell-run archives with parameters, markers, compartment features,
  project/recovery storage and pixel rechecking; see [cell archives](docs/cell-archive.md).
- Bounded GPU canvas caching reuses unchanged image/annotation pixels while
  keeping picking and edit tools live; see [canvas cache](docs/overlay-cache.md).
- Native marker watershed cell detection, nucleus/body/cytoplasm features,
  nanobind jobs and ImGui previews with atomic cell publication and Undo/Redo;
  see [cell detection](docs/cell-detection.md).
- Standalone C++ document owns an image-server reference, immutable ROI annotations and calibration.
- Dense RGB and optional OpenSlide servers with 64-bit metadata and bounded RGBA region reads.
- Stable annotation IDs, deletion, calibrated area and mean RGB intensity.
- Atomic general-ROI object edits, parent/class assignments, subtree deletion,
  bounded undo/redo and document/object revisions shared by Python and the GUI.
- Image viewport with cursor-anchored zoom, pan, fit, rectangle/ellipse/polygon/line/point drawing.
- ImGui image canvas, annotation inspector and selected-object measurements.
- Click/drag overview navigation, RGB/grayscale/channel display, contrast window and gamma.
- Python bindings use the same document as the GUI; annotation lists are snapshots.
- Synthetic demonstration image and binary P6 PPM reader (8-bit RGB, maxval 255).
- Native `qupathxx_region` CLI writes extracted regions as RGBA PAM files.
- Native background tile scheduler with priorities, shared tickets, cancellation,
  generation checks, bounded work reservations and a decoded LRU cache.
- Atomic native GeoJSON import/export and measurement CSV, shared by Python and
  ImGui, with explicit conversion-loss reports; see [interchange](docs/interchange.md).
- In-window project image browsing, add/rename/relink controls and independent
  views/selections/history; see [project browser](docs/project-browser.md).
- Persistent per-image camera/display views with transactional schema-1 migration;
  see [workspace storage](docs/project-workspace.md).
- Empty project workspaces, New project, confirmed image removal and `show(project)`;
  see [project lifecycle](docs/project-lifecycle.md).
- Background project-file opening with Save/Discard/Cancel workspace replacement;
  see [opening projects](docs/project-opening.md).
- Background explicit saves preserve newer live edits as unsaved and recover interrupted
  SQLite transactions; see [project saving](docs/project-save.md).
- Recovery association after moves/copies/renames and bounded native workspace retirement;
  see [association](docs/recovery-association.md) and [retirement](docs/project-retirement.md).
- Background recovery loading and reviewed checkpoint deletion; see
  [recovery maintenance](docs/recovery-maintenance.md).
- Abandoned autosave work cleanup across sessions, with committed snapshot rescue; see
  [recovery work cleanup](docs/recovery-work-cleanup.md).
- Opt-in age/count checkpoint retention with idle-session protection; see
  [retention](docs/recovery-retention.md).
- Recovery publication/progress reporting and safe maintenance cancellation;
  see [maintenance progress](docs/recovery-maintenance-progress.md).
- Bounded native ownership-marker prefetch for large recovery catalogs;
  see [marker prefetch](docs/recovery-marker-prefetch.md).
- Deferred initial project/recovery/image opening with status, counts and cancellation; see
  [project opening](docs/project-opening.md).
- Bounded revision-keyed shape measurements with anisotropic calibration; see
  [shape measurements](docs/geometry-spatial.md#revision-keyed-shape-measurements).
- Native optical density and fixed-vector stain separation with Python access;
  see [stain transforms](docs/stain-transforms.md).
- Background ROI optical-density/stain means with cancellation and stale-result
  checks in C++, Python and ImGui; see [analysis jobs](docs/analysis-jobs.md).
- Retained analysis records with ROI input digests, schema-3 project/recovery
  storage and background rechecks; see [analysis provenance](docs/analysis-provenance.md).
- Shared immutable archive snapshots preserve save baselines with bounded copying
  during live edits; see [snapshot sharing](docs/analysis-snapshot-sharing.md).
- Bounded native ROI tissue thresholding and connected components, with Python
  access and an incremental ImGui mask preview; see [tissue masks](docs/tissue-detection.md).
- Automatic budget-based or explicit native pyramid resolution for tissue masks,
  with level-zero component geometry and level-aware input digests.
- Native square opening/closing and bounded hole filling before tissue component
  filtering, with ROI/opacity exclusion and cleanup statistics.
- Optional readiness waits for native GUI replay and bounded completion of tile
  uploads before captures; see [GUI replay](docs/gui-replay.md).
- Native background autosave, session-specific recovery files and dirty-close
  decisions; see [recovery](docs/project-recovery.md).

The GUI displays dense and OpenSlide images through asynchronous 256-pixel tiles,
coarse pyramid fallback and a 64 MiB GPU payload cache. OpenSlide regions can also
be read from Python and the native CLI. Calibration stays unknown until read from metadata or explicitly set;
physical area raises an error when either axis is unknown. Mean intensity is the
unweighted mean of straight RGB bytes over nontransparent pixels, not optical
density or stain-specific quantification.

## Build and launch

Requires Python 3.11+, CMake 3.24+ and a C++17 compiler. On Windows install Visual
Studio's Desktop development with C++ workload. GUI builds fetch Dear ImGui
v1.91.9b and GLFW 3.4 and require desktop OpenGL. Linux also needs GLFW's X11 or
Wayland development dependencies; macOS needs Xcode command line tools.

```powershell
uv venv --python 3.12
uv pip install -e ".[test]" --config-setting "cmake.define.QUPATHXX_BUILD_GUI=ON" --config-setting "cmake.define.QUPATHXX_BUILD_TESTS=ON"
.venv/Scripts/python -m qupathxx
# Or open an image:
.venv/Scripts/python -m qupathxx sample.ppm
```

Omit the GUI config setting for a headless Python package with no GLFW/ImGui
downloads. On Unix use `.venv/bin/python` instead. In the GUI, enable Rectangle
tool and drag with the left mouse button. Right drag pans, the wheel zooms,
and Fit image resets the view. Disable Rectangle tool to select by clicking.

```python
import qupathxx as qpx

doc = qpx.Document(qpx.Image.demo())
doc.set_calibration(0.25, 0.25)
region = doc.add_rectangle(qpx.Rect(100, 100, 200, 150), "Tumor region")
print(doc.area_um2(region), doc.mean_intensity(region))
qpx.show(doc)  # Blocks; run on the main thread. Requires a GUI-enabled build.
print(len(doc.annotations))  # Includes annotations drawn in the UI.
```

`Image(width, height, rgb)` accepts a flat sequence of RGB bytes and copies into
native storage. Read-only NumPy views retain native pixel ownership; writable
exports use explicit copies. Documents have a single owner. `show(Document)` and
`show(Project)` retain the Python GIL. For live Python producers, use
`show(ViewerSession)` on the main thread: it releases the GIL and routes queued
commands to the native owner. See [live viewer sessions](docs/live-viewer.md).

Standalone C++ build, with no Python dependency:

```powershell
cmake -S . -B build/desktop -DQUPATHXX_BUILD_PYTHON=OFF -DQUPATHXX_BUILD_GUI=ON -DQUPATHXX_BUILD_TESTS=ON
cmake --build build/desktop --config Release
ctest --test-dir build/desktop -C Release --output-on-failure
build/desktop/Release/qupathxx_app.exe
```

Python checks: `.venv/Scripts/python -m pytest`.
The native executable accepts `--smoke` for a finite startup/render/shutdown check.

The [CI matrix](docs/ci-distribution.md) checks native configurations, fresh
installed wheels, typing and software-rendered desktop workflows on hosted
Windows/Linux runners. ImGui input replay covers navigation, editing, analysis,
project storage/recovery and managed Python producers. Actual OS pointer delivery,
hardware GPU performance and monitor DPI transitions remain open.

## OpenSlide region reading

Install an [OpenSlide SDK](https://openslide.org/download/) and enable the
optional reader. The Windows validation SDK is openslide-bin 4.0.1.2; its
identity and the public slide fixture are recorded in
[the fixture manifest](tests/fixtures/manifest.json).

```powershell
uv pip install -e ".[test-readers]" --config-setting "cmake.define.QUPATHXX_BUILD_OPENSLIDE=ON" --config-setting "cmake.define.OpenSlide_ROOT=C:/path/to/openslide-sdk"
.venv/Scripts/python tools/fetch_reader_fixtures.py
$env:QUPATHXX_APERIO_FIXTURE = (Resolve-Path build/fixtures/CMU-1-Small-Region.svs).Path
.venv/Scripts/python -m pytest -q
```

On Windows, CMake copies the SDK DLL beside the binaries and installs it with
the package, together with the SDK's license directory when present. On Unix,
the OpenSlide shared library must be discoverable by the system loader.
OpenSlide is optional and disabled by default on a fresh configuration.

```python
import qupathxx as qpx

slide = qpx.open_slide("sample.svs")
print(slide.metadata.width, slide.metadata.levels[0].downsample)
region = slide.read_region(qpx.RegionRequest(1000, 2000, 0, 256, 256))
rgba_bytes = region.data  # Owned Python bytes, no NumPy dependency.
doc = qpx.Document(slide)  # Retains the server and copies known calibration.
slide.close()  # Idempotent; waits for reads. Future reads through doc also fail.
```

Coordinates are always in level zero; width/height count output pixels at the
requested pyramid level. Out-of-image pixels are transparent black. The output
limit is 256 MiB per request; OpenSlide conversion uses an additional temporary
buffer. Read buffers survive server closure. Open/read/close release the Python
GIL and read/close operations serialize per server.

The installed package includes the native CLI beside `_native` (a standalone
CMake build places it in the executable output directory):

```powershell
build/cp312-cp312-win_amd64/Release/qupathxx_region.exe sample.svs 1000 2000 0 256 256 region.pam
build/cp312-cp312-win_amd64/Release/qupathxx_region.exe --thumbnail sample.svs 250 150 thumbnail.pam
build/cp312-cp312-win_amd64/Release/qupathxx_region.exe --thumbnail sample.svs 250 150 thumbnail.pam --source-pixels 8388608
```

See the [image-server contract](docs/image-server-contract.md) for pixel,
coordinate, ownership and current validation boundaries.
The [tile-scheduler contract](docs/tile-scheduler-contract.md) describes the
background-read API, budget accounting and a Python example. The
[tiled viewer contract](docs/tiled-viewer.md) records rendering and measured scope.
Open a slide with `.venv/Scripts/python -m qupathxx sample.svs`.
Mean intensity is calculated in a cancellable background task when requested
with the measurement button, rather than on each frame.
The [document command contract](docs/document-commands.md) describes atomic
edits, hierarchy, undo/redo, retention limits and the large-object benchmark.
The inspector applies rectangle/name/class/parent changes as one operation.
Use toolbar Undo/Redo or Ctrl+Z/Ctrl+Y; Delete removes the selected subtree.

## Architecture and next milestones

See the [project plan](docs/project-plan.md) for release scope, architecture,
milestone acceptance criteria and the next implementation packet.

`cpp/core.cpp` and `cpp/include/qupathxx/core.hpp` have no Python or windowing
dependencies. `cpp/image_server.cpp` adapts dense images and
`cpp/openslide_server.cpp` implements the optional slide reader.
`cpp/bindings.cpp` exposes model and reader operations through nanobind.
`cpp/gui.cpp` presents and edits that model using Dear ImGui, GLFW and OpenGL.
`src/qupathxx` provides packaging and the CLI. This follows naparixx's separation
of native state, bindings and frontend while implementing a new ImGui frontend.
No naparixx or QuPath source has been copied into this project.

1. Extend the RGB tiled viewer with docking, fluorescence/ICC display, real
   vendor navigation benchmarks, resize/minimize and per-monitor DPI validation.
2. Broaden complex-geometry editing/performance fixtures and compare hierarchy,
   class, shape and interchange behavior with pinned upstream workflows.
3. Add explicit QuPath project import compatibility checks. `.qpproj` and
   `.qpdata` remain unsupported; native SQLite projects are implemented.
4. Validate stain/tissue/cell workflows against a pinned QuPath runtime and add
   the remaining physical filters and processing options.
5. Broaden live-viewer command coverage and add versioned model integrations;
   managed queues/notifications, NumPy exchange, jobs, batches and stubs are implemented.
6. Add classifiers, tissue microarrays, broader plugin/distributed batch workflows
   and the clean-machine/platform distribution gates in the project plan.

Groovy scripts, Java extensions, Bio-Formats and existing QuPath projects will
require dedicated compatibility work. Full parity needs an upstream feature
inventory and behavioral fixtures; the current tests validate this native
foundation, not equivalence with QuPath.

References: [QuPath features](https://qupath.readthedocs.io/en/stable/docs/intro/about.html),
[nanobind builds](https://nanobind.readthedocs.io/en/latest/building.html),
[Dear ImGui](https://github.com/ocornut/imgui), [GLFW](https://www.glfw.org/).
Downloaded dependencies retain their upstream licenses. A project license has
not yet been selected.

The [geometry and spatial-query contract](docs/geometry-spatial.md) covers optional
GEOS setup, immutable shapes, hole masks, index behavior and benchmark scope.

[Detection and cell objects](docs/object-kinds.md) support independent nucleus
geometry, undoable edits, compartment measurements and nucleus overlays.

The [class registry](docs/classes.md) provides shared colors, atomic renaming,
assignment counts and batch classification through Python and the inspector.

[Multi-selection](docs/selection.md) supports canvas/list gestures, a primary
inspector object, batch classification and atomic subtree deletion.

[Polygon editing](docs/geometry-editing.md) provides vertex handles, hole drawing
and undoable Boolean area operations through the native model and Python.

[Native project storage](docs/project-storage.md) saves multi-image SQLite
projects with exact ROI/object state, relative links, relinking, stale-writer
checks and transactional rollback. Open with `--project study.qpxproj`; the
viewer includes a project path and Ctrl+S. SQLite is statically built by default;
set `QUPATHXX_BUILD_PROJECT=OFF` to omit storage and its source download.

[Tissue contours](docs/tissue-contours.md) convert bounded native masks into polygons
with holes and multipart components, then publish one undoable annotation batch
through Python or the ImGui inspector.

[Tissue archives](docs/tissue-archive.md) preserve lossless masks, parameters and
input digests in native projects and recovery snapshots, with explicit session
rechecking in Python and ImGui.

[Immutable object snapshots](docs/object-snapshots.md) share captured object
blocks with save/recovery workers and preserve isolation during live edits.

The optional [quantitative TIFF/OME reader](docs/quantitative-tiff.md) preserves
stored sample types and channel/Z/time axes separately from RGBA previews. Build
with `QUPATHXX_BUILD_TIFF=ON`; open its native channel panel with F2.


[Saved raw measurements](docs/raw-analysis-records.md) retain original channel
statistics, exact sample hashes and object/TMA context. The native, Python and
ImGui workflows support replay verification, historical CSV export and captured
raw project batches with hash-only checkpoint reuse.

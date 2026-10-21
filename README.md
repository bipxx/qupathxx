# qupathxx

A new C++17 bioimage analysis application with nanobind Python bindings and a
Dear ImGui desktop interface. Inspired by QuPath's workflows and the native-core
architecture in `../naparixx`. This is an initial implementation, not a complete
QuPath port or a compatible replacement for Java QuPath.

## Implemented

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
native storage. The initial API has no NumPy zero-copy interface. Documents are
single-threaded; `show` retains the Python GIL to prevent concurrent Python
mutation. It does not provide a live Python console or background processing.

Standalone C++ build, with no Python dependency:

```powershell
cmake -S . -B build/desktop -DQUPATHXX_BUILD_PYTHON=OFF -DQUPATHXX_BUILD_GUI=ON -DQUPATHXX_BUILD_TESTS=ON
cmake --build build/desktop --config Release
ctest --test-dir build/desktop -C Release --output-on-failure
build/desktop/Release/qupathxx_app.exe
```

Python checks: `.venv/Scripts/python -m pytest`.
The native executable accepts `--smoke` to render three frames and exit.

Validated on Windows with MSVC 19.44 and Python 3.12: 129 Python tests including
transaction, hierarchy, pyramid, Aperio and opt-in framebuffer checks, eleven native suites with
and without OpenSlide/GEOS, and GUI startup/render/shutdown with known and unknown
calibration. ImGui input replay verifies zoom, pan, shape drawing/history and overview
navigation; actual OS pointer delivery and monitor DPI transitions remain open.
The Python tests also pass from a wheel installed into a fresh virtual
environment, using its packaged OpenSlide/GEOS DLLs and native CLI.

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
2. Extend rectangular hierarchy/classes and undo/redo with robust polygon/ellipse
   ROIs, detection/cell objects, multi-selection, spatial indexing and interchange.
3. Project persistence with versioned native schema and explicit QuPath import
   compatibility checks. `.qpproj` and `.qpdata` are not supported yet.
4. Color deconvolution, tissue detection and watershed cell detection with
   reproducible parameters and comparisons against a pinned QuPath release.
5. NumPy array exchange, cancellable jobs and Python analysis integrations.
6. Classification, whole-slide batch processing, plugin APIs and distribution.

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

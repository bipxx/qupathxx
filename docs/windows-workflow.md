# Windows research workflow candidate

This engineering candidate opens an Aperio brightfield slide, authors a
bounded calibrated rectangle, retains tissue and region-intensity measurements,
exports CSV and saves a project that reopens in ImGui or Python. The native code
is MODEL1 source `6219a08f`; the package adds the owner's AGPL-3.0-only decision.
It does not include the parked PRESET1 implementation or imply full QuPath parity.

## Files and prerequisites

Download the [Windows prerelease candidate](https://github.com/bipxx/qupathxx/releases/tag/v0.1.0-windows-candidate.1).
Choose the native ZIP for standalone use, or the wheel for Python3.12 x64.
Application and dependency source archives, notices and validation evidence are
provided alongside the binaries. The optional CMU-1.svs asset mirrors the pinned
CC0 fixture; its recorded hash is unchanged.

In this workspace the final files are in
`C:\Users\HEQ\Projects\qupathxx\build\windows-workflow-checkpoint\delivery-agpl`:

- `qupathxx-windows-x64-candidate.zip`: standalone ImGui app and native CLIs,
  six EXE/DLL files, fonts, dependency notices, project license and run notes.
- `qupathxx-0.1.0-1-cp312-cp312-win_amd64.whl`: licensed Python 3.12 x64 package.
- `qupathxx-source-6219a08f.zip`: matching native-source snapshot with the
  project license and source notes. All495 recorded compiler-input files match
  their original raw-byte hashes; later PRESET1 code is excluded.
- `qupathxx-dependency-sources.zip` and `dependency-sources.json`:12 original
  source archives with checksums and upstream origins, including OpenSlide's
  complete source/build distribution and nanobind's bundled robin-map.
- `candidate.json`, `wheel-license-overlay.json`, runtime manifests and
  `installation.json`: identity, package changes, dependency closure and install
  evidence. The native compiler identity is unchanged by the license overlay.

Use Windows x64 with an OpenGL3.2-capable desktop/driver and the Microsoft Visual
C++ x64 runtime required by the manifest (`concrt140`, `msvcp140`, `msvcp140_2`,
`vcruntime140`, `vcruntime140_1`). The wheel also requires CPython 3.12 x64;
the standalone app does not require Python. The example requires the `arrays`
extra; this run resolved NumPy2.5.4. No proprietary reader SDK is required.

The reproducible slide is [CMU-1.svs](https://openslide.cs.cmu.edu/download/openslide-testdata/Aperio/CMU-1.svs),
listed as CC0-1.0 in [the pinned fixture manifest](../tests/fixtures/manifest.json).
Its SHA256 is `00a3d54482cd707abf254fe69dccc8d06b8ff757a1663f1290c23418c480eb30`.
The existing local copy is `build/fixtures/CMU-1.svs` relative to the main workspace.
Keep the slide available at the same path when reopening its linked project.

## Python: analyze, export and reopen

From the active source checkout, use a new environment and an empty output folder:

```powershell
$delivery = 'C:\Users\HEQ\Projects\qupathxx\build\windows-workflow-checkpoint\delivery-agpl'
py -3.12 -m venv .venv
.\.venv\Scripts\python.exe -m pip install "$delivery\qupathxx-0.1.0-1-cp312-cp312-win_amd64.whl[arrays]"
.\.venv\Scripts\python.exe -I examples\region_workflow.py C:\data\CMU-1.svs C:\data\study-output --roi 29185 2048 512 512 --show
```

The example preserves existing outputs. ROI coordinates are level-zero pixels;
rectangles must lie within the image and contain at most1,048,576 pixels. A region
at the slide origin is often background; this chosen ROI contains tissue.

Outputs are `study.qpxproj`, `measurements.csv` (one retained ROI row), `tissue.csv`
(one row per retained component) and `workflow.json`. The summary includes actual
native source identity, calibration, ROI, record IDs and replay results. This
fixture reports0.499 micrometers/pixel in both axes,262,144 samples and246,394
foreground samples across11 components with the unchanged default recipe.
The example independently checks RGB moments and verifies native saved-result
replay. These are engineering checks, not a labeled biological accuracy study.

## Standalone ImGui

Extract the entire native ZIP into a new folder. Keep its DLLs, `fonts` and
`licenses` next to the EXEs. Open a slide or the Python-produced project:

```powershell
.\qupathxx_app.exe C:\data\CMU-1.svs
.\qupathxx_app.exe --project C:\data\study-output\study.qpxproj
```

For authoring, navigate to tissue, select the rectangle tool (`R`) and draw a
bounded ROI in the slide canvas. Select it, enable **Intensity statistics** in
the analysis controls and run analysis (`Ctrl+M`). Keep the result (`Ctrl+K`).
Use **Detect tissue** (`Ctrl+D`), then **Keep tissue mask** (`Ctrl+U`) when ready.
The measurement-table controls export retained ROI/component tables to CSV.
Save the project (`Ctrl+S`) and reopen it; linked slide pixels remain external.
Use the same recipe for comparisons. Display changes do not change measurements.

The [installed desktop probe](benchmarks/windows-workflow-checkpoint/installed_desktop.py)
exercises real ImGui input, retained results, CSV, save/reopen and standalone
startup. It uses the installed package, not the source Python module. Its report
and capture are under `build/windows-workflow-checkpoint/desktop-viewport`.

## License, dependencies and removal

QuPathxx's own code is **AGPL-3.0-only**, selected by the owner; see [LICENSE](../LICENSE).
Dependencies retain their own notices. Both artifacts preserve the installed
notice trees for GEOS, GLFW, Dear ImGui/STB/GL loader/Proggy fonts, ImageJ shape
code, libtiff, nanobind, nlohmann-json, Noto CJK/emoji fonts, OpenCV, the OpenSlide
SDK and its dependencies, robin-map, SQLite, TIFF JPEG/zlib and tinyxml2. The
Python package additionally relies on the separately installed NumPy distribution
and its notices. See [third-party notices](../third_party/README.md).

The static manifests inventory every shipped file and PE dependency. They show
that native executables do not depend on Python; OS/driver/runtime availability
is demonstrated only on the tested host. The source revision, original wheel
digest, unchanged members and license-overlay members are recorded separately.
The [dependency source inventory](benchmarks/windows-workflow-checkpoint/dependency-sources.json)
maps the shipped native libraries to pinned source archives. OpenSlide's source
distribution includes its16 library projects, patches and build system. GEOS,
OpenCV, SQLite, JSON, TIFF/JPEG/zlib/XML, ImGui, GLFW and nanobind/robin-map sources
are supplied alongside the matching application source. Build recipes remain
in that application snapshot; unmodified font assets retain their original
notices and pinned source URLs. NumPy and the OS/compiler/graphics runtimes are
separately installed prerequisites, not bundled application sources.

[Fresh Windows deployment run42099947943](https://github.com/bipxx/qupathxx/actions/runs/42099947943)
passes install, real-slide Python/ImGui authoring/analysis/export/save/replay,
standalone reopening, and wheel/native uninstall. PATH was restricted to Windows
system directories; mapped GEOS/OpenSlide libraries came from the installed
package. Uninstall preserved seven project/CSV/summary files byte-for-byte.
The fresh Windows Server2022 image was20261101.353.1 with declared MSVC runtimes
and pinned test-only Mesa. Mesa was added only to disposable test folders, then
removed; it is absent from shipped artifacts. This is an explicit fresh-host
deployment result, not a claim about every consumer GPU or Windows configuration.

To remove the Python package, run `python -m pip uninstall qupathxx` in its
environment; a dedicated environment may instead be deleted after closing its
processes. To remove the standalone app, close it and delete its extracted folder.
Neither action should delete separately stored slides, CSVs or projects. Keep
those outside the application/environment directory.

## Support boundary

Verified: local Windows11 Pro10.0.26200 x64 plus the fresh hosted configuration
above, the declared packages, one RGB8 calibrated Aperio
ROI, native tissue/statistics, saved replay, CSV and both installed UI routes.
The separate baseline retains its original tests; this checkpoint does not
upgrade them into acceptance of changed code or new platforms. No claim covers
arbitrary vendor inputs, whole-slide budgets, dense-scene performance, biological
parity, fluorescence/ICC behavior, Linux/macOS GUI, external model adapters or
speed superiority. Those remain in the [existing queue](api-delivery-plan.md).

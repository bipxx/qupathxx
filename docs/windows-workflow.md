# Windows research workflow candidate

This local engineering candidate opens an Aperio brightfield slide, authors a
bounded calibrated rectangle, retains tissue and region-intensity measurements,
exports CSV and saves a project that reopens in ImGui or Python. The native code
is MODEL1 source `6219a08f`; the package adds the owner's AGPL-3.0-only decision.
It does not include the parked PRESET1 implementation or imply full QuPath parity.

## Files and prerequisites

In this workspace the final files are in
`C:\Users\HEQ\Projects\qupathxx\build\windows-workflow-checkpoint\delivery-agpl`:

- `qupathxx-windows-x64-candidate.zip`: standalone ImGui app and native CLIs,
  six EXE/DLL files, fonts, dependency notices, project license and run notes.
- `qupathxx-0.1.0-1-cp312-cp312-win_amd64.whl`: licensed Python 3.12 x64 package.
- `qupathxx-source-6219a08f.zip`: matching native-source snapshot with the
  project license and source notes. All495 recorded compiler-input files match
  their original raw-byte hashes; later PRESET1 code is excluded.
- `candidate.json`, `wheel-license-overlay.json`, runtime manifests and
  `installation.json`: identity, package changes, dependency closure and install
  evidence. The native compiler identity is unchanged by the license overlay.

Use Windows x64 with an OpenGL-capable desktop/driver and the Microsoft Visual
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
Distribution/source-availability review and a genuinely clean-machine release
walkthrough remain R13 work before claiming a public supported release.

To remove the Python package, run `python -m pip uninstall qupathxx` in its
environment; a dedicated environment may instead be deleted after closing its
processes. To remove the standalone app, close it and delete its extracted folder.
Neither action should delete separately stored slides, CSVs or projects. Keep
those outside the application/environment directory.

## Support boundary

Verified: this Windows host, the declared packages, one RGB8 calibrated Aperio
ROI, native tissue/statistics, saved replay, CSV and both installed UI routes.
The separate baseline retains its original tests; this checkpoint does not
upgrade them into acceptance of changed code or new platforms. No claim covers
arbitrary vendor inputs, whole-slide budgets, dense-scene performance, biological
parity, fluorescence/ICC behavior, Linux/macOS GUI, external model adapters or
speed superiority. Those remain in the [existing queue](api-delivery-plan.md).

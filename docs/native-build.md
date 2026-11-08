# Build and use the C++ application

Native products need CMake 3.24+, a C++17 compiler and a platform SDK. On Windows,
use Visual Studio's Desktop development with C++ workload; on macOS, Xcode command
line tools. Python and nanobind are required only when explicitly enabling bindings.
First configuration downloads checksum-pinned open dependencies, so network access
is needed unless their CMake FetchContent sources have already been supplied.

## Smallest project-analysis example

From a standalone source checkout:

```sh
cmake -S . -B build/native -DCMAKE_BUILD_TYPE=Release -DQUPATHXX_BUILD_EXAMPLES=ON -DQUPATHXX_BUILD_TESTS=ON
cmake --build build/native --config Release --target qupathxx_project_example qupathxx_application_entry_tests
ctest --test-dir build/native -C Release --output-on-failure --no-tests=error -R '^(native_project_example|application_entry)$'
```

On Windows with Visual Studio, run `build/native/Release/qupathxx_project_example.exe`;
with a single-configuration generator, run `build/native/qupathxx_project_example`.
For example:

```sh
build/native/qupathxx_project_example --demo input.qpx
build/native/qupathxx_project_example input.qpx analysis.qpx
```

The first command saves two embedded RGB pixels using native project APIs. The
second opens the first image, creates a level-zero ROI up to512×512 pixels, computes
optical-density means, retains the analysis and saves a new project. It reopens
the output and verifies the retained result by native replay. Both commands refuse
existing output paths. They do not alter the input project or infer calibration.
The fixed transform in the example is illustrative; optical-density means do not
estimate a biological stain model. Quantitative multichannel TIFF needs the raw
analysis APIs instead of this RGB example.

[examples/native_project.cpp](../examples/native_project.cpp) is a C++ caller of
`Project`, `AnalysisRequest`, `analyze`, `keep_analysis`, `save_new` and `replay`.
Link another in-tree consumer to `qupathxx_core`; CMake propagates its include path,
C++ standard and native link dependencies. This source build does not provide an
installed `find_package(qupathxx)` SDK contract.

`cmake --install build/native --config Release --prefix path/to/new-stage` installs
the selected executables under `qupathxx/`, including the example when enabled.
For a selective build, build the always-installed CLI targets `qupathxx_batch` and
`qupathxx_region` and (with project storage enabled) `qupathxx_spatial_counts`
before installation. Default builds include them automatically.

## Choose native capabilities

| CMake option | Default | Dependency / supported use |
| --- | --- | --- |
| `QUPATHXX_BUILD_PROJECT` | ON | Pinned SQLite and JSON fetched by CMake; projects, save/recovery and this example |
| `QUPATHXX_BUILD_INTERCHANGE` | ON | Pinned JSON; native GeoJSON interchange |
| `QUPATHXX_BUILD_EXAMPLES` | OFF | Builds this project example; requires project storage |
| `QUPATHXX_BUILD_TESTS` | OFF | Native CTest targets; build the selected tests before running them |
| `QUPATHXX_BUILD_GUI` | OFF | Pinned RmlUi/FreeType/ImGui/GLFW fetched by CMake; OpenGL 3.3 (macOS core 4.1) and Linux X11/Wayland development libraries; adds `qupathxx_app` with packaged [RML/RCSS resources](rmlui.md) |
| `QUPATHXX_BUILD_TIFF` | OFF | Pinned native TIFF/compression sources fetched by CMake; quantitative TIFF/OME-TIFF |
| `QUPATHXX_BUILD_OPENSLIDE` | OFF | OpenSlide SDK discoverable through `OpenSlide_ROOT`; supported whole-slide readers |
| `QUPATHXX_BUILD_GEOS` | OFF | GEOS ≥3.12 CMake package; robust topology and project-density workflows |
| `QUPATHXX_BUILD_CLASSIFIER_TRAINING` | OFF | Exact OpenCV4.11.0 SDK with core/ml; `OpenCV_DIR` points to its CMake config; native forest training |
| `QUPATHXX_BUILD_PYTHON` | OFF | Python ≥3.11 interpreter/development module and nanobind CMake package; optional interface only |

Native project/core services work without optional reader/training/geometry SDKs;
requests needing a disabled backend report that limitation. Enable only required
features. For the complete raw-classifier example, enable project, TIFF, GEOS
(for the batch workflow command) and classifier training. Configure SDK locations
with `-DGEOS_DIR=... -DOpenCV_DIR=...`; training is native, with no Python fallback.
All readers and required codecs must retain their open-license notices. See
[reader license policy](reader-license-policy.md) for the project's dependency records.

## Guard Python independence

[Native-only CI](../.github/workflows/native-only.yml) leaves the Python option at
its OFF default, disables Python/Python3/nanobind discovery and rejects binding
targets. It builds only the required native consumers/tests, exercises save/replay
and no-overwrite, installs them, checks installed files and Linux runtime imports,
then repeats the same small consumer check against the installed executable.
It also enables the existing native TIFF stack for the focused fluorescence and
spatial-count tests and exercises the installed spatial CLI. It does not run the
Python matrix or download scientific image fixtures.

This headless Linux guard complements the existing
[Windows GUI/native-only evidence](benchmarks/native-optional/README.md); it is
not a new release or a claim that every backend/platform is qualified.

# CI and development artifacts

[Build and installed workflows](https://github.com/bipxx/qupathxx/actions/workflows/ci.yml)
runs on main pushes, pull requests and manual dispatches. Changes confined to
`docs/` and `README.md` skip builds. Each job uploads logs,
JUnit results and available workflow reports, including on failure. Actions use
pinned revisions and read-only repository permissions. A green run accepts its
listed configurations and fixture scope; it does not accept every M0–M7 gate.

| Configuration | Checks |
| --- | --- |
| Windows 2022, Python 3.11, NumPy 1.26 | Full headless wheel, native and installed Python suites, public stubs and strict typing |
| Ubuntu 24.04, Python 3.12, NumPy 2 | Full headless wheel and the same installed-package checks |
| Windows 2022, Python 3.12, NumPy 2 | GUI wheel, native/Python suites including opt-in desktop replays, stubs, typing and standalone native executables |
| Ubuntu 24.04, minimal native | Python, GUI, GEOS, OpenSlide, interchange and project storage disabled |
| Ubuntu 24.04, native sanitizers | AddressSanitizer and UndefinedBehaviorSanitizer, geometry/interchange/storage enabled, Python/GUI/OpenSlide disabled |

GEOS is built from a checksum-pinned source archive. Cached SDK files are hashed
before reuse. Windows OpenSlide and test-only Mesa archives have pinned sizes
and SHA-256 identities in `tools/ci/runtime-assets.json`; extracted SDK files and
the selected Mesa DLLs are verified. Linux uses the runner's OpenSlide package.
The sanitizer job instruments this project's native code and embedded SQLite;
the shared GEOS SDK is not sanitizer-instrumented. These checks do not include
ThreadSanitizer or establish race freedom.

Enabled builds install notices for the bindings, robin-map, GLFW, Dear ImGui,
its embedded STB code, default font and GL loader, in addition to the core/runtime
notices. They come from the actual dependency sources or the pinned copies
documented in [third-party notices](../third_party/README.md). The installed-wheel
probe requires them; extraction markers missing from a new dependency stop
configuration for review.

The Windows GUI job uses Mesa llvmpipe. It copies the actual Python executable
and two verified renderer DLLs into an owned temporary directory, with explicit
paths to the test environment's standard library and installed packages. It also
adds the renderer to the disposable installed package for native CLI tests.
System OpenGL and the base interpreter are untouched. The workflow report checks
that the GUI really selected llvmpipe. This checks software rendering and replay
behavior, not hardware GPU performance, per-monitor DPI or vendor drivers.
The disposable runner uses a 1920x1080 desktop; the probe requires the requested
1440x900 framebuffer at 100% scale. This avoids Windows clipping the initial
window to its smaller default desktop and changing normalized replay geometry.

Each wheel is installed into a new virtual environment and imported in isolated
Python mode. A workflow probe verifies the loaded native binary against the
original wheel and checks bundled notices, analysis/tissue/cell retention,
project save/reopen, NumPy regions, CSV/GeoJSON export and native batch processing.
GUI builds also open the saved project. Full pytest and generated-stub/typing
checks then use that installed package. Negative typing checks require all seven
deliberately invalid calls to reject. The downloaded small Aperio fixture is
size/checksum-verified on every run, including cache hits. Downloads retry transient
transport failures at most three times; bad identity and permanent HTTP errors
fail immediately. External SDK/fixture inputs are checked before compilation.
Captured QuPath numerical references run by default; manual
dispatch can additionally regenerate live references in the Windows GUI job.

The Windows GUI artifact contains a wheel and a standalone candidate ZIP. The
ZIP is extracted from the original wheel's executables, runtime DLLs and notices;
it includes neither Python nor Mesa. A disposable copy runs the project viewer,
batch checkpoint workflow and exact-pixel region extraction with only Windows
system directories on `PATH`. Mesa is added only to that test copy. The candidate
also opens the Aperio slide fixture and compares extracted pixels with the
installed reader. It requires the Microsoft Visual C++ x64 runtime and OpenGL 3.2
for the desktop app.

Artifacts are development candidates, not published supported releases. The Linux
wheel uses the explicitly built GEOS SDK via `LD_LIBRARY_PATH` and the system
OpenSlide runtime; it is not an audited portable manylinux wheel. macOS wheels,
hardware/DPI campaigns, clean user-machine installers, project license selection
and the remaining workflow/format acceptance work are still open. No package is
automatically published to PyPI or a GitHub Release.

To reproduce a job, install `tools/ci/requirements.txt`, provide the platform
compiler/CMake, 7-Zip for Windows GUI mode and Linux OpenSlide development files
where appropriate, then run:

```sh
python -I tools/ci/run.py --mode headless --work build/ci/my-new-run
```

The work directory must not already exist. Other modes are `gui`, `minimal`,
`native` and `sanitize`; the GUI matrix configuration currently targets Windows.

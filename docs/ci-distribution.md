# CI and development artifacts

[Build and installed workflows](https://github.com/bipxx/qupathxx/actions/workflows/ci.yml)
runs on main pushes, pull requests and manual dispatches. Changes confined to
`docs/` and `README.md` skip builds. Each job uploads logs,
JUnit results and available workflow reports, including on failure. Actions use
pinned revisions and read-only repository permissions. A green run accepts its
listed configurations and fixture scope; it does not accept every M0–M7 gate.

Linux jobs use Ninja already available on the hosted runner. If it is absent,
the tool step installs the pinned `ninja==1.13.0` binary wheel, with bounded pip
transport retries and a three-minute step timeout. It does not refresh system
package indexes. The verified self-contained OpenSlide SDK supplies the reader
runtime; the prior package-index step could hold an otherwise minimal job
before any compilation or tests.

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
the selected Mesa DLLs are verified. Linux uses the pinned self-contained
OpenSlide SDK and bundles its reader ELF/notices with `$ORIGIN` resolution;
the installed probe verifies the mapped library and exact SDK hash. See
[reader runtime validation](reader-runtime.md).
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
Manual dispatch can also enable `extended_readers` for the pinned full Aperio
pyramid, damaged-tile checks and a per-level/cache benchmark. The ordinary smoke
fixture remains the default; see [extended reader validation](extended-reader-validation.md).

The Windows GUI artifact contains a wheel and a standalone candidate ZIP. The
ZIP is extracted from the original wheel's executables, runtime DLLs and notices;
it includes neither Python nor Mesa. A disposable copy runs the project viewer,
batch checkpoint workflow and exact-pixel region extraction with only Windows
system directories on `PATH`. Mesa is added only to that test copy. The candidate
also opens the Aperio slide fixture and compares extracted pixels with the
installed reader. Thumbnail gates check exact dense and complete-tile TIFF
pixels. The single-level Aperio thumbnail must reject its inadequate default
source budget without creating an output file. It requires the Microsoft Visual C++ x64 runtime and OpenGL 3.2
for the desktop app.

Artifacts are development candidates, not published supported releases. The Linux
headless candidate now [bundles GEOS and OpenSlide runtimes](linux-runtime-bundle.md),
with exact SDK hashes, installed process mappings and relocated Python/native CLI
checks after clearing `LD_LIBRARY_PATH`. Native build tests still use their SDK
paths. This is not an audited portable manylinux wheel. macOS wheels,
hardware/DPI campaigns, clean user-machine installers, project license selection
and the remaining workflow/format acceptance work are still open. No package is
automatically published to PyPI or a GitHub Release.

## Recorded validation

[Docked workspace run 39787239562](https://github.com/bipxx/qupathxx/actions/runs/39787239562)
has five completed successful jobs: Windows GUI passes 1292 Python tests and
34 native tests; Windows/Linux headless pass 1033/1027 Python tests and 33 native
tests each. Minimal and sanitizer configurations pass 33 each. Installed GUI
validation now opens the docked workspace and saves separate layout preferences;
the standalone candidate also saves and reloads a layout. Exact artifact hashes,
skips, local checks and the separately recorded pending aggregate run metadata
are in the [validation record](benchmarks/docking-ci-windows-linux.json).

[Thumbnail run 39773391727](https://github.com/bipxx/qupathxx/actions/runs/39773391727)
passes all five configurations. The Windows GUI suite passes 1273 Python tests
with six fixture/configuration skips, including thumbnail publication and
minimized-window activation visibility. Windows/Linux headless pass 1026/1019
with 253/260 skips. Native GUI passes 34 tests; all other configurations pass 33.
The [artifact record](benchmarks/thumbnail-ci-windows-linux.json) includes the
standalone dense/TIFF thumbnail success and default-budget Aperio rejection
checks. The [thumbnail policy](thumbnails.md) records the unresolved Linux
partial-tile TIFF edge case; these development candidates do not accept it.

[Run 39757130782](https://github.com/bipxx/qupathxx/actions/runs/39757130782)
passes all five jobs at source revision `f12a644246025820bf9846c9378d3edc83cff134`.
Windows GUI passes 1242 Python cases with five skips; Windows headless passes
1015 with 232 skips; Linux headless passes 1008 with 239 skips. GUI native tests
pass 33; each other native configuration passes 32, including ASan/UBSan. All
three wheels pass the installed workflow, stub regeneration and positive/negative
typing checks. The GUI reports llvmpipe and a 1440x900 framebuffer. Standalone
project, batch, dense-region and Aperio opening/extraction checks pass.

The [raw validation record](benchmarks/ci-windows-linux.json) retains run/job
links, source/native/wheel hashes, JUnit identities, skip reasons and the
standalone ZIP's complete file-hash inventory. Its 40 files include three native
executables, three runtime DLLs, 33 notices and a README; all payload files match
the tested wheel. Python and Mesa are absent. Earlier incomplete wheel artifacts
from the validation attempts were withdrawn.

A separate fresh local GUI wheel passes 1243 Python cases with four skips in
351.55 seconds, all 33 native tests, public stubs and strict typing. It uses
llvmpipe and also executes the live pinned QuPath regeneration and existing
generated large-pyramid cases. The four skips are two Windows symlink permission
cases, the GEOS-disabled-only case and the headless-only GUI contract. Hosted
GUI's five skips are two missing generated large-pyramid fixtures and the
GEOS-disabled, headless-only and opt-in live QuPath cases. These results do not
establish hardware GPU or frame-time performance budgets.

To reproduce a job, install `tools/ci/requirements.txt`, provide the platform
compiler/CMake, 7-Zip for Windows GUI mode and Linux OpenSlide development files
where appropriate, then run:

```sh
python -I tools/ci/run.py --mode headless --work build/ci/my-new-run
```

The work directory must not already exist. Other modes are `gui`, `minimal`,
`native` and `sanitize`; the GUI matrix configuration currently targets Windows.

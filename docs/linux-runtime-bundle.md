# Bundled Linux runtimes

Linux development candidates can carry their GEOS C API, GEOS C++ library and
OpenSlide runtime beside the native module and executables. Their installed
loader paths resolve these package-local files without an SDK directory on
`LD_LIBRARY_PATH`. This removes the external GEOS search-path requirement from
the Linux headless CI candidate. It does not establish a manylinux wheel or a
supported release across older system C/C++ runtimes.

`QUPATHXX_BUILD_GEOS=ON` enables geometry support.
`QUPATHXX_BUNDLE_GEOS=ON` additionally installs the two imported shared runtimes
under their SONAME filenames, for example `libgeos_c.so.1` and
`libgeos.so.3.15.0`. The bundle option requires a Linux target, shared GEOS
targets, and the matching SDK's `GEOS_LICENSE_FILE`. It rejects static targets,
missing notices and a C API library without an exact `$ORIGIN` runtime path.
The C API must resolve its own C++ dependency after relocation; resolving only
the top-level executable's dependencies is insufficient.

The [SDK builder](../tools/build_geos_sdk.py) retains the pinned GEOS source
identity and copies its `COPYING` notice into `share/licenses/geos`. On Linux it
sets `CMAKE_INSTALL_RPATH=$ORIGIN`. Its optional `--work-dir` keeps source/build/SDK
caches separate between operating systems, and `--jobs` controls build parallelism
(including MSVC's compiler parallelism on Windows). Existing Windows runtime
bundling remains enabled by the Windows geometry configuration.

The project sets installed module/executable paths to `$ORIGIN` whenever either
Linux bundle is enabled. This uses CMake's
[installed-runtime-path mechanism](https://cmake.org/cmake/help/latest/prop_tgt/INSTALL_RPATH.html).
GEOS bundling works without OpenSlide; `QUPATHXX_BUNDLE_OPENSLIDE` remains a
separate option for the verified self-contained reader SDK.

The Linux headless [CI entry point](../tools/ci/run.py) enables both bundles,
uses the SDK loader search path for building/native tests, then removes
`LD_LIBRARY_PATH` before installed verification and pytest. Its
[runtime probe](../tools/ci/check_linux_runtime.py) checks:

- Installed and wheel ELF hashes against the verified SDKs, including exact
  preservation of the GEOS notice.
- Actual process mappings for all three bundled libraries.
- A separately copied package in a path containing spaces, imported by a fresh
  isolated Python process with no SDK search path; a native GEOS union yields
  the independently known area of 150 pixels².
- Relocated native region pixels, a native batch checkpoint, and its explicit
  historical CSV export.

Four [configuration tests](../tests/test_geos_runtime_bundle.py) independently
compile/run a relocated C API consumer and reject missing notices, static
targets and unsuitable C API loader paths. They require the opt-in Linux
`QUPATHXX_GEOS_SDK` path and are enabled in the Linux headless CI environment.

The [local validation receipt](benchmarks/linux-runtime-local-windows-linux.json)
records source fingerprint
`1069666f68e222c2740d98eeff13a19c53329140561e5a87910e4b329394ca4a`:
1,590 Linux Python passes with 389 documented desktop/platform skips, 40 Linux
native checks, the full relocated runtime probe and strict stub/typing checks.
Windows passes 41 native and 50 focused checks, with four Linux-only cases skipped,
plus installed workflows and typing. No new full Windows suite is claimed for
this fingerprint. A separate native GEOS-only build disables OpenSlide, GUI,
Python, TIFF and training; its installed batch executable resolves both GEOS
libraries locally and processes an embedded project without `LD_LIBRARY_PATH`.

The [manylinux qualification pipeline](manylinux-qualification.md) separately
builds against an older baseline and checks fresh consumers; its hosted acceptance
remains pending.

The [hosted artifact audit](benchmarks/linux-runtime-ci-windows-linux.json) accepts
this scope at commit `5bfb9de85f607146c1c809c1360dd97af554271a`. All five jobs pass:
Windows desktop 1,968 Python/41 native, Windows headless 1,590 Python/40 native,
Linux headless 1,588 Python/40 native, and 40 native checks each for Linux minimal
and ASan/UBSan. Runtime records match the installed native and wheel artifacts;
all package-local runtime and relocation checks pass, including all four Linux
configuration cases. The captured desktop overview is visually inspected.

Older-system ABI/manylinux qualification, macOS distribution, supported installers and the
unanswered project-license decision remain open. Scientific/QuPath and hardware
acceptance have their own outstanding gates in the project plan.

# Native C++ SDK

P01 installs a self-contained native CMake SDK with the current C++ authorities.
The first qualified configuration is Windows x64, MSVC19.44/v143, C++17 or later,
Release dynamic CRT (`/MD`), `_ITERATOR_DEBUG_LEVEL=0`. The package checks the
compiler series and architecture at discovery, and forces an ABI header on C++
consumers to refuse static/debug CRT and incompatible iterator layouts. Debug,
other compiler series and other platforms are not qualified by this checkpoint.
Microsoft documents the [CRT options and required consistency](https://learn.microsoft.com/en-us/cpp/build/reference/md-mt-ld-use-run-time-library?view=msvc-170).

## Consume the installed SDK

Point CMake at the SDK's relocated `lib/cmake/qupathxx` directory:

```cmake
cmake_minimum_required(VERSION 3.24)
project(my_analysis LANGUAGES CXX)
find_package(qupathxx 0.1.0 CONFIG REQUIRED COMPONENTS core)
add_executable(my_analysis main.cpp)
target_link_libraries(my_analysis PRIVATE qupathxx::core)
qupathxx_stage_runtime(my_analysis)
```

```powershell
cmake -S . -B build -Dqupathxx_DIR="C:/SDK location/lib/cmake/qupathxx"
cmake --build build --config Release
```

Include native headers such as `qupathxx/project.hpp`,
`qupathxx/expression_aggregation.hpp`, `qupathxx/expression_reduction.hpp` and
`qupathxx/expression_differential.hpp`. Generated `qupathxx/sdk.hpp` supplies
version, exact compiled source identity, compiler and native feature flags.
`qpx::build_environment()` reports the library identity; it must agree with the
SDK header. This source/configuration identity qualifies the local engineering
candidate, not a general ABI promise for every build sharing version0.1.0.

Available application targets are:

| CMake target/component | Availability |
| --- | --- |
| `qupathxx::core` / `core` | Always; public native APIs, enabled readers and scientific services |
| `qupathxx::density_workflow` / `density_workflow` | Project storage and GEOS enabled |
| `qupathxx::nucls_import` / `nucls_import` | Project storage enabled |

Request additional components in `find_package` when needed. Unknown or
unavailable required components refuse discovery. CMake `qupathxx_FEATURE_*`
variables describe actual native backends, including the effective linear
algebra capability enabled by classifier training. No backend is rediscovered
from a system installation. A repeated find retains the same package and runtime
list; a different already-loaded SDK is refused.

`qupathxx_stage_runtime(target)` copies required SDK DLLs next to the executable
after linking. Alternatively, the application can deploy the files listed in
`qupathxx_RUNTIME_FILES` through its own installer. The usual compatible MSVC
system/runtime installation is still required. The SDK does not download a
runtime, Python, readers or model files.

## Installed layout and dependencies

The SDK contains public headers under `include/qupathxx`, application archives
under `lib/qupathxx`, private pinned archives/import libraries under
`lib/qupathxx/deps`, native DLLs under `bin`, CMake configuration under
`lib/cmake/qupathxx`, and exact notices under `share/qupathxx/licenses`.
The self-contained example is under `share/qupathxx/examples/native_sdk`.
Paths in exported include/link/runtime requirements are relative to the installed
prefix. Relocation needs no source checkout, original install location,
dependency SDK, package registry, download or Python/nanobind installation.

The selected full-feature package carries HDF52.2.0; libtiff4.7.1;
libjpeg-turbo3.1.3; tinyxml211.0.0; prefixed zlib1.3.2; SQLite3.54.1;
OpenCV4.11 core/ml and its private zlib; GEOS and OpenSlide from the exact existing
SDKs; and Boost.Math1.88 standalone code already compiled into the core. Private
target closure is checked at configure time, including hidden static dependencies.
Unexpected targets or link requirements refuse packaging. Unused JPEG/OpenJPEG
archives present in the OpenCV development SDK are not copied into this package.
Header-only JSON and Boost need notices, but no consumer headers or link libraries.
Windows system libraries and Threads remain toolchain/platform requirements.

Exact source pins/notices remain in the owning CMake dependency modules and
[third-party inventory](../third_party/README.md). Source/fixture/license receipts
from earlier workflows retain their original scope. P01 includes all selected
native dependency notices and checks installed file hashes; final L01 release
membership and corresponding-source staging remain separate.

The first ABI audit found that upstream JPEG's default `WITH_CRT_DLL=OFF`
overrode the parent dynamic CRT, causing earlier LNK4098 warnings. The native
JPEG wrapper now passes that upstream option from the parent's actual CRT choice.
The old archive is preserved; the current SDK uses `/MD` consistently and does
not suppress default libraries or linker diagnostics. Only that private codec
was rebuilt; the other numerical/backend archives were retained.

## Build and install from source

Use the usual native configuration, explicitly selecting needed open backends,
and opt in to SDK installation. Python remains OFF by default:

```powershell
cmake -S SOURCE -B BUILD -DQUPATHXX_INSTALL_SDK=ON -DQUPATHXX_BUILD_PYTHON=OFF
cmake --build BUILD --config Release --target qupathxx_core
cmake --install BUILD --config Release --component NativeSDK --prefix "C:/SDK location"
```

Build available `qupathxx_density_workflow` and `qupathxx_nucls_import` targets
before installation when those components are enabled. Their existing dependency
targets must also be built; an install does not compile missing inputs. The
`NativeSDK` component is independent of existing app, wheel and GUI install
layouts. `QUPATHXX_INSTALL_SDK=OFF` remains the default. Other platforms retain
their native builds, but this initial binary SDK install option explicitly
requires Windows x64 MSVC. Linux/macOS packaging is not qualified here.

## External consumer check

Copy the installed example to a directory outside the source/build trees:

```powershell
cmake -S copied-example -B consumer-build -Dqupathxx_DIR="C:/Relocated SDK/lib/cmake/qupathxx"
cmake --build consumer-build --config Release
consumer-build/Release/sdk_consumer.exe NEW_RESULT_DIRECTORY
```

The molecular example needs project/statistics/algebra backends and requests the
available density/NuCLS components. It compiles every installed public header,
checks compiled feature/source identity, imports an authored raw matrix, calls
native QC/normalization/pseudobulk/PCA/DE, retains exact results, saves/reopens
owned project data and exports CSV. Component entry points are linked/called.
It does not implement a second scientific algorithm or qualify biological results.

Its `--jpeg TIFF EXPECTED_RAW_BYTES` mode checks native raw decoding against
independent fixture bytes. This covers the changed private CRT dependency with
eight scalar/RGB, strip/tile and shared-table cases. Existing scientific numeric
receipts are reused; the new risks are installation, linking, runtime loading and
identity across the native consumer boundary.

The local check relocates the SDK into a path with spaces, removes the original
installation path, and configures/builds/runs from a separate consumer directory
with Python paths and package registries disabled. Exported properties, compiler
include logs and linker library logs are checked for original checkout/cache/
dependency/old-prefix requirements. This establishes source-independent build
inputs; it is not an OS filesystem-denial or fresh-machine qualification.
Expected wrong-CRT/debug/component refusals remain visible in their logs.
See the [source-scoped receipt](benchmarks/feature-expansion/README.md#p01-relocatable-native-sdk)
for final file identities, commands and validation scope. No wheel, new bindings,
GUI package or release publication is part of P01.

The [main integration receipt](benchmarks/native-format-closeout/README.md)
records the later matching-engine SDK relocation check. The local Windows
profile uses output prefixes short enough for derived filenames and atomic
temporary-file suffixes; general long-path support is not qualified by it.

# Native macOS portability gate

The optional `macos_native` CI mode builds the native engine on Intel x86_64
(`macos-15-intel`) and Apple silicon arm64 (`macos-15`). Runner labels follow the
[official hosted-runner matrix](https://docs.github.com/en/actions/reference/runners/github-hosted-runners).
The Python interpreter must report Darwin and the requested native architecture
before any build directory is created. The normal Windows/Linux and portable
wheel modes remain separate; `portable_linux` takes precedence if both optional
modes are selected.

This advances the macOS implementation and distribution work through real native
compiler/runtime evidence. It is not macOS package acceptance. The gate builds
GEOS 3.15.0 and OpenCV 4.11.0 from the existing hash-verified source identities,
retains their notices and uses the hash-pinned CMake/Ninja universal2 wheels.
The build uses the host Apple C++ compiler, records its version and removes
Python and dynamic-loader overrides. It builds Release with two parallel jobs.

The configuration enables geometry, quantitative TIFF/OME, projects, interchange
and native classifier training. Python bindings, GUI and OpenSlide are disabled
in this first native gate. At least 41 native test cases must pass without skips,
including the geometry, project, classifier, pixel-feature, cell and Gaussian
modules. The native batch Mach-O architecture and linked runtimes are recorded.
The region executable must reproduce independently specified PPM pixels exactly.
The receipt records source commit, runner image, architecture, compiler/build-tool
versions, configuration, JUnit, SDK library/notice hashes and the batch binary.

The build-host SDK paths may be embedded. Successful native tests do not prove
relocation, Python installation, whole-slide readers, GUI behavior, universal2
wheel correctness, deployment-target compatibility, signing/notarization, an
installer or a supported macOS release. These remain subsequent M7 gates.

Run the optional mode on a candidate branch:

```powershell
gh workflow run ci.yml --ref BRANCH -f macos_native=true
```

Or, on a Mac matching the requested architecture:

```bash
python -m pip install --require-hashes --only-binary=:all: -r tools/ci/requirements-macos-native.txt
python -I tools/ci/run_macos_native.py --work build/ci/macos-native --architecture arm64
```

The work directory must be new. Failed attempts keep their logs and are not
implicitly restarted. Artifacts are named `macos-native-x86_64` and
`macos-native-arm64`. Hosted acceptance is pending; the
[preflight receipt](benchmarks/macos-native-preflight.json) verifies runner/tool
inputs, workflow lint, wrong-platform rejection and the separate GEOS lib64
runtime fix without claiming macOS execution.

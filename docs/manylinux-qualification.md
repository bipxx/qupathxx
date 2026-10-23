# Manylinux qualification pipeline

This pipeline is under validation. No supported manylinux wheel is accepted yet.
The existing local Linux development wheel audits as `manylinux_2_39_x86_64`;
bundling its dependencies does not lower its system ABI requirements.

The optional `portable_linux` input to the existing CI workflow builds CPython
3.11 and 3.12 headless candidates in the pinned PyPA manylinux 2.28 image.
Builder and Debian Python consumer images are pinned to verified architecture-specific
registry digests in [manylinux-assets.json](../tools/ci/manylinux-assets.json).
SDK cache keys include those inputs, keeping newer-host SDK binaries out of the
older-baseline build. Regular workflow dispatches and pushes retain the existing
five-configuration matrix unless this input is enabled.

The [builder entry point](../tools/ci/run_manylinux.py) first requires a real
glibc 2.28 environment. It runs the full installed headless workflow, native tests,
typing, fixtures and benchmarks. It then calls the
[wheel qualifier](../tools/ci/qualify_manylinux_wheel.py), which:

- Examines every packaged ELF, including both native executables, for x86-64
  architecture, required system symbol versions and unbundled dependencies.
- Applies the pinned auditwheel 6.6.0 policy rather than inferring compatibility
  from filenames or successful imports on the build host.
- Runs auditwheel repair for the requested policy and checks filename/WHEEL tags.
- Requires native/runtime/notices payloads to remain byte-identical. Unexpected
  library grafting or ELF changes reject, preserving the verified SDK identities.
- Reinstalls the qualified artifact and repeats the runtime-mapping/relocation
  and installed-workflow probes without SDK loader overrides.

PyPA documents the [manylinux baseline model](https://github.com/pypa/manylinux)
and [auditwheel's system-ABI limitations](https://github.com/pypa/auditwheel#limitations).
A successful repair alone is not this project's complete acceptance gate.

Separate fresh Debian consumer jobs install the qualified wheel and the system
C++ runtime, with no SDK cache. Their [consumer probe](../tools/ci/check_manylinux_consumer.py)
checks the artifact hash, source commit, all installed ELF hashes, package-local
library mappings and an independent rectangle-union area. It opens the pinned
Aperio fixture, annotates a Unicode-labelled ROI, compares its native RGB mean
with NumPy, retains analysis, saves/reopens a project, exports measurements,
checks exact native CLI region pixels and processes/exports a native batch
checkpoint. The full consumer test suite then runs against that installed wheel.

The [local preflight receipt](benchmarks/manylinux-preflight-windows-linux.json)
records eight policy tests passing on Windows and Linux, verified image inputs,
actionlint validation and a real-slide workflow smoke on the current Linux host.
The independent ABI gate rejects all six ELFs in the known too-new local wheel
for the requested 2.28 policy and creates no retagged wheel. That negative check
and local smoke do not qualify an older ABI. Positive builder/consumer evidence
still requires the hosted run; supported releases, other platforms, hardware
acceptance and project-license selection retain their separate open gates.

## Observed builder layout failure

Run 39986257233 reached the real older-baseline SDK build. Its CPython 3.11 job
installed OpenCV under `lib64/cmake/opencv4`, then failed because the CI wrapper
passed a nonexistent `lib/cmake/opencv4` to CMake. No qualified wheel or consumer
acceptance resulted. The SDK helper and CI wrapper now locate exactly one complete
config in the SDK root, `lib`, or `lib64`; missing, incomplete and ambiguous
layouts fail explicitly. Six regression cases cover those layouts. The build and
consumer requirements above remain unchanged and require a new successful run.

The next run, 39991725062, passes wheel compilation and all 40 native tests,
then exposes the same lib/lib64 assumption in the independent GEOS runtime
verifier. The runtime verifier and relocated-library regression now select one
unambiguous C API library directory from lib or lib64. Four new layout cases
reject absent and ambiguous C API runtimes and cover both supported locations.
This preserves exact installed/wheel/SDK hash matching and the relocation policy;
no library is copied or retagged to bypass the check. A new successful build and
fresh-consumer run remains required.

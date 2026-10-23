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

## Accepting downloaded evidence

`tools/ci/accept_manylinux.py` requires one completed successful hosted commit
and both CPython builder/consumer cohorts. Wheels must have matching filename
and WHEEL tags, qualification hashes, complete native/runtime payload manifests
and runtime notices. Pinned tool versions, builder image/libc, the original
tested wheel and original/qualified installed, relocated and fresh-consumer
native/source identities must agree. Both consumers use the pinned slide fixture.

The checker requires passing native, builder and consumer JUnit evidence, public
stubs, positive/negative typing, package-local runtime records, geometry, Unicode
annotation/project reopening, retained analysis, CSV, exact native region pixels
and batch/checkpoint CSV. Only the explicitly recorded QuPath cell scientific
expected failures are allowed; other expected failures reject acceptance.

```powershell
gh run download RUN --dir build/portable-evidence
gh run view RUN --json headSha,url,status,conclusion,jobs > build/portable-run.json
python -I tools/ci/accept_manylinux.py --artifacts build/portable-evidence --run-json build/portable-run.json --commit FULL_COMMIT --output build/portable-acceptance.json
```

The receipt is written only after all checks pass. Its state is
`build-and-fresh-consumer-accepted` and `release_ready` stays false. The checker
audits evidence downloaded from trusted CI; it does not authenticate arbitrary
files or replace the builder's actual ELF symbol-policy check. Scientific failures
remain counted and do not become scientific acceptance. Project license,
GUI/macOS, biological accuracy, hardware budgets and releases need separate gates.

The [auditor receipt](benchmarks/manylinux-acceptance-auditor-windows-linux.json)
records 31 focused tests passing on Windows/Linux, including mixed/unfinished
runs, corruption, mismatched runtimes, wrong fixtures, loader overrides and
incomplete workflow/test/typing rejection. Synthetic cohorts test evidence binding,
not executable ELF validity. Actual failed run 39986257233 is rejected without
publishing a receipt. Positive hosted acceptance still requires the pending run.

## Observed GEOS verifier layout failure

Run 39991725062 passes wheel compilation and all 40 native tests, then exposes
the same lib/lib64 assumption in the GEOS runtime verifier. The verifier and
relocated-library regression now select one unambiguous C API library directory
from lib or lib64. Four new layout cases cover both locations and reject missing
or ambiguous runtimes. Exact installed/wheel/SDK hash matching and the relocation
policy are preserved. New portable run 39995913896 at commit
`1ff10524cc14665ffe2bee65310db742a6359205` reaches ABI qualification for CPython
3.11, where the scanner incorrectly treats the x86-64 GLIBC loader as an
unbundled dependency. The [loader-policy correction](manylinux-loader-policy.md)
retains symbol-version checks and is submitted as run 40003114384 at
`03ed2828c262415263a1e8e1f75eccc7e68dd9d6`. Twelve focused checks pass on both
Windows and Linux; replay of the actual failed requirements passes against the
pinned policy. Neither the original failure nor this replay is accepted as a
portable package or fresh-consumer pass.

The loader-policy follow-up now passes all four builder/consumer jobs. Its
[audited acceptance](benchmarks/manylinux101-build-consumer-acceptance.json)
qualifies CPython 3.11/3.12 wheels at exact source
`03ed2828c262415263a1e8e1f75eccc7e68dd9d6`, including fresh Debian consumers.
This establishes the pipeline on its older engine revision. Current Gaussian
and calibrated-cell sources need a new qualification run before their artifacts
can claim that policy. Supported release and the other M0–M7 requirements remain
open.


## Current accepted Gaussian and calibrated-cell engine

Run [40021039038](https://github.com/bipxx/qupathxx/actions/runs/40021039038)
qualifies accepted main revision `e457c74719204841b87f33f80e4871c79bf42b54`,
including the short-kernel Gaussian optimization and capture-time calibrated cell
features. The [artifact audit](benchmarks/manylinux104-build-consumer-acceptance.json)
passes for CPython 3.11 and 3.12 `manylinux_2_28_x86_64` wheels and both fresh
pinned Debian consumers. Each builder passes 42 native suites and 1,659 Python
cases; each fresh consumer passes 1,643 Python cases. Each Python cohort retains
exactly four scientific expected failures, with optional skips recorded separately.

The audit binds original/qualified wheel and installed native hashes, unchanged
runtime payloads, ELF policy/tags, pinned assets and notices, relocation checks,
stubs, strict positive/negative typing, exact reader pixels, project restoration,
analysis/CSV and batch workflows. It closes the older-engine qualification gap
for this exact accepted source. The newer physical-area and managed-viewer
candidates require their own qualification. GUI/macOS, project license, biological
accuracy, hardware budgets and supported releases remain open; `release_ready`
stays false.

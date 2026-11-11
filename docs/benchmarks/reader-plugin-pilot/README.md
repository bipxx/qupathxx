# Standalone binary reader pilot — 2026-11-10

Phase 3 of the [reviewed plan](../../format-reader-plugin-plan.md) is delivered
for Windows x64, MSVC 19.44.35225.0, Release `/MD`, Python OFF. Three workers owned
the shared P6 extraction/example, host bridge, and independent test modules/kit
proof; the parent owned the C ABI, verified loader and native integration.
The [guide](../../reader-plugins.md) describes the supported entry points.

The installed host loaded a real independently compiled P6 DLL from a relocated
development kit. Its graph contains no application core or Python target. Native
module-side decoding produces exact independent PAM bytes through region,
thumbnail, resampling and RGB-nominal routes. The explicit module error
`Reader plugin open: truncated PPM raster` proves the CLI bridge path; no fallback
output is written. Static-CRT configuration refuses for its intended reason.

Local evidence in the primary workspace's `build/reader-plugin-pilot` contains:

- Frozen compiler inputs, manifests, command logs and binary hashes. The initial
  passing-code test identity is
  `6c724b6ef47e5f40f5052306d6664383de0627f01bbcfa470211bc150bdd1dff`;
  its literal remains in the original raw test binary. Final corrected-test and
  installed-host identity is
  `fabba0a3302541c412e31683ed7b1dbdf69364925729aaa95a4367750abb2434`.
  The final manifest contains 826 compiler inputs. Corrections between those
  identities affect test fixtures and a test-only JSON dependency, plus generated
  provenance; passed operational code is unchanged.
- Sixteen focused CTest cases pass across the recorded runs: fourteen first-pass
  cases and the two corrected cases rerun afterward. They cover the pure-C ABI
  layout, bridge, loader and affected reader/source/project/save/recovery/import
  authorities. The loader case contains eighteen fresh-process checks: correct
  modules/dependencies, missing/changed bytes, wrong descriptors/profiles/ABI,
  null/throwing entry points, undeclared and foreign preloaded dependencies.
  Every rejection verifies its intended cause and continued built-in decoding.
- The bridge tests preserve UInt64 values above 2^53 and UINT64_MAX, valid NaN/Inf
  and signed zero, calibration and exact sample/validity bytes. Malformed
  metadata, buffers, short fills, error text and statuses refuse. Cancellation,
  concurrent reads, close during a read, source mutation and exactly-once
  destruction pass. Legacy view/options absence and standalone Project refusal
  remain explicit boundaries.
- Development host and kit install once. The proof at
  `kit-proof/44ac0a138c1d940b` copies and relocates that install, builds/installs
  the external module once from the installed decoder/header, and runs from an
  unrelated CWD. Four display modes, truncated-input refusal, raw-route refusal
  and the independent `/MT` guard-negative configure pass. No checkout/core
  library is needed by the plugin build. Scientific input bytes stay exact.

The continuation corrected a previous evidence gap: `QUPATHXX_HAS_PROJECT` is
private to the core and had not enabled the registry test's new SQLite checks.
They now run and pass, comparing exact saved reader/view/link/source-verification
payloads through reopen/resave. Offline TIFF/Zarr fixtures use required native
captured-view encodings; malformed missing-view rows still reject. This corrects
the [historical receipt](../reader-registry/README.md), rather than assigning its
skipped checks to an older source identity.

Failures are retained. A Windows Concurrency alias required a test-helper rename.
The newly enabled offline fixture initially lacked its required TIFF view, and
the mutation fixture attempted a write through the Windows source guard. Tests
now assert the actual native persistence/locking contracts without weakening
product validation. An unquoted PowerShell `-DNAME=$var` passed a literal variable
to the first kit invocation; a diagnostic established that cause, and quoting the
complete argument reused the successful install with no native rebuild.

All 38 existing binding object hashes remain unchanged. The independent panel
runtime is preserved; GUI changes were not part of this pilot's build or claims.
Candidate8 assets, wheel/SDK consumers and their original qualifications remain
frozen. There is no new format coverage, biological/performance claim or public
release. Phase 4 saved plugin sources, implementation-bound reuse/adoption and
admitted deployment/removal remain the next separate work.

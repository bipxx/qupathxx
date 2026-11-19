# Native label analysis checkpoint

Selected outcome: shared native SLIC regions and per-cell subcellular spots,
reused by an offline integer TIFF label adapter, with a usable CLI project and
export route. The [guide](../../label-analysis.md) declares algorithms, domains,
identity/persistence rules and limits. The later
[public OMERO checkpoint](../omero/README.md) is separate from these products.

The checkpoint is Windows x64 Release, C++17/MSVC19.44 with the retained admitted
GEOS/TIFF/OpenSlide/native dependency closure. New units link explicitly against
the retained core. The candidate9 runtime/core and frozen SDK/wheels are not
modified or requalified. This is native/CLI local evidence, not desktop controls,
Python bindings, fresh-host packaging, biology or whole-slide acceptance.

Local result: all four native suites and15 standalone batch routes pass. The
standalone process uses a Windows-only PATH with Python environment variables
removed; CSV/count/QPL bytes match native test outputs and scientific inputs are
unchanged. All78 protected baseline/candidate product hashes remain exact. The
local source/object/product manifest is
`build/gui-aggregation/gap-closure/results.json`; the work is uncommitted and
does not change any published release identity.

## Decisive checks

| Contract | Independent check |
| --- | --- |
| Immutable raster and archives | Authored labels/counts/bounds, exact canonical reopen; truncation/trailing/corrupt/rehashed-invalid/sparse archive refusal; new-only and cancelled export. |
| SLIC | Uniform quadrants, a strong red/blue edge, checkerboard islands, opaque/domain admission, deterministic identity, unchanged pixels, work/CPU/cancel checks. |
| Raw spots | Authored parent masks and original multichannel sums/counts/sample SD, zero parents, 4/8 connectivity, small/large rejection, Gaussian/DoG impulse, nonfinite/UInt64/budget/cancel cases. |
| Shared geometry and reducers | Existing raw accumulator and exact numeric admission are reused. Analytical rational anisotropic origin/extent contours agree; existing fluorescence regression remains passing. |
| Captured native workflow | Analytical decoded-RGB regions and original raw TIFF spots; inclusive polygon-boundary centers versus half-open rectangles; one-undo Keep, object/count publication and removal; stale ROI/cell/calibration/document and cancellation jobs. |
| Offline adapter | Independent unsigned TIFF encoding; exact sparse and UINT32_MAX IDs, multipart identity, original raw statistics, wrong input/shape/channel/type/ROI, changed-file, budget and cancel refusals. No external model assets. |
| Persistence and exports | Native save/open, explicit exact input/result/raster replay, byte-identical CSVs, exact QPL reopen, all-parent counts including zeros, protected/new-only exports; edited objects/counts and user descendants preserved. |
| CLI | Actual parser/service and standalone batch routes: create -> kept project -> reopen/verify -> export, plus explicit cohorts, record-only, no overwrite, protected sources/journals, argument/budget/cancel failures and original input bytes. |

Source tests are `cpp/tests/label_analysis_tests.cpp`, `label_workflow_tests.cpp`
and `label_cli_tests.cpp`. The existing `fluorescence_cells_tests.cpp` pins the
unchanged compartment contract. CMake registers these targets; workflow/CLI
checks explicitly skip with code77 when optional GEOS/TIFF/project capabilities
are disabled. The qualified local profile enables all three capabilities.

Local commands and exact source/object/product identities are retained in
`build/gui-aggregation/gap-closure/`. The incremental controller uses the retained
compiler/dependency configuration, recompiles only changed includes/units and
links the standalone batch dispatch with15 retained unchanged CLI objects. It
does not rebuild a core archive, bindings, GUI or package. The final local result
manifest identifies the current source patch and products; older intermediate
records/outputs do not inherit that identity.

Failure evidence remains alongside the passing logs. Corrections addressed a
manual consumer include, an invalid attempt to reuse nucleus-required cell
compartments for generic regions, lazy linked-source capture on project reopen,
and an independently authored TIFF fixture overlapping its next-IFD pointer.
The shared generic reducer fixes the abstraction; no nucleus assertion, numeric
tolerance or exact persisted identity check was weakened. Claude's read-only
plan attempt failed at the account usage limit before review and was not retried.

## Remaining catalogue scope

This closes the selected native/CLI label workflow. ICC/presentation remains R3;
broader eligible reader profiles and QuPath project migration remain R2/R5;
additional classifier/cell/TMA scientific cases remain R6/R8/R10; total-process,
whole-slide and Linux/macOS desktop evidence remain R12/R14. Version-specific
model execution, extension interfaces and other offline interchange remain R15
opportunities. These are separate workflow selections, not an implied promise
of100% QuPath parity or a reason to rebuild completed candidates.

## Source cleanup handoff

The later [integration cleanup](../omero/README.md#cleanup-and-commit) formats the
pending native label code and groups its CMake tests without altering scientific
tokens/contracts. Source-matched label/fluorescence/workflow/CLI objects are
rebuilt against the retained core; all four affected suites pass. The earlier
receipt above remains its own exact scope. New source/product identity and the
local commit handoff are recorded separately under
build/gui-aggregation/gap-closure/cleanup/results.json. No public, corpus, GUI,
binding, SDK/wheel or package campaign is repeated.

## PNG label-mask continuation

The later A08/R15 continuation imports unsigned grayscale PNG labels through the
existing optional `org.qupathxx.png` module and the same captured native label
workflow. It adds no decoder, dependency or model asset. The provider remains
SHA256 `098cb3a15d3e7e5811498023515c41c71404170c6bbb675ef7681483491bfbbf`;
its existing [source lock, notices and admission](../png-reader-plugin/README.md)
remain authoritative. New host code is AGPL-3.0-only. Published candidate9 and
all130 preceding protected products remain byte-identical.

Seven affected native checks pass: label analysis, fluorescence regression,
label workflow, label CLI, reader-plugin host, missing-PNG refusal and configured
PNG integration. Independent authored1/2/4/8/16-bit PNGs retain original integer
codes, including65535. The configured test checks sparse/disconnected IDs, original
raw sums and area, one-undo publication, save/open/exact replay, CSV/QPL equality,
decoder/version identity retention and tamper refusal, real decoder work allowance,
invalid color/alpha/palette/dimension/CRC, stale source and cancellation/budget
refusal. No numeric tolerance or old stored identity was relaxed.

Twelve standalone native batch routes pass with a Windows-only PATH and Python
environment variables removed: raw input capture, PNG import, retained records,
reopen/verification, CSV/QPL exports, missing provider, existing/protected outputs,
legacy TIFF verification/CSV and help. Native/CLI PNG outputs agree exactly.
The pre-change TIFF project retains record
`48058acacffc9a32ce3da2b2498be99d7c05d4aec5a3645c5a1091e6cceb7046`
and CSV SHA256
`12dba940a9c17894fd7f86394b5538ff9459972f6d264c0948c2cf8679d9de41`.

Commands, owned fixtures, source/object/product hashes, before/after legacy
comparison and test results are retained under
`build/gui-aggregation/gap-closure/png-labels/`. The existing integration cache
reuses compatible objects and the frozen core/dependency libraries; only changed
native/CLI/test objects are rebuilt. An initial new-test compile omitted the
explicit workflow-exception header; fixing that include reused all passing
objects and linked tests. One preflight receipt-key error stopped before native
execution. Neither failure changed acceptance assertions or triggered a campaign.

This is analytical Windows native/CLI crop scope. RGB/palette/alpha-channel label
interpretation, NGFF labels, model execution, new desktop/bindings, release
packaging, biological and whole-slide acceptance remain separate selections.

# Current implementation task

## Current execution card (2026-10-25)

- Active integration: `feature/scientific-grid-159`; native multi-file OME now has
  local synthetic reader/project/frontend verification. Open-source-only reader scope remains in force.
- Frozen TMA candidate: `19371b7`, unchanged. All original engine/Mac/portable
  jobs completed successfully; all 11 archives are size/hash/source verified.
- Acceptance: the [combined receipt](benchmarks/tma-dearray-202-qualification.json)
  accepts engine, both Mac-native architectures and both portable builders/fresh
  consumers at `19371b7`. The successor auditor `cc2f524` preserves the original
  source identity and the rejected stale-count audit. Main remains unchanged.
- E1/E2: named GUI audit repaired; early optimization refusal, source-byte helper
  execution and pytest-environment admission implemented. All 438 local auditor
  controls and 48 Linux stdlib boundary witnesses pass. This is tooling/local
  verification, not a new hosted product build at the auditor revision.
- MFO1: [local workflow evidence](benchmarks/multifile-ome-local.json) binds
  native source `1c62165` / fingerprint `8409a274...f6c80f` to seven passing
  native suites on each Windows/Linux host. Installed regression passes 304
  cases plus 14 declared skips; the 17-case desktop follow-up covers 12 desktop
  skips, leaving two OpenSlide capability skips. The final multi-file cohort
  passes all 39 cases, including both entry points, C/Z/T/SubIFD samples,
  cache/file/XML limits, project/QRA3/QRA4 replay, relocation/relink, CLI/batch
  resume, ImGui and managed viewer. Counts overlap; do not add them together.
- MFO1 qualification: exact native/installed/desktop/typing cohorts are registered
  in the existing producer and all three auditors. Frozen source `9e7d0be`
  passes 562 clean-checkout admission controls and seven source-contract proofs.
  The [campaign receipt](benchmarks/multifile-ome-210-campaign.json) records one
  full-profile dispatch per role: [engine40262416972](https://github.com/bipxx/qupathxx/actions/runs/40262416972),
  [Mac40262423808](https://github.com/bipxx/qupathxx/actions/runs/40262423808),
  [portable40262431017](https://github.com/bipxx/qupathxx/actions/runs/40262431017).
  Next follow those original handles, verify all required archives and run the
  strict exact-source audits. Do not redispatch because an observation expires.
  [A-D checkpoints](multifile-ome-tiff.md#delivery-checkpoints) have local evidence;
  E is in progress and unaccepted. Broader fault/cancellation, whole-slide,
  scientific, hardware and release requirements remain open. MFO1 remains
  incomplete. The original Linux-only audit accepts all three Linux modes
  (50 native suites each; 3,532 headless installed cases); other platforms
  remain pending, so no full campaign acceptance follows.
- E3: [observer-only source-proof reuse](benchmarks/source-proof-reuse-local.json)
  is locally verified at `0edb948` with 579 controls and a retained-artifact
  Linux successor audit. Every inherited job result matches; all 317 canonical
  compiler blobs remain identical to frozen `9e7d0be`. E4 remains open at the
  next producer-stage edit.
- The [efficiency review](reviews/compilexx-efficiency-review.md) records seven
  evidence-backed improvements. Earlier sections below retain workflow history;
  use this card for current scheduling. All M0-M7 remain Partial.

The full updated M0-M7 goal remains active. Every milestone remains Partial.
Reader implementation scope now follows the [open-source-only policy](reader-license-policy.md).
Exclude proprietary reader/codec SDKs from required parity; retain eligible
OpenSlide/TIFF workflows and multi-file OME work.
The qualified application source is `61e546e`; the promotion receipt changes
documentation/evidence only. Preserve the user's divergent local main.
See the [full plan](project-plan.md), [inventory](feature-matrix.md) and
[API delivery paths](api-delivery-plan.md) for the complete remaining scope.

## Accepted integrated raw-resource engineering campaign

The [combined receipt](benchmarks/raw-grid-191-qualification.json) verifies all
11 archive sizes/digests and the frozen 498-file source contract. Complete
inherited native semantic, installed, runtime, reader, analysis/archive,
classifier, batch, desktop and typing cohorts pass at one frozen source.

- [Engine40226794203](https://github.com/bipxx/qupathxx/actions/runs/40226794203):
  all five required modes accepted. Windows GUI passes 50 native suites and
  3,499 installed cases. Windows/Linux headless pass 49 suites and 3,069/3,067
  installed cases; minimal and ASan/UBSan each pass 49 suites. Extended readers,
  live pinned QuPath reference and required runtime manifests remain enabled.
- [Mac40226800465](https://github.com/bipxx/qupathxx/actions/runs/40226800465):
  ARM64 and x86_64 each pass 49 native suites, binary/notice inventory,
  relocation and fresh native probes. Mac Python, GUI and OpenSlide remain open.
- [Portable40226806311](https://github.com/bipxx/qupathxx/actions/runs/40226806311):
  both CPython3.11/3.12 builders pass 49 native and 3,067 installed cases; both
  fresh Debian consumers pass 3,025. Extended readers and ABI/runtime checks pass.
  The original queued CPython3.11 consumer completed without redispatch.

The named scientific cohort passes 283 GUI cases, or 276 headless cases plus
exactly seven declared desktop skips. The three strict legacy scientific
xfails remain explicit. These engineering results do not establish independent
biological equivalence, hardware/process-memory acceptance or release readiness.

## Raw-grid workflow preserved by this qualification

Typed raw sampling, QRA2 retention/replay, CSV, interrupted/resumed batches,
Python, native CLI, ImGui and managed-viewer workflows use the same native
contracts. TIFF layout conformance and interleaved decoder-block reuse preserve
source bits and axes. Hash-only capture uses canonical chunks plus validity;
coarse materialization pins chunk commitments and rereads only bounded chunks.
Changed raw bits/support reject before use. Dense grids keep their original
full-window policy. Canonical recipe/hash domains and QRA1/QRA2 encodings remain
unchanged; newly smaller-budget recipes require a compatible admission reader.

The [local materialization receipt](benchmarks/raw-grid-materialized-streaming-local.json)
retains independent ten-dtype references, exact admission and source-mutation
controls, dense fallback and the inspected result capture. Its source443 module
keeps its own fingerprint; it is not relabeled as source61. Admitted payloads
are not process RSS or latency measurements.

## Next work and remaining scope

The [active feature branch](https://github.com/bipxx/qupathxx/tree/feature/scientific-grid-159)
now contains engineering-accepted TMA dearraying through native C++, Python, ImGui
preview/atomic apply, the managed viewer and standalone CLI. See the
[frontend receipt](benchmarks/tma-dearray-frontends-local.json). The source191
baseline remains the accepted public main. The separate `19371b7` TMA campaign
is accepted by the combined receipt above; it has not been promoted to main.

The dearray cohorts are now registered in all auditors and the producer, with
373 local controls and independent collection/wiring checks. Explicit physical
units pass native, Python, ImGui, managed and CLI checks. The completed
engineering workflow's [frozen campaign](benchmarks/tma-dearray-202-campaign.json)
has completed and passed its successor audits. Continue representative real-slide/biological
and large-slide requirements. The original
axis campaign completed but was [rejected by strict engine/portable audits](benchmarks/tma-axis-196-audit-rejection.json);
do not promote it or dispatch another axis-only campaign to replace its records.

Continue vendor/multi-file/whole-slide and pyramid/tiled sampling, raw classifier
features, broader annotation/interchange and fault campaigns, independent
masks/features/split-merge tolerances, eligible hardware/RSS/GPU/OS evidence and
all supported-release requirements. Fixture rights/labels, project license,
notices, signing, older-OS/ABI and clean-machine decisions remain open. The
existing Java/Groovy/JavaFX first-release deferral is unchanged. No other M0-M7
requirement is dropped. Preserve frozen candidates, raw failures and the user's
local main; do not substitute this qualification for completing the full plan.


## TMA structural corrections: locally verified

Compiled source `96172f3` provides atomic row/column insertion and removal in
native C++, Python, managed ViewerSession commands and ImGui. Retained cores
keep IDs, case metadata and geometry; nested retained cores survive removal,
and undo restores the hierarchy. See [the contract](tma-axis-editing.md) and
[local evidence](benchmarks/tma-axis-editing-local.json).

The clean installed module has fingerprint
`12de03f213d3084399c8afa7cdcf1090b73c01226e6a8e12065fc0fc06e4fde4`
for 312 compiler inputs. Four selected native suites pass on Windows and
TIFF/project Linux. Installed tests pass 187 cases plus 10 declared desktop
skips; the complete selected desktop run passes 53 without skips. Generated
stubs match, six strict positive typing files pass and the new negative fixture
rejects three intended mistakes. The [inspected capture](benchmarks/tma-axis-editing-local/axis-controls.png)
shows insertion into a 3x3 grid with three missing new cores and no overflow.
Initial fixture-containment and WSL-launcher failures remain in the receipt.

The axis semantic/typing cohorts are now registered in the producer and all
three auditors at `b61f7ba`; [329 preflight controls](benchmarks/tma-axis-auditor-controls.json)
pass. Required evidence is two native semantic markers, 15 installed cases,
24 TMA desktop cases (or their exact declared headless skips) and three negative
typing errors. The shared source verifier now uses batched Git reads without
relaxing raw fixture bytes or missing/truncated-response rejection. All 312
compiler inputs still match the installed `96172f3` module; it is not relabeled
as the later harness source, and no native rebuild was needed.

Source191 is now accepted by all required engine/Mac/portable auditors and
promoted on public main as receipt commit `614ab99`. Its qualification does not
cover these later axis APIs. The [axis campaign](benchmarks/tma-axis-196-campaign.json)
is frozen at `ad4aa2c` after 329 clean-checkout controls and 506/35-file
scientific/axis source proofs. Exactly one full-profile engine, Mac and portable
run was dispatched; the original source191 handles are terminal.

- [Engine40235207517](https://github.com/bipxx/qupathxx/actions/runs/40235207517)
- [Mac40235213038](https://github.com/bipxx/qupathxx/actions/runs/40235213038)
- [Portable40235218333](https://github.com/bipxx/qupathxx/actions/runs/40235218333)

All 11 archives now pass size/digest/source verification. Both Mac-native
architectures pass their strict audit. Engine and portable audits reject seven
actual batch-source verifier controls absent from the frozen QPTMA case manifest;
the jobs being green does not override that failure. Preserve the
[unaccepted campaign and raw failures](benchmarks/tma-axis-196-audit-rejection.json).
Integration corrects the manifest and independently collects actual source node
IDs; all 330 auditor preflight controls pass locally. This correction does not
retrospectively qualify the old campaign. Public main's separate automatic
run40233922383 completed successfully at `614ab99`.

The original fresh-checkout fingerprint `2bf9748f...8ea103e`, compiled source961
identity and frozen sourcead4 remain distinct and unchanged. The newer dearray
implementation changes compiler inputs and requires its own named/full platform
evidence. Every remaining M0-M7 requirement stays active.


## Automatic dearraying reference preparation

The [dearraying contract](tma-dearraying.md) records seven independent synthetic
masks and an actual pinned QuPath0.7.0 binary-detector capture. The two-pixel
geometry tolerance was committed before capture. Four of five full-grid cases
meet it; skewed geometry reaches 2.2361 pixels and remains outside tolerance.
All five missing-flag sets match. An unobserved row yields 2x4 upstream and a
blank image yields 1x1, so the native proposal workflow must retain explicit
review/refusal for unestablished requested dimensions. Eighteen identity and
false-acceptance controls pass. The first native detector and preview/apply workflow now have local engineering
evidence; biological/whole-slide acceptance remains open.
The completed axis candidate was isolated from `cea83fe`, merged with the
qualified baseline and frozen at `ad4aa2c`. These dearraying research files and
any later implementation are outside that completed, unaccepted campaign.


## Automatic dearraying frontends: locally verified

Compiled source `a492ea5` and fingerprint
`2e217fbd3e36b7cfe58a99adff1615cd22d6e0141a256a3a89bf680f282e20ff`
identify 317 frozen compiler inputs. Five selected native suites pass on each
Windows/Linux host. The isolated installed Windows package passes 222 cases
with 19 declared desktop skips, nine new dearray desktop cases, all 53 inherited
desktop cases, generated-stub equality, seven positive typing files and three
intended negative typing errors. This is a locally assembled package, not a
qualified wheel. The initial CLI fixture setter error and corrected run remain
in the [frontend receipt](benchmarks/tma-dearray-frontends-local.json).

The controller reuses immutable native results, its existing bounded preview,
identity/revision checks and one atomic QPTMA import. The managed viewer queues
copied options, returns an owning proposal and applies through ordinary bounded
edits with the captured revision. The CLI writes a new proposal for one explicit
image and composes with existing project import. Inspected captures show the
controls and the saved/reopened skewed 12-core grid with gray missing B-2.

Remaining dearray-specific work is full platform, biological/upstream and
large-slide qualification. Physical conversion is now locally verified as below. All other
original requirements remain in the plan and inventory; all milestones are Partial.


## Physical dearraying and platform gates: locally verified

Compiled source `4b0e131` uses 317 compiler inputs at fingerprint
`6eb238703a073afce7247f89cd63c324969c990ca6db2a26c15d5fd3f0cc69f6`.
Pixel recipe identities remain unchanged. Micrometer requests bind the requested
sizes, separate calibration axes and resolved pixel dimensions; document-owned
workflows capture overrides and refuse unknown or contradictory calibration.
A generated calibrated OME-TIFF pyramid proves natural-level reading with
level-zero size/coordinate semantics. See the
[physical receipt](benchmarks/tma-dearray-physical-local.json).

Five selected suites pass on each Windows/Linux host. The isolated Windows
module passes 281 selected installed cases plus 23 declared desktop skips,
13 dearray and 53 inherited desktop cases, generated-stub equality, positive
typing and six intended negative errors. The producer and all three auditors
require the new named cohorts; all 373 inherited/new controls pass and actual
current evidence is accepted by the domain helper while older evidence rejects.
This is not a distributable-wheel or full-platform qualification. Preserve
original sourcead4's rejected campaign and the qualified main baseline.


## Frozen integrated dearraying campaign: in progress

Source `19371b7` is frozen on `feature/tma-dearray-workflow-202` after 373
clean-checkout controls and 512/35/60 scientific/axis/dearray source proofs.
Exactly one complete profile was dispatched per role:

- Engine [40245160107](https://github.com/bipxx/qupathxx/actions/runs/40245160107).
- Mac-native [40245165671](https://github.com/bipxx/qupathxx/actions/runs/40245165671).
- Portable [40245171698](https://github.com/bipxx/qupathxx/actions/runs/40245171698).

The [dispatch record](benchmarks/tma-dearray-202-campaign.json) preserves original
handles and attempts. Fresh checkout bytes have fingerprint `68af206c...dc36b7a`;
canonical compiler Git blobs match the local4b0 module's `6eb23870...f0cc69f6`.
Neither module/source identity is relabeled. Collect all11source/digest-verified
archives and run the three complete frozen-source auditors before acceptance.
Do not mutate the frozen ref or redispatch on an observation timeout. Sourcead4's
rejected campaign and public main614 remain separate. All M0-M7 remain Partial.


## Large synthetic pyramid: local Windows engineering check

The [local receipt](benchmarks/tma-large-pyramid-local.json) records a streamed
30720-by-22528 synthetic OME pyramid (2.08 GB decoded base RGB). The retained
physical-unit engine reads only level 3: four tiles and 786432 decoded bytes.
Geometry and missing flags pass the preregistered contract; 40 generator and
verifier controls pass. OS process peak, cache peak and admitted work are
recorded separately. No native rebuild was needed; 317 compiler inputs remain
unchanged. Biological/vendor-slide and other-host resource acceptance remain
open. This separate check does not change the frozen campaign requirements.

The [large-pyramid admission receipt](benchmarks/tma-large-pyramid-admission-local.json)
adds four zero-decode refusals: one-pixel source/output deficits, a one-byte
working-budget deficit and an oversized full-base read. Each same reader then
recovers at exact source/output/work limits with matching pixels and geometry.
The combined generator/verifier suite passes 115 controls. Platform and
representative biological/vendor-slide requirements remain open.


## Next implementation slice: multi-file OME

The [independent corpus](benchmarks/multifile-ome-preparation.json) fixes shuffled
C/Z/T placement, mixed byte orders, Unicode companion paths and subresolutions.
The [local foundation](benchmarks/compound-source-local.json) adds bounded owning
companion observations, QPV3/QRA3/QRA4, source-set stale comparisons and typed
Python descriptors. Legacy QRA1/QRA2 fixtures retain byte-exact round trips.
The [native workflow follow-up](benchmarks/multifile-ome-local.json) now opens
both corpus members and verifies private dispatch/shared budgets/guards,
project restart and Python/CLI/batch/ImGui/managed paths locally. Cohort registration is complete; follow the original frozen campaign for
full platform qualification in the
[slice contract](multifile-ome-tiff.md). All original M0-M7 requirements remain.

The frozen TMA candidate now has [strict Mac-native acceptance](benchmarks/tma-dearray-202-macos-native.json)
for ARM64 and x86_64, 50 suites each. Linux-only strict acceptance is retained
locally. All original jobs and both fresh portable consumers have now completed
successfully. The original strict engine audit rejected its stale GUI count;
the [separately identified successor audit](benchmarks/tma-dearray-202-qualification.json)
now passes every inherited gate. The complete campaign is engineering accepted
at `19371b7`; this does not promote main or establish biological/release support.

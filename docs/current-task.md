# Current implementation task

## Current execution card (2026-10-26)

- Corrected [campaign236](benchmarks/raw-classifier-236-campaign.json) is live at
  frozen source `530c6bb` on `feature/raw-classifier-qualification-236`: engine
  `40368263565`, Mac-native `40368276721`, portable `40368289302`. Each was
  dispatched once with the complete required inputs. The 718 frozen Git inputs
  match the 807-control preflight exactly. Poll these original new handles;
  authenticate every archive and audit from `build/ci/raw-classifier-236`.
  Linux-only acceptance is recorded below; full corrected-source acceptance
  remains pending. Keep this checkout unchanged.
- Corrected-source [Linux-only audit](benchmarks/raw-classifier-236-campaign/linux-observation/receipt.json)
  passes all three profiles at `530c6bb`: 50 native suites each and 3747
  headless Python passes. Native semantics, installed cohorts/typing, inherited
  runtime/reader gates and final source checks pass. Windows and Mac remain
  pending; portable acceptance is recorded below.
- Corrected-source [portable acceptance](benchmarks/raw-classifier-236-campaign/portable-acceptance/receipt.json)
  passes both CPython 3.11/3.12 builders and fresh Debian consumers at `530c6bb`.
  Each fresh consumer passes 3704 Python cases. All four raw cohorts give
  40 passes/18 declared skips; typing, inherited reader/runtime/source and
  packaging gates pass. The Git-only development fixture is explicitly skipped
  in the actual Git-free consumers. Mac arm64 has started; x86_64 is queued;
  Windows GUI is still running. This does not close full T5 or release gates.
- The next streamed-contour workflow has a [four-pattern exact reference](raw-stream-contours.md)
  at `9edfbc1`. [Local existing-path evidence](benchmarks/raw-contour-seams-local.json)
  records 101 Python passes, including all ten new cases; two OpenSlide cases
  skip. All sixteen actual raw stream layouts preserve labels and global crop
  indices. Existing full-grid geometry matches every edge, part, hole, count,
  area and perimeter exactly, using the retained installed `0dc916c` module.
  Initial fixture failures are preserved. This is next-workflow preparation,
  not native streaming contour implementation or a new wheel qualification.
- Original campaign235 [full engine successor audit](benchmarks/raw-classifier-235-failure/reconciliation/final-engine/receipt.json)
  accepts all five engine profiles at runtime source `176ec42`, with observer
  `530c6bb` explicitly separate. Native suites: 51 GUI and 50 each other profile;
  Python passes: Windows GUI 4212, Windows headless 3743, Linux headless 3741.
  All eleven original archives authenticate. Original Mac/portable failures stay
  unaccepted; this engine receipt cannot qualify the corrected C++ candidate.
- Campaign235 cannot achieve full acceptance: Mac arm64 job `121713805694`
  failed compilation at two mixed `size_t`/`uint64_t` `auto` declarations.
  [Original evidence and correction](benchmarks/raw-classifier-235-failure.json)
  preserve the authenticated archive, job/build logs and exact source identities.
  Integration `b8a465e` splits those declarations without conversions. Two
  affected Windows minimal native suites and their semantic contract pass;
  old/new declaration probes reject/pass for both Darwin target architectures.
  These syntax probes do not qualify a full Mac build. Both resulting Windows
  executables are retained separately before cache reuse.
- [Reconciliation evidence](benchmarks/raw-classifier-235-failure/reconciliation/receipt.json)
  preserves all eleven authenticated archives. Both Mac builds failed with the same
  deduction errors. Both fresh portable consumers failed only the new Git-backed
  development fixture because their runtime images omit Git. Integration
  `8e09571` declares that fixture unavailable without Git; the real Git-positive
  control and a real no-Git child pass. The 58 raw application cases stay required.
- Frozen Linux-only audit initially rejected the already-passing extra public
  stub case absent from the scientific manifest. Observer `c24f011` adds that
  case and independently checks actual collection. Its compatible successor
  audit against the clean original source passes all three Linux profiles:
  50 native suites each and 3741 headless Python passes. It does not qualify
  Windows, portable consumers, Mac or the corrected runtime source.
- All 807 selected successor admission/auditor controls pass at `8e09571`;
  eight source contracts agree across 718 unique inputs. The initial run using
  the stale root editable environment failed import; it is preserved alongside
  the passing installed-consumer run. No dependency reinstall was needed.
- Original [campaign235](benchmarks/raw-classifier-235-campaign.json) stays frozen
  at `176ec42`: engine `40363168207`, Mac `40363189978`, portable `40363203596`.
  All three runs are terminal: engine success, Mac and portable failure.
  The final engine audit and archives are preserved above; corrected campaign236
  is the active successor, rather than another campaign235 attempt.
- Pre-dispatch inherited checks add 286 passes to the 515 selected controls
  (801 total); all eight domain source contracts agree across 718 unique files.
- T5 preflight is implemented at `fb3dfe6`: the shared raw classifier contract
  is mandatory in engine, Mac-native and manylinux builder/consumer auditors,
  with matching producer checks. [Local preflight evidence](benchmarks/raw-classifier-preflight-local.json)
  records 515 passing admission/auditor controls, 459 bound source inputs,
  actual minimal native refusal, and exact GUI/headless declarations for 58 raw
  cases. Strict mypy passes four files and rejects twelve intended errors.
- Preflight fixed the runnable example's assertion-dependent `take()` calls.
  Its real child-process regression now completes checkpoint/stream resume and
  project retention under both normal Python and `-O`. Existing local wheel
  observations remain bound to `0dc916c`; this preflight does not relabel them as
  a new hosted or headless-wheel qualification.
- Latest frontend delivery: desktop/managed raw classification and bounded
  contour apply at `0dc916c` have [installed local acceptance](benchmarks/raw-frontend-local.json).
  Windows cp312 GUI/GEOS/training/TIFF/project ON, OpenSlide OFF: 195 Python
  passes, including all 17 raw frontend cases, and seven native suites. Three
  named skips cover OpenSlide and the separate backend-OFF profile.
- Real actions train/checkpoint-resume, preview exact indexed windows, recover
  from invalid model loads, save/export/resume, retain/reopen and browse history.
  A class-colored bounded contour preview leads to one atomic apply/undo/redo.
  Historical files grant no live apply authority; edits/image switches retire
  previews. Managed apply captures fresh authority, checks optional revision and
  reserves the full conservative result payload plus IDs before mutation.
- The geometry-OFF/training-ON native suite passes. Two training-OFF native
  suites and CLI training refusal pass at the same 383-input fingerprint; its
  CLI saves/reuses an actual ON-trained model and matches all 27 numeric blocks.
  Separate installed active-contour cancel/close probes both observe running
  tickets, prevent mutation, reclaim results and release borrowed readers.
- Wheel contents/source/capabilities, loaded GEOS DLL paths, notices and both
  SDK manifests authenticate. Stubs match; four valid typing files pass and
  twelve errors in three invalid files reject. The receipt binds 199 raw payloads
  and 26 retained final/development products, with original failures preserved.
  Dependencies are reused, so this is not clean-machine or release acceptance.
- Next: observe the three original236runs through terminal state, authenticate
  their archives, and run the maintained frozen-source audits. Preserve these T4.3-T4.5 local
  exits; do not rebuild consumer features or repeat full campaigns without a
  changed input/failure. Stream stitching, sparse ROI prediction traversal,
  automatic maintenance, abrupt-process/power-loss and independent scientific,
  resource/hardware/release gates remain open. All M0-M7 remain Partial; only
  open-source readers and required codecs are eligible.
- Latest accepted delivery: raw-classifier project batches at `c2c0fa0` have
  [installed local acceptance](benchmarks/raw-pixel-batch-local.json). Windows
  cp312, training/TIFF/project ON, GUI/GEOS/OpenSlide OFF: 136 Python passes,
  including all 14 new raw-batch cases; five named profile skips. Four native
  suites pass. The separate OFF native suite and actual-trained-model CLI
  save/reuse pass; all 27 output blocks match the installed ON consumer.
- C++/Python/native CLI reuse the existing batch kernel, immutable capture,
  checkpoint seals, owned bundles and Stop/Continue. Exact lattice/model
  admission, current-input hash-only reuse, QWL1/full-file verification, mixed
  RGB/raw catalogs, source changes, tampering, quotas, interruption, relocation
  and asynchronous snapshot behavior pass. See the [guide](project-raw-pixel-batches.md).
- Wheel/runtime source and 377 compiler inputs agree; twelve final/original
  products are retained. The original failing native fixture executable was not
  retained separately; its source/log/XML remain. Generated stubs match; three
  valid typing files pass and seven errors in two invalid files reject.
  Test dependencies are reused, so no clean-machine/release claim is made.
- Ordered remaining work: [T5 preflight and integrated qualification](api-delivery-plan.md#t4-execution-tasks-finish-consumers-before-another-qualification),
  then the remaining SCI1-B workflows and applicable decision/release lanes.
  T4's named Windows consumers now have local scope-matched evidence; broader
  platforms and every unfinished M0-M7 requirement remain open.
- Earlier [raw CLI](benchmarks/raw-pixel-cli-local.json) at `f463110` and
  [Python API/example](benchmarks/raw-python-bindings-local.json) at `202b41a`
  retain their source-bound baselines. New batch output is per completed image;
  per-file prefix resume remains the CLI/native-file-job path.

## Preserved local baselines

- QWL1 external raw prediction links/records at `a608a53` have
  [local acceptance](benchmarks/raw-stream-links-local.json): three affected
  workflows, sixteen regressions at `d42fad4`, 370 matching inputs and 22 retained
  binaries. Original RGB fixture failures stay source-bound. Metadata-only
  missing-file reopen, mixed records, save-as/recovery and verified relocation
  preserve the native persistence baseline.

- Owner prediction save/resume, training checkpoint and historical file/block
  jobs at `404f506` have [local acceptance](benchmarks/raw-pixel-file-jobs-local.json):
  three affected workflows, sixteen regressions at `447d3ae`, 367 matching inputs
  and 22 retained binaries. All 19 initial suites passed before export-worker
  full-prefix verification hardening. One-slot delivery, fresh owner/output
  checks and join-before-release remain the native integration baseline.

- QTC1 synchronous training prefix checkpoint/resume at `fb7ab73` has
  [local acceptance](benchmarks/raw-training-checkpoint-local.json): eight Windows,
  eight Linux and three Windows training suites, 363 matching inputs and 19
  retained binaries. Balanced/unbalanced multistage rows/hashes/checkpoint bytes
  and fixed-parameter backend models agree with uninterrupted execution. Replay
  repeats prefix IO/features; partial checkpoints publish no training table.

- QTT1 sparse training/trained archives and retention at `cc6fab4` have
  [local acceptance](benchmarks/raw-tiled-archives-local.json): three affected
  workflows, sixteen regressions at `30d0659`, 360 matching inputs and 22 retained
  original/final binaries. Mixed full/tiled project/snapshot/recovery readback,
  historical-authority refusal, all-selected record bounds and fresh owner
  replay remain source-bound baselines. Original fixture failures are preserved.

- QWS1 prediction files/hash-only prefix resume at `c843dfa` have
  [local acceptance](benchmarks/raw-stream-file-local.json): three affected
  workflows; sixteen regressions retain `125afbb`. Both source snapshots bind 356
  inputs and 22 retained binaries. Exact resumed bytes, TIFF close/reopen/trained
  models, conditional atomic commits, changed sources, tampering and aliases pass.
- Bounded raw prediction streams at `8d544eb` have
  [local acceptance](benchmarks/raw-pixel-stream-local.json): eight suites each
  Windows/Linux and three Windows training suites, 352 matching inputs and
  nineteen retained binaries. O(1) global traversal, fractional full-context
  seams, owned-block release, one-slot owner backpressure and fresh-file delivery
  controls remain source-bound baselines for the later file/resume path.
- Sparse owner-captured tiled raw training at `1273c3e`
  has [local acceptance](benchmarks/raw-tiled-training-local.json). The affected
  workflow suite passes on Windows GEOS-ON, Linux GEOS-OFF and Windows training-ON;
  348 compiler inputs match across all three. Sixteen passing regressions retain
  source `fb7ddb0`; only the test fixture changed afterward. Original three workflow
  failures and all 22 original/final executables are retained. The failure came
  from a quota fixture retaining a large string allocation after text restoration.
- `ScientificRawLattice` and `RawPixelTiledTrainingRequest`/table/job reuse the
  existing annotation/source snapshots, spatial index and forest. Full-grid and
  tiled training share one global-coordinate reservoir; three tile policies agree
  on raw/filtered feature rows, labels, owners and counts. A trillion-cell lattice
  supplies four labelled rows from one 4,096-core candidate tile. Actual Windows
  backend models and prediction votes agree with explicit effective parameters;
  real TIFF, physical calibration, quotas, cancel/stale/retry/thread controls pass.
- Integration: `feature/scientific-grid-159`. Native source `7beb80b` adds
  owner-captured raw annotation sampling and prediction jobs, sharing current
  raw-analysis/object authorities, spatial indexing, reservoir and model kernels.
  The [local receipt](benchmarks/raw-pixel-workflows-local.json) binds identical
  330-input fingerprint `700f1a10...e1677c` on Windows/Linux. Six selected suites
  pass per host: numeric/model/RGB regressions plus labelled overlaps, non-unit
  rational axes, document calibration overrides, pre-IO refusal, cancellation,
  metadata/compound-file staleness, retry and owner-thread result delivery.
  Initial test-helper include failures and the corrected development pass remain
  separate. Partitioned-index ownership now chooses the minimum annotation ID.
- Archive/retention source `440f610` now has [local acceptance](benchmarks/raw-pixel-archives-local.json).
  The affected `pixel_workflows` suite passes on Windows/Linux with 336 frozen
  compiler inputs and retained final binaries. The ten other suites retain their
  passing source220304d evidence; only the workflow test helper changed. The
  receipt preserves all five failed development rounds. It covers sampled and
  predicted archives, real-TIFF Document retention, project/snapshot/recovery
  readback, historical-authority refusal, stale-source and archive tamper controls.
- Positive backend training now has [local Windows evidence](benchmarks/raw-pixel-training-local.json)
  at `e7e4b66`: three backend-ON suites and the backend-OFF workflow suite pass.
  The pinned OpenCV 4.11.0 SDK verifies 195 manifest files. Real TIFF direct/job
  training, model/trained-archive files, retained project/recovery workflows and
  portable prediction votes pass. Exact historical retraining uses recorded
  effective forest parameters; fresh admission can change the work-budget field
  in model identity while preserving rows/votes. Earlier failed diagnostics and
  the deliberately stopped wrong-cache run remain separate evidence.
- Native replay and bounded contour apply now have [local evidence](benchmarks/raw-pixel-replay-apply-local.json)
  at `92edba8`: four selected suites pass on Windows GEOS-ON and Linux GEOS-OFF;
  three pass in the Windows training-ON/GEOS-OFF configuration. All 339 compiler
  inputs match across the configurations; eleven final executables are retained.
  Reopened TIFF records verify through fresh owner jobs; changed raw bits and
  historical/wrong-capture authority reject. Rational-axis/context contours,
  bounded conversion, cancellation, fresh-file guards and atomic apply/undo/redo
  pass on Windows GEOS-ON. Failed reader-cleanup and duplicate-apply development
  controls remain raw evidence, with broader regressions kept at their sources.
  Earlier Linux development XML was overwritten by a reused JUnit output path;
  original text logs remain. Final Linux XML is retained byte-exactly at its
  correct run, with this limitation recorded in the receipt.
- Raw sample-index window sampling/features now have [local evidence](benchmarks/raw-lattice-windows-local.json)
  at `ae0ba29`: five selected suites each Windows/Linux and two Windows training
  regression suites pass, with 342 matching compiler inputs and twelve retained
  executables. Independent Fraction references cover 13 wide-weight means and
  20 large irreducible-lattice uint64 samples; 9,072 tiled feature slots match
  full-grid bits across twelve dtype cases. A million-by-million global lattice
  admits bounded windows without constructing a whole-raster ScientificGrid.
  The shared exact sampler/feature algorithms preserve global phase and halos;
  distinct owning window data cannot silently enter legacy full-grid consumers.
- Window-aware models and owner prediction now have [local evidence](benchmarks/raw-window-classifier-local.json)
  at `642f728`: eight selected suites each Windows/Linux plus three Windows
  training suites pass, with 345 matching inputs and nineteen retained binaries.
  Portable QXS1/QRM1 bytes remain identical for equivalent full/window recipes.
  Forty-two full/window prediction cores match support, labels and vote bits;
  real TIFF and synthetic million-by-million global ROI prediction pass. Jobs
  enforce owner delivery, cancellation, document identity/revision, calibration,
  actual source metadata and fresh primary/companion-file guards. Metadata copies
  and sampler quotas/series now reject before IO for window and full-grid jobs.
- Earlier window-only boundary (superseded by T1 above): the window receipt did
  not cover tiled training/prediction or versioned stream/checkpoint resume. Continue
  nanobind/Python/CLI/batch/ImGui/managed viewer integration afterward. Current
  window prediction is one bounded window, not a completed prediction stream.
  Cross-tile contour stitching is separate.
  Linux/Mac GEOS-positive/backend-enabled and integrated platform training remain
  unqualified. See the
  [contract](raw-pixel-classifier.md) and
  [ordered route](api-delivery-plan.md#sci1-a-typed-raw-classifier-delivery-checkpoints).
- MFO1 fault campaign213 is now [engineering accepted](benchmarks/multifile-fault-213-qualification.json)
  at frozen `bfa416e`. All 11 archive sizes/digests/sources and strict full engine,
  both Mac-native architectures/relocation and portable builder/fresh-consumer
  audits pass. Windows GUI: 51 native / 4,053 installed; Windows headless:
  50 / 3,601; Linux headless: 50 / 3,599. Minimal/sanitizer and each Mac-native
  architecture pass 50 suites. Named multi-file installed cohorts require 54
  cases. Counts overlap. This campaign does not cover later raw classifier code.
  Original handles [engine40268520155](https://github.com/bipxx/qupathxx/actions/runs/40268520155),
  [Mac40268527953](https://github.com/bipxx/qupathxx/actions/runs/40268527953) and
  [portable40268533643](https://github.com/bipxx/qupathxx/actions/runs/40268533643)
  are terminal. Preserve the earlier partial receipt; do not redispatch it.
- Accepted feature baselines [TMA `19371b7`](benchmarks/tma-dearray-202-qualification.json)
  and [normal multi-file OME `9e7d0be`](benchmarks/multifile-ome-210-qualification.json)
  remain separate. Use the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue).
- Public main's qualified application source remains `61e546e`; main and the
  user's divergent local root branch are unchanged. All M0-M7 remain Partial.
  Open-source-only [reader eligibility](reader-license-policy.md) stays in force.
  Broader readers, annotation/interchange, three strict scientific xfails,
  independent research/model validation, hardware budgets, real TMA acceptance,
  platform support and release obligations remain open. Mac-native acceptance
  does not supply Mac Python/GUI/OpenSlide support.

## Historical task snapshots

The sections below retain earlier source-bound observations. Use the current
execution card above and the API delivery queue for scheduling. Older next-action
statements do not reopen completed work.

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

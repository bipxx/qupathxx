# Current implementation task

The full updated M0-M7 goal remains active. All milestones remain Partial.
The qualified application source on public main is `3fc3bcb`; the following
commit publishes documentation/evidence only. The user's divergent local main
is preserved. See the [full plan](project-plan.md), [inventory](feature-matrix.md)
and [API delivery paths](api-delivery-plan.md) for remaining work.

Hierarchy/recovery and the explicit-grid RGB-cell workflow now pass complete
source-bound engineering qualification. The shared native grid, float64 channels,
physical kernels, QCL5/QCA5/CSV, typed Python/stubs, CLI, batch, managed viewer and
ImGui paths retain their existing immutable capture and publication authority.
The [workflow guide](grid-cell-workflows.md) describes the entry points.

The [combined qualification receipt](benchmarks/scientific-grid-169-qualification.json)
records all required audits at the same frozen source:

- [Engine 40188586241](https://github.com/bipxx/qupathxx/actions/runs/40188586241):
  all five jobs accepted. Windows GUI passes 50 native suites and 3,256 installed
  cases; Windows/Linux headless pass 49 suites each and 2,831/2,829 installed
  cases. Minimal and ASan/UBSan jobs pass 49 suites each. Runtime inventories,
  forwarder/loader probes, readers, hierarchy, TMA, shape/frontier, archives,
  classifiers, batch, rendered workflow and strict typing checks remain required.
- [Mac native 40188591097](https://github.com/bipxx/qupathxx/actions/runs/40188591097):
  ARM64 and x86_64 each pass 49 native suites, uploaded binary/notice inventories,
  relocation and fresh native runtime probes. This excludes Mac Python, GUI and
  OpenSlide qualification.
- [Portable extended 40190126665](https://github.com/bipxx/qupathxx/actions/runs/40190126665):
  both CPython 3.11/3.12 manylinux builders and fresh Debian consumers pass the
  complete inherited cohorts, including required builder NDPI coverage and static
  runtime/ABI inventories. The earlier green portable run omitted extended
  readers and is retained as an under-scoped attempt, not full qualification.

The scientific installed contract requires 107 named cases: all pass in GUI;
headless requires 105 passes and exactly two declared desktop skips. Missing
binding casters and Darwin type deduction found in candidate168 are fixed and
covered by runtime getter/lifetime/None checks, strict typed returns and a guard
against unbound C++ annotations. The [correction receipt](benchmarks/scientific-grid-portability-fix-local-windows.json)
retains before/after failures, local installed checks and the precise source proof.
Both old candidate158/168 failures remain unqualified; their frozen sources and
raw evidence are preserved.

Next work preserves the full original scope: scientific masks/features and
split/merge tolerances; vendor/pyramid/tiled and typed quantitative sampling;
ICC/fluorescence, hierarchy/interchange fault coverage, automation/TMA/classifier
extensions; real hardware/memory/performance campaigns; rights/labels, licensing,
Mac Python/GUI/OpenSlide, supported OS/ABI matrix, installers/signing and clean
machines. Three strict isolated-cell scientific gates remain unresolved.
Engineering qualification does not establish biological equivalence or release
readiness. Continue through M0-M7 rather than closing them from green CI.

Earlier source-specific narrative remains in Git history and
[current-task history](current-task-history.md).

## Active comparison integration after source169

The feature branch adds an independent pinned comparison for the existing
explicit-grid production detector. Seven natural-grid cases require exact
nucleus/body masks and matched features at the frozen tolerances. All nine
retain independent feature arithmetic and context checks; eight isotropic
cases compare equivalent pixel and micrometre recipes. The clipped ROI nucleus
and requested-resampling differences remain visible. No native detector or
reference fixture changes are involved. The older distance recipe's three
strict expected failures remain unchanged; the existing LoG recipe already
passes those isolated-cell checks.

The comparison and public ROI debug capture join the named installed
scientific-grid contract, raising its next-candidate requirement to
185 GUI / 183 headless passes plus
two declared desktop skips. Source169's accepted 107-case contract remains
historical evidence for that source. New comparison tools, regression source
and immutable capture dependencies are bound by the next source contract.
The dispatch-profile helper also remains a post-source169 feature-branch change.
Do not transfer source169 platform acceptance to this newer harness.

The [local comparison receipt](benchmarks/grid-cell-qupath-comparison-local-windows.json)
records 252 passing installed/auditor checks and two declared desktop skips,
including all 27 new comparison cases. All 298 compiler inputs and the installed
native binary are unchanged from the retained source169 local artifact; a
426-file source proof binds the new harness and capture dependencies. No native
rebuild or duplicate platform campaign was used to establish this local scope.

The pinned public ROI debug capture now reproduces the ordinary output exactly
and shows a 192x128 cropped processing image. A second capture reproduces all
twelve non-log payloads byte-for-byte. The explicit reference-ROI/no-context
comparison removes the 24 native-only pixels but retains four reference-only
pixels. Their small positive upstream LoG responses differ from zero/negative
responses in the unchanged native C++ numerical probe. Bodies match exactly.
The new capture verifier rejects byte changes, recipe/identity changes and
re-sealed semantic corruption. Production arithmetic and old fixtures are
unchanged; no tolerance is fitted to accept this discrepancy.

The [ROI stage/domain receipt](benchmarks/grid-cell-roi-debug-local-windows-linux.json)
records 275 passing installed/auditor cases on Windows and on a fresh WSL Linux
environment, each with two declared desktop skips. Both execute all 32 grid
comparison and 14 debug-capture checks, and pass the 151-case headless named
cohort. The Linux wheel/native hashes match qualified engine run 40188586241;
the 298 Windows compiler inputs and installed native hash remain unchanged.
A 444-file proof binds the new harness and capture. These are local comparison
checks on retained artifacts, not a full new engine/Mac/portable qualification.

The resampled public capture now records the actual 171x128 RGB input and
nominal 1.5 coordinate step. Geometry-lattice and pixel controls distinguish that
step from 256/171 despite equal array shapes. Captured-input detector masks and
features match independently reconstructed processing geometry; the original
source area sampler remains explicitly different. The 26 new checks reject
mixed identities, re-sealed metadata/pixel/geometry changes and wrong mapping or
rounding assumptions. Fourteen non-log capture payloads repeat byte-for-byte.
No native production sampler, detector or original fixture is changed.

The [resampled processing receipt](benchmarks/grid-cell-resampled-processing-local-windows-linux.json)
records 305 passing installed/auditor cases on Windows and on the retained
source169 Linux wheel, each with two declared desktop skips. All 26 resampled
checks and the 177-case headless named grid cohort run on each platform. The
462-file source proof binds the new fixture/tools; all 298 compiler inputs and
both installed native module identities remain unchanged. The source169 full
platform campaign remains distinct from this newer local comparison scope.

## Active typed raw integration

Current native retention source: `ab206a7`, fingerprint
`faefc9ed27165c4de9d587d6c40139425c63e6b8a794853ea00b52de04ed4f63`.
The [QRA2 retention receipt](benchmarks/raw-grid-retention-local-windows-linux.json)
binds 308 compiler inputs and 485 source files. Windows and minimal Linux
resampling pass; TIFF/project-enabled Linux passes resampling and `raw_image`.
All 97 focused Python oracle/auditor checks pass, including twelve oracle checks.

Implemented dependencies include all-ten-dtype exact means and typed grid reads,
immutable metadata before pixel IO, source-bit hashes, per-channel support/
nonfinite reasons, a true hash-only scan, and explicit-grid raw requests/results/
jobs. Earlier evidence remains in the [mean](benchmarks/raw-scientific-means-local-windows-linux.json),
[reader](benchmarks/raw-scientific-sampling-local-windows-linux.json),
[digest](benchmarks/raw-scientific-digest-local-windows-linux.json) and
[analysis/TIFF](benchmarks/raw-grid-analysis-local-windows-linux.json) receipts.
The TIFF dependency fix preserves configured prefixed zlib across repeated
configuration; earlier rejected linker/header/WSL observations are retained.

QRA2 now retains grid geometry/options and original raw metadata without
changing natural QRA1 encoding. Independent Python struct/hashlib vectors pin
both versions and their actual native input hashes. All 188 derived-statistics
cases across ten dtypes (with/without ROI) keep records, canonically re-encode,
reject corrupt inputs and replay through fresh requests. Grid verification
compares statistic bits, including zero signs. Historical snapshots/reopened
projects remain unverified until explicit replay succeeds. Actual UInt16 TIFF
records pass SQLite save/reopen/replay and second save. The existing legacy raw
archive/project/batch cases remain in that native suite.

Natural-only CSV stays schema 1; grid/mixed tables use typed schema 2 with explicit
core origins/extents/counts/context and separate source/derived dtypes. Natural
scalar level/scale/origin fields remain blank for grid rows. Selective ASan/UBSan
instruments new sampling, analysis, record, archive and CSV units and their tests;
the remaining linked minimal core is unsanitized. This is not a full sanitizer
campaign. Thirteen prior native artifacts were retained/hash-verified before
cache reuse; the old source169 module remains unchanged. CTest evidence now
retains full successful output; the initial truncated passing JUnit is rejected
as semantic evidence and preserved.

The [Python binding receipt](benchmarks/raw-grid-python-local-windows.json)
records a Windows CPython 3.12 isolated candidate. Its module is compiled at
`fd32e9d`, fingerprint
`69fd8540468bcb379d6b29b5d94208e49d1fde8effacaa85bb966ce9bdfc9bc6`;
the delivery/stub/harness source is `1eadf23`. All 309 compiler inputs match the
compiled commit, so later stub/typing fixes reuse that binary without relabelling
its revision. A 489-file proof binds the delivery source.

All 137 selected raw/TIFF/record/project/batch/stub runtime cases pass, including
14 new typed grid cases: actual ten-dtype TIFF means (including exact UInt64/
Int64 cases), explicit channel/Z/time selection and nonfinite independence,
readonly HWC arrays surviving reader close/GC, copied options, optional retained
descriptors, QRA2 project replay and typed CSV. Generated stubs match that module;
six strict positive files and four exact negative typing cohorts pass. NumPy
annotations use a private alias and the grid descriptor is optional. The 110
pure reference/auditor/signature checks pass; a final registry correction reruns
all 89 auditor cases and matches complete runtime module case lists. Windows
ImGui library compilation and Linux binding syntax pass; no new GUI behavior or
Linux Python runtime is accepted. The initial editable-hook redirection, absent
staged CLI and NumPy-shadow typing diagnostics are retained as rejected evidence.

The candidate package is isolated in a new environment; the ordinary source169
installed environments remain unchanged. A wheel is not yet packaged/qualified.
The Windows development cache now has TIFF-enabled core/module and a current
natural batch executable; other cached executables retain older identities.

The [raw grid batch receipt](benchmarks/raw-grid-batch-local-windows-linux.json)
accepts selected local native checks at `9c99217`, fingerprint
`dc46c0c08b9a3ec95061c93e4f3fe3e1ac997232b5942363ba499b45c8ffd129`.
The shared ROI count/context planner, distinct raw-grid batch recipe and hash-only
resume pass Windows/minimal Linux resampling and cells, plus TIFF/project Linux
raw_image and project_batch suites. The latter exercises two-image interruption,
resume/reuse, QRA2 checkpoints and typed CSV. All 99 evidence-auditor cases pass.
The native Windows CLI separately passes exact UInt64 ordinary TIFF means, OME
channel/Z/time selection, Unicode checkpoints, compute/hash-only resume and
conflicting flag rejection. Its input-project/checkpoint helper uses the older
accepted Python module; new producer bindings are not yet runtime accepted.

Three failed native observations retain the impossible outside-Document-ROI test
setup. Corrected tests assert Document rejection and use an explicit grid for
outside-image sampling. Independent suites are now collected after a test
failure, with aggregate failure retained. An initial OME uint64 fixture failed
in the writer before CLI execution; valid dtype/format cases replace that setup.
The receipt preserves 65 payloads and distinguishes each source/artifact scope.

The [raw grid frontend receipt](benchmarks/raw-grid-frontends-local-windows-linux.json)
now accepts the local grouped producer candidate. Compiled source `8ece80e`
has fingerprint
`5297728aecff15fb76489a9dc9ab450eeb98d1378b37b36e4281979cabf2f951`;
delivery source `283a197` adds generated stubs and corrects a Python exception
expectation. All 311 compiler inputs remain equal to the tested module source;
a 492-file proof binds each source. The isolated package inventory includes its
module, companion CLI, DLLs, facade and generated stubs.

All five selected native suites pass on Windows and TIFF/project Linux:
resampling, cells, raw_image, viewer_session and project_batch. Installed Windows
runtime passes 142 cases with exactly three declared raw desktop skips; a separate
desktop-enabled cohort passes all 33 cases. Eight new producer cases include
copied GUI options, queue admission/reclamation, ROI factory/Python batch resume,
two CLI dtype/axis cases, ImGui measure/keep/typed CSV, managed retain/verify/save,
and desktop batch resume. Native viewer checks additionally cover owner-time
calibration and stale/cancelled publication. Six positive typing files, four exact
negative diagnostic cohorts, generated-stub equality and 120 pure checks pass.
Data controls and retained-grid result layouts were visually inspected without
overflow. The first runtime/desktop runs are retained as rejected evidence: a
new test expected ValueError for an established IndexError channel-range error.
Correcting that assertion required no native rebuild. The grouped Windows build
command took about 696.63 seconds; this is not a controlled compiler benchmark.

The [raw-grid workspace receipt](benchmarks/raw-grid-workspace-local-windows-linux.json)
accepts the conservative captured-payload ledger and repeated-scan correction at
compiled `326c66c`, fingerprint
`6b3c4fb14e290508947e7c8ec71394221a9eb1952ea1b12861f719d70b660261`.
Delivery `5e3c65b` keeps all 311 compiler inputs equal, freezes the byte-identical
source169 fixture contract, and adds a public captured-context budget test. The
delivery proof covers 493 files; the receipt retains 118 payloads and the
nine-file isolated package inventory.

Raw-grid execution reserves request/result objects, captured ROI/context/source,
metadata copies, geometry-hash temporaries and full-context eligibility/statistic
arrays before pixel IO, using one metadata observation. Source identity is
captured after lazy TIFF metadata initialization and before decoder reads. The
first sourcec770baa failed actual TIFF checks because it queried identity too
early; both rejected observations and binaries are retained. Full archive decode
still validates semantic counts. An immutable private eligible-count authority
avoids repeating the worker's ROI scan on keep/restore; the hash still binds the
mask/grid/calibration. Changed masks and checksum-valid false counts reject.
QRA1/QRA2 golden bytes remain unchanged, and historical lower-work snapshots stay
readable. New executions must satisfy the complete workspace admission.

A conservative rounding-gap proof avoids linear precision scans for safely
separated coordinates; boundary cases retain the exact scan. A pointwise oracle
checks 1,092 grids. Five selected native suites pass on Windows and TIFF/project
Linux. Installed runtime passes 143 cases plus three declared raw desktop skips;
the full desktop-enabled scientific cohort passes all 218 cases, and 33 selected
desktop regressions pass. Six positive/four negative typing cohorts, regenerated
stub equality, 120 pure checks and selective raw/raster ASan/UBSan pass. The
selective sanitizer links matching unsanitized core/TIFF/project dependencies;
it is not a full sanitizer campaign.

Seven-sample local observations on an unreserved workstation show 512x512 keep
median about57.77ms ->0.008ms and one-million-by-one capture about326.04ms ->0.0012ms.
All raw observations are retained. These are not controlled speedup or full UI/
hardware-budget acceptance. Reader decode/cache, allocator/runtime overhead,
process RSS and separately held results retain separate budget requirements.

The wider run exposed six stale manifests hashing the expanding live contract.
Their original source169 contract hash is preserved at an immutable fixture path;
no expected numeric data or native-header hash was replaced. The corrected cohort
passes 218 cases; the 211pass/6fail observation is retained.

The [qualification campaign](benchmarks/raw-grid-186-campaign.json) is now
frozen at `745662b` on `feature/raw-grid-workflow-186`. Its 493-file source proof
and 264 preflight controls pass. Preflight found two stale synthetic manylinux
typing assumptions, corrected to derive their counts from the shared manifest;
this changes no native inputs. The earlier 262pass/2fail observation is retained.

- [Engine40218705420](https://github.com/bipxx/qupathxx/actions/runs/40218705420):
  five Windows/Linux modes, extended readers and live QuPath reference enabled.
- [Mac40218710979](https://github.com/bipxx/qupathxx/actions/runs/40218710979):
  ARM64/x86_64 native, inventory and relocation; no Mac Python/GUI/OpenSlide claim.
- [Portable40218716355](https://github.com/bipxx/qupathxx/actions/runs/40218716355):
  CPython3.11/3.12 manylinux builders and fresh Debian consumers; extended readers
  explicitly enabled for builder NDPI coverage.

The complete frozen-source engine auditor now accepts all five jobs: Windows
GUI passes 50 native suites and 3,418 installed cases; Windows/Linux headless
pass 49 native suites and 2,990/2,988 installed cases. Minimal and ASan/UBSan
pass 49 suites each. Required readers, runtime manifests, scientific cohorts,
typing, rendered workflows and inherited semantic gates remain enabled.

Both portable CPython 3.11/3.12 builders and fresh Debian consumers are accepted:
49 native/2,988 installed cases per builder, 2,946 installed per consumer, with
213 scientific passes and five declared desktop skips in each of four cohorts.
The three strict legacy scientific xfails remain unresolved.

Mac Intel passes 49 native suites, uploaded binary/notice inventory and fresh
relocation probes. This is a single-architecture partial audit. Original ARM64
job121245381654 is now building; retain that handle and the frozen source rather
than redispatching. Complete two-architecture Mac acceptance and the combined
campaign remain pending. Mac Python/GUI/OpenSlide remain outside this gate.

Next: collect and audit ARM64, then combine the same-source receipts before
promotion. Independent integration work can extend cross-tile quantitative
sampling without changing the frozen candidate. Vendors/pyramids/tiled sampling,
application/hardware budgets, independent biological acceptance and supported
release work remain open. Every original M0-M7 milestone remains Partial.


## Post-freeze quantitative TIFF layout evidence

The [local layout receipt](benchmarks/raw-grid-tiff-layout-local.json) binds
30 new installed synthetic reader/sampler cases to delivery `23b5c71` and the
unchanged compiled `326c66c` module. All 311 compiler inputs and the nine-file
installed package inventory match; a 494-file source proof binds the test source.
The new 248-case scientific contract passes 243 cases with five declared desktop
skips locally. Reader/auditor controls pass 216 cases, including omission,
duplication, failure and skipped-case rejection for the new required cohort.

Exact Python Fraction footprints check compressed strips/tiles, big-endian and
separate-plane tiles, 64-bit integer rounding/cancellation, nonfinite channels,
image-edge context, deliberately different SubIFD previews, OME channels/Z/time/
calibration, nondefault series, zero cache retention and pre-decode admission.
The first 27 setup failures are retained: ImageServer has no context manager;
using contextlib.closing corrects the tests without changing the native API.

This is local engineering integration evidence. The frozen745662b campaign
retains its original 218-case contract; new tests cannot retroactively acquire
its platform acceptance. No extra builder was dispatched. Vendor/whole-slide/
multi-file sampling and broader scientific, hardware and release gates remain.


## Interleaved quantitative TIFF block reuse

The [local reuse receipt](benchmarks/tiff-interleaved-block-reuse-local.json)
binds compiled `046ac34`, fingerprint
`cc3100f8486087129a8d9c0e848b7f35c010a55ea1084874786abd57daab99fb`,
311 compiler inputs, a 494-file source proof and 60 retained evidence payloads.
Adjacent selected channels in an interleaved IFD now share one decoded block
regardless of LRU admission; separate-plane blocks remain distinct. The copy
checks cancellation between rows and keeps original channel order/bits/padding.

Before/after fixtures at zero and 1,024-byte caches observe 12 -> 6 interleaved
tile decodes and 6 -> 3 strip decodes; separate tiles remain 12. These are work
counts, not wall-clock speedups. Four before failures and two passing separate
controls are retained. Windows and TIFF/project Linux each pass five selected
native suites, including an independent byte-written multichannel TIFF fixture.
The isolated Windows candidate passes all 254 required scientific desktop cases,
189 runtime cases with three declared desktop skips, 34 selected desktop cases,
103 auditor controls and generated-stub equality. Selective TIFF-reader/test
ASan/UBSan passes with matching unsanitized dependencies; it is not a full-engine
sanitizer run. The first broader runtime setup omitted the region executable;
its ten failures are retained, and the current-source CLI is now included.

The frozen745662b campaign is separate: engine and portable are accepted, and
original Mac ARM64 job121245381654 has started building. Collect that exact job
and finish the two-architecture Mac audit before promotion. New046ac34 binaries
do not inherit the earlier campaign's exact-source acceptance. Full original
M0-M7 scope remains active, with all milestones Partial.

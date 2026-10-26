# Compilexx lessons applied to qupathxx

Current disposition (2026-10-26): the source-bound findings below are historical.
E1-E4 and multi-file campaign213 have the accepted baselines listed in the
[single queue](../api-delivery-plan.md#dependency-lanes-and-immediate-queue).
Current remaining classifier work is the T1-T5 route in that plan. Its new
efficiency decisions are sparse ROI tile-union planning, one global reservoir,
per-window admission and one complete frontend integration/qualification path.
Sparse tiled training (`1273c3e`) and bounded prediction streams (`8d544eb`) now have local acceptance; typed persistence/resume is next. No new cross-project or Astra audit
is claimed by this reconciliation.

Reviewed 2026-10-25 at qupathxx source `682ba00`. This is an implementation
efficiency review, not another Astra review or a new scientific/release gate.
All seven documents in `../compilexx` are covered. Their input sizes, line
counts and SHA-256 values are in the [inventory](compilexx-206-evidence/document-inventory.json).
Cross-project incidents are attributed lessons; the findings below have current
qupathxx evidence. Product/compiler inputs were not changed by this review.

## Findings, in execution order

### 1. P0: migrate the old GUI auditor alongside the named cohort

Category: code smell, duplicated contract. Confidence: high, actual rejection.

Evidence: `tools/ci/accept_unified_engine.py:407` still contains
`17 if args.tma_cohorts else 12 if args.tma_cases else 8`.
The frozen candidate's `tools/ci/tma-axis-cases.json` declares 24 GUI TMA cases.
The actual Windows GUI XML has exactly those 24 cases, all passing. All hosted
jobs completed successfully, but the unchanged full engine auditor exited 1 on
the older 17-case check. Its [original failure](compilexx-206-evidence/full-engine-audit.log)
and [case census](compilexx-206-evidence/gui-cohort-mismatch.json) are retained.
The complete campaign remains unaccepted; Mac-native and portable audits pass
independently. Eleven archive digests/sizes/source identities are verified.

Cross-project lesson: scikit-imagexx's metadata migration left a second strict
consumer unchanged (`scikit-imagexx-plan-inefficiencies.md:594-609`); ImageJ's
consumer-boundary review requires the complete producer/reader closure
(`imagejxx-plan-inefficiencies.md:729-732`). The earlier qupathxx QPTMA manifest
failure and this separate GUI count failure show the same migration cost.

Smallest improvement: make the existing named axis cohort authoritative at this
call site, retaining exact identity, duplicate, failure and skip rejection.
Exercise the complete auditor against a real-shaped GUI artifact before the
next dispatch, including a same-count substituted name and missing/failed case.
Keep the frozen candidate and original failed audit immutable. Bind a successor
auditor explicitly to the unchanged artifact/source contract and re-audit all
retained engine artifacts. Do not rebuild native code for this observer-only
repair or silently edit the frozen source to make its own source proof agree.

Owner/effort: existing auditor owner; small repair plus affected controls and
saved-artifact audit. This should precede another platform campaign.

### 2. P1: reject optimized execution before running the engine auditor

Category: code smell, assertions used for required external validation.
Confidence: high, distinguishing witness.

Evidence: `tools/ci/accept_unified_engine.py:85` is
`assert result['failures']==result['errors']==0`. The file contains 246 Python
assertions. The isolated exact `junit` function rejects a failing XML normally
but returns a result containing `failures: 1` when compiled with optimization.
See [witnesses](compilexx-206-evidence/witnesses.json) and the retained
[probe source](compilexx-206-evidence/probe.py). This tests that validation
function, not a claim that the complete CLI accepted a bad campaign. Follow-up
source inspection found an existing `not __debug__` guard after dynamic helper
imports: the CLI already rejected optimization before its network/audit work.
The remaining change is to move that guard before helper imports and make the
standalone JUnit validator reject explicitly. This narrows the original finding;
no historical CLI acceptance under optimization is asserted.

Cross-project lesson: scikit-imagexx explicitly identifies inherited
`PYTHONOPTIMIZE` removing controller assertions (`:611-619`); ImageJ requires
checking optimization at the actual child boundary (`:678`).

Smallest improvement: fail immediately when `not __debug__` at the existing
auditor entry, before helper loading/network/artifact work. Use explicit
exceptions as individual validation blocks are touched. A child launched with
`-O` must reject before any GitHub request; normal failing XML must still reject.
Do not mechanically rewrite all 246 checks or introduce a new validation engine.

Owner/effort: auditor owner; small boundary fix and two subprocess controls.
No historical optimized campaign execution was established by this review.

### 3. P1: execute the authenticated helper source, not an eligible stale pyc

Category: design smell, source identity differs from execution identity.
Confidence: high for the loading mechanism; no historical contamination shown.

Evidence: `tools/ci/accept_manylinux.py:19` uses
`resampling_spec.loader.exec_module(resampling)`; the engine/Mac auditors and
axis/dearray helpers use the same file-loader pattern. The local witness
creates a valid timestamp cache, changes source to equal-length bytes while
preserving mtime, and loads the old value despite `sys.dont_write_bytecode=True`.
Authenticating the source later does not establish which code that loader ran.

Cross-project lesson: ImageJ's authenticated helper review specifically notes
that `-B` prevents writes, not reading existing bytecode (`:193`).

Smallest improvement: at these authenticated dynamic-loading seams, read and
compile the exact source bytes that are bound to the source proof, retaining
`__file__` and existing module semantics. Share only the repeated loader among
the actual consumers. Test a distinguishing stale cache, nested helper loading
and ordinary success. A fresh private pycache prefix is an alternative where
the existing launch owner can enforce it before imports. Do not delete global
caches, add another environment or change the scientific comparators.

Owner/effort: shared evidence-loader owner; bounded multi-caller repair.

### 4. P1: admit the pytest environment before paying for compilation

Category: design smell, late configuration failure. Confidence: high.

Evidence: `tools/ci/run.py:100` copies `os.environ`; lines 205-206 later invoke
installed pytest with `-I`. Neither this entry nor `run_manylinux.py` explicitly
binds `PYTEST_ADDOPTS`/`PYTEST_PLUGINS`. In a fresh isolated installed subprocess,
the same one-case input runs one case normally, but inherited
`PYTEST_ADDOPTS=--collect-only` exits 0 with zero executed JUnit cases.

The existing strict named cohorts can reject incomplete results later. The
finding is late failure and uncontrolled test configuration, not proof that
exit zero alone grants full acceptance. Python `-I` does not neutralize pytest's
own environment settings.

Cross-project lesson: naparixx's runner review names these exact variables
(`:514`), while napachan's admission lesson requires the complete actual launch
contract before runtime work (`:410-417`).

Smallest improvement: reject conflicting inherited pytest options/plugins at
the maintained producer boundary, or bind an explicitly permitted profile.
Record plugin-autoload policy without disabling plugins intentionally required
by the suite. Exercise a real tiny pytest child as above; retain exact collected
and executed identities. Do not create another test runner.

Owner/effort: `run.py`/manylinux producer owner; small preflight and child tests.

### 5. P2: reuse source proofs within one immutable audit invocation

Category: code smell, repeated reads. Confidence: high for duplication; elapsed
savings are unmeasured.

Evidence: `tools/ci/accept_unified_engine.py:137` assigns
`entry['qptma_sources']=qptma.source_contract(ROOT,COMMIT)` inside the job loop;
resampling follows at line 138. Those are the same checkout/commit for all five
jobs. Six domain source lists contain 720 entries but only 585 unique paths:
135 repeated declarations, before the additional per-job repetition. Their
[overlap census](compilexx-206-evidence/source-overlap.json) includes the dynamic
512-file scientific list. Each shared `source_contract` still reads Git blobs
and working files rather than reusing the earlier source result.

Cross-project lesson: scikit-imagexx removed repeated proof-manifest reads within
one invocation (`:834-843`, `:899-903`); VisPy makes the same distinction between
invocation reuse and an unsafe persistent PASS cache (`:24`).

Smallest improvement: hoist checkout-only proofs outside the per-job loop and
reuse immutable results. Where source sets overlap, batch/deduplicate exact
paths within that invocation while checking every declared expected identity.
Keep artifact, installed-origin and job-specific checks per job. Recheck at an
actual mutation/trust boundary. Preserve schema compatibility initially.

Owner/effort: existing source-proof owner; small hoist first, shared read reuse
only after measuring the existing proof phase. No overall speedup claim.

### 6. P2: preserve independent diagnostics after a test-stage failure

Category: design smell, unnecessarily coupled stage execution. Confidence:
high for control flow; the amount of avoidable replay is unmeasured.

Evidence: `tools/ci/run.py:29` immediately raises on an unexpected child exit.
The broad installed suite at lines 205-206 precedes independent comparisons,
typing and standalone checks. Failure there prevents those later observations,
even if their installed-artifact prerequisites already passed.

Cross-project lesson: napachan added collect-all for independent outcomes while
retaining overall failure (`:167-172`); ImageJ moves cheap independent checks
before a broad suite rather than replaying the prelude (`:376`). Qupathxx's local
build drivers already retain independent selected native-test outcomes.

Smallest improvement: keep dependency preparation/build/install fail-fast.
After a verified installed artifact exists, collect eligible independent stage
outcomes with the existing runner, then fail the aggregate if any required stage
failed. Mark dependent stages not-run with the causal prerequisite. Keep finite
deadlines and no favorable retries. Test this with fake child commands, not
another full platform campaign.

Owner/effort: producer owner; medium, because stage prerequisites must be made
explicit before changing execution order.

### 7. P2: replace repeated current-state prose with one short execution card

Category: design smell, competing state descriptions. Confidence: high.

Evidence: `docs/current-task.md:246` still says
`Windows GUI and fresh portable consumers remain in progress` at the reviewed
source. The authoritative original handles now show all jobs complete, and the
engine audit is rejected for finding 1. Current work appears after more than
200 lines of status/history. The seven compilexx documents themselves contain
repeated cross-project reviews and superseding current cards.

Cross-project lesson: napachan reduced its current status page from 538 to 173
lines (`:378-381`); naparixx reconciles contradictory current fields (`:510`);
`agents_basics.md:5` says to prefer straightforward local code for one-off logic.

Smallest improvement: put the current integration source, frozen candidate,
exact campaign verdict, next deliverable and blocker at the top of the existing
task document. Keep historical receipts immutable and link them. Update this
card when the authoritative state changes; append only a new mechanism or
decision to compilexx, not a second queue or copied full ledger. Finish the
ready saved-artifact audit and compound-source workflow before another broad
cross-project review or unrelated feature expansion.

Owner/effort: root integration/documentation owner; small editorial change.

## What not to import from the other projects

- Their agent counts, storage ceilings, GPU/browser controls, Python-porting
  machinery and approval requirements are project-specific. No local orphan,
  storage exhaustion or ndarray ownership defect is asserted here.
- Cumulative PR path-filter waste is demonstrated in VisPy, but this review did
  not establish an equivalent qupathxx duplicate run. Do not add CI reuse logic
  for that possibility alone.
- Packaging-only reuse, frozen compiler inputs, exact dispatch handles, named
  cohorts, fixture rights and byte-preserved receipts already have qupathxx
  implementations/evidence. Improve their demonstrated gaps rather than
  proposing all of them again as missing infrastructure.
- Existing Windows build/configure records show 88.90/12.40 seconds for one
  physical-unit build, and Linux 111.97/56.21 seconds with different targets.
  They do not prove a controlled host speed ratio or that compilation dominates
  end-to-end delivery. Summarize existing phase records before optimizing.
- Open-source-only reader scope remains in force. This review does not select
  the project license, relax scientific tolerances or change M0-M7 completion.

## Recommended next work

Repair the demonstrated GUI cohort consumer and close its saved-artifact audit;
apply the three small execution-boundary fixes together at the existing owners;
then resume compound reader-source observations and versioned codecs through
the installed multi-file OME workflow. Hoist redundant proof work when touching
that code. Defer broader runner restructuring until its stage dependencies are
explicit. No new framework, dependency, native rebuild or performance campaign
is needed for the review findings themselves.

Review limitations: targeted source/control-flow review with three safe local
witnesses and the actual frozen engine audit. No rendered UX review, exhaustive
application-code audit or new biological/hardware acceptance was performed.
Product fixes are recommendations, not applied by this review.


## E3 implementation delta (observer only)

The [local receipt](../benchmarks/source-proof-reuse-local.json) records the
measured original proof phase, invocation-local hoisting/batching, 579 controls
and original/successor Linux audit equivalence. Source `0edb948` changes observers
and tests; all 317 canonical compiler-input blobs match frozen `9e7d0be`. Per-job
receipt copies preserve schema and prevent mutation of the private source
snapshot. A final deduplicated raw-byte check rejects source drift or conflicting
declarations before publication. The resampling proof retains its CRLF flag
and additionally authenticates the shared helper. No persistent PASS cache,
new native build, binary relabeling or overall CI speedup claim follows.
The original platform campaign continues on its unchanged handles; E4 remains
open. The review findings above retain their original source-bound evidence.


## E4 implementation delta (producer only)

The [local diagnostic receipt](../benchmarks/installed-diagnostics-local.json)
records 609 controls plus real Windows Python 3.13/Linux Python 3.12 child probes. Source
`2098cf2` keeps dependency/build/install/check_install fail-fast, then collects
serial independent diagnostics. Explicit artifact prerequisites and a final
aggregate failure prevent partial success publication. Existing CI JSON uploads
already retain the stage ledger. Valid JUnit may be inspected even after an
unrelated pytest failure; malformed/missing artifacts block their consumers.
All 58 legacy command sites and original deadlines/expected exits remain.

The initial legacy-prefix test used AST dump hashes that differ across Python
versions. It now parses the committed pre-refactor source in the executing
interpreter; the producer prefix itself was unchanged. Initial failure evidence
is retained. This later producer is not retrospectively executed by campaign210;
all 317 native compiler blobs are unchanged. The original campaign's separate
combined engineering acceptance does not close biological/resource/release gaps.


## Planning reconciliation (2026-10-26)

This update applies the recorded lessons to the current queue at `d19d403`;
it is documentation work, not a new independent Astra review or product audit.
The original findings and receipts above retain their reviewed-source meaning.

| Current finding / confidence | Verbatim evidence at the reviewed integration source | Improvement adopted and validation |
| --- | --- | --- |
| Completed work still occupies the active queue / high | API plan: `1 / E1: close the TMA audit` and `2 / E2: repair execution boundaries`, followed by `E1/E2 shared a coherent successor tooling freeze and are now implemented.` | Move E1/E2/E3/E4 to a completed-mechanism baseline table. Keep pending MFO1 acceptance as an observation/artifact join and SCI1-A as the next product implementation. No redispatch or repeated native build follows from a plan edit |
| SCI1 combines unrelated delivery dependencies / high | API plan: `4 / SCI1: remaining research workflows`, including `raw classifier features` in the same row as vendor readers and annotation work | Split the next complete classifier workflow into A1-A6 checkpoints with inputs and exit evidence. Retain readers, contours, interchange, spatial and automation breadth in SCI1-B; retain DEC1 and REL1. No scope reduction or inferred research completion |
| Typed reader availability can conceal classifier conversion debt / high | `pixel_features.hpp`: `std::vector<float> values_;` and `extract_pixel_features(const PixelBuffer&`; `pixel_classifier.hpp`: `classify_pixels(const PixelBuffer&` | Specify raw dtype/channel/grid/support/conversion semantics before public-header, binding or UI changes. Require independent extremes/rounding and streamed seam controls; preserve RGB defaults and archive compatibility. This is a planned remedy, not a claim that typed classification is implemented |

Cross-project lessons remain attributed rather than assumed local failures:
`napachan-plan-inefficiencies.md` sections on isolated test journeys and
clean-checkout dependencies support affected-cohort input closure before
expensive qualification. `imagejxx-plan-inefficiencies.md`'s latest review
supports tracing producer-to-consumer ownership and testing distinguishing
native SDK domains. `naparixx-plan-inefficiencies.md`'s latest executable review
supports recording effective build configuration and keeping current cards
separate from historical receipts. Existing qupathxx mechanisms already cover
immutable source pins, exact cohorts and saved-artifact reuse; no general runner
or new evidence framework is proposed.

Validation for this update is editorial: local links, queue/checkpoint coverage,
reader policy, preserved milestone scope and whitespace checks. Runtime and
scientific evidence are unchanged. Campaign213 remains pending acceptance until
its existing artifact join says otherwise; this update does not poll or promote it.


## Typed classifier execution delta (2026-10-26)

The [native feature receipt](../benchmarks/raw-pixel-features-local.json) binds
source `a13adef` and all 321 compiler inputs to six passing suites on each
Windows/Linux host. The independent direct 2-D Decimal oracle checks 12 source
cases / 12,600 feature slots, including explicit nonzero offsets and distinct
per-channel scales. Strengthened test fixtures follow the separately retained
`8ccd1c3` development pass; the production kernel did not change. Selected
executables, generated build identities, raw logs and framed Git source proof
are retained. No installed binding/wheel/stub rebuild was needed for a private
kernel with no public API change.

This implements the numeric feature authority, not training/model persistence
or a public raw classifier. Next integrate owner capture, model/schema and
physical recipe checks through the current training/prediction and codecs.
The [partial campaign213 receipt](../benchmarks/multifile-fault-213-partial-qualification.json)
separately accepts Mac-native/portable roles and Linux-only audits at `bfa416e`;
its original Windows GUI job is still live, and overall acceptance remains open.
No campaign was dispatched for each feature checkpoint and no saved binary was
relabeled. Existing broader scientific/resource/release requirements remain.


## Model geometry and qualification delta (2026-10-26)

Source `ed2bf40` adds raw model/result types reusing the existing immutable
forest and raw feature kernel. RGB's scalar-downsample result cannot represent
both raw grid axes. Explicit per-axis recipe/physical resolution checks and
QPR1 framing prevent silently applying a model to a changed channel/schema or
sampling contract. Six selected suites pass on each Windows/Linux host; see
[the local receipt](../benchmarks/raw-pixel-classifier-local.json). The next
owner-captured training/job adapters must reuse current source/ROI and object
snapshot authorities. Local model files do not complete project archives,
checkpoint resume, bindings or frontends; no wheel/stub campaign was run yet.

The original [campaign213 receipt](../benchmarks/multifile-fault-213-qualification.json)
now accepts all 11 verified archives and full engine/Mac-native/portable audits
at `bfa416e`. Preserve the earlier partial receipt and source identities. The
newer classifier source is not covered, and no new qualification was dispatched
for its individual checkpoints. All broader milestone requirements remain.


## Owner workflow execution delta (2026-10-26)

[Native workflow evidence](../benchmarks/raw-pixel-workflows-local.json) at
`7beb80b` verifies bounded raw annotation sampling/prediction jobs on both
Windows/Linux hosts. Six selected suites pass per host. Source/ROI and object
snapshot authorities, deterministic RGB reservoir and spatial index are reused;
no second document authority or worker-side document mutation is introduced.
Document calibration and raw file metadata remain separate captured epochs.
Fresh primary/companion guards run at completed-result delivery, rather than on
every progress poll. Actual backend-enabled training, retention/apply, archives,
phase-preserving streaming/resume and all installed frontends remain open.

The initial test-helper reuse missed explicit core/RGB includes; the core built,
but both test consumers failed. Fix the owning helper imports, preserve the raw
failure and avoid reliance on including translation-unit order. Review also
found that spatial queries do not promise ID order: use an explicit minimum ID
for same-class overlap ownership. A partitioned-index regression distinguishes
that case. Final 330-input source proof, raw logs and selected binaries are
retained. No wheel/stub rebuild or per-checkpoint hosted campaign was performed.

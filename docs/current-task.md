# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. R5-ROOT1 is locally verified in its recorded scope.

Current: **R5-SETTINGS1 — saved image analysis defaults** (ready for contract
preparation; no implementation or verification claimed).

- Outcome: retain recognized pinned ImageData image-type/stain defaults through
  native import, Python/ImGui inspection/use and saved reopen. Keep defaults
  distinct from historical image/ROI facts and explicit analysis requests; do not
  silently change recipes or historical results when loading settings.
- Evidence/entry: cpp/qupath_saved_data.cpp still reports image_settings_dropped
  for ImageData$ImageType and ColorDeconvolutionStains. Existing losses fixture
  contains brightfield H&E settings. Reuse StainTransform, normalized basis/white
  handling, stain_model_sha256, requests/retained recipe identity, QuPath preview/
  review, immutable document state, storage snapshots and current GUI controls.
- Next action: inspect the pinned serialized fields and fixture expectations;
  identify exact retained names/vectors/white/type semantics and current losses.
  Trace default consumption in analysis/cells/classifiers and UI before choosing
  a small typed state/API. Decide unsupported combinations, unset/explicit
  precedence, migration and undo before changing headers/casters/consumers.
- Checks: captured defaults and independent normalized-vector/identity values;
  import review/source preservation; save/reopen/legacy load and mutation/refusal;
  explicit recipes/results unchanged; owning snapshots and worker final release;
  thin Python/types and one actual apply/inspect flow. Do not repeat the existing
  stain numerical campaign unless its inputs or algorithm really change.
- Build/reuse: reuse installed root-facts-r5-env for baseline and the existing
  cache. Trace constructor/caster/worker ownership and borrowed-buffer lifetime
  before freezing. Settle interfaces/store/consumers for a combined build; use a
  dedicated staging-only environment so isolated children load the same product.
  Preflight selected pytest names, run affected contracts before final install,
  and generate stubs once if APIs change. Retain unchanged evidence in real scope.
- Stop: one pinned recognized settings workflow is retained and usable across
  that route, with unsupported values explicit and publication atomic. Broader
  ImageData/project properties, versions/builders/records, remaining R4/R5 gaps
  and R6-R16 stay in the queue. SETTINGS1 will not close all interchange or M4.

Latest closeout (2026-11-04, R5-ROOT1):
Outcome/evidence: [historical image facts](benchmarks/root-facts-r5/README.md)
closes declared root metadata/measurements through native/Python/ImGui/history/
schema19 snapshots/reopen. Shared ObjectProperties construction is native-owned;
image/ROI final worker release passes. Encoded borrowed BLOBs live through SQL
execution; private rvalue binding is rejected. Final native405ba1ad, unchanged
kernel scopece786260, observer49afda8b: 228 distinct staged passes/eleven skips
(15 new headless passes), six native suite roles, actual desktop, five stub checks
and positive/three-diagnostic negative typing.453 compiler inputs,62 installed
members/RECORD and the existing496-file pinned runtime match. Capture inspected.
Waste/decision: **retain** all failures/candidates; **reuse** ten unchanged pure
model cases and kernel results in their original scope. **Merge** fixes at common
fact construction and borrowed-buffer authorities. Three builds670.91/88.97/34.24s,
one final install and one stub generation; the latter builds address real bugs.
**Remove** private-helper test selection, old child imports and duplicated schema
report expectations. Use real public pytest nodes and a scoped staging .pth,
which also reaches -I children. Preflight async final release and argument/buffer
ownership before freezing the next API. No full platform/science/speed campaign.
Plan change/next action: ROOT1 is locally verified; reuse its facts/codec/store/
ownership mechanisms. Proceed to actual saved type/stain-default losses above.
Other properties and owner research/license/support/release decisions remain open.

After each task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse and necessary checks before continuing/retrying.
Only openly licensed readers and required codecs are eligible.

# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. R5-ROOT1 is locally verified in its recorded scope.

Current: **R5-SETTINGS1 — saved image analysis defaults** (contract settled; implementation and affected verification in progress).

- Outcome: retain recognized pinned ImageData image-type/stain defaults through
  native import, Python/ImGui inspection/use and saved reopen. Keep defaults
  distinct from historical image/ROI facts and explicit analysis requests; do not
  silently change recipes or historical results when loading settings.
- Evidence/entry: cpp/qupath_saved_data.cpp still reports image_settings_dropped
  for ImageData$ImageType and ColorDeconvolutionStains. Existing losses fixture
  contains brightfield H&E settings. Reuse StainTransform, normalized basis/white
  handling, stain_model_sha256, requests/retained recipe identity, QuPath preview/
  review, immutable document state, storage snapshots and current GUI controls.
- Investigation/contract: pinned version1 ColorDeconvolutionStains writes one
  bounded text map; missing third stain means a signed residual. A new synthetic
  capture covers all six image types, explicit three-stain basis and wide white,
  with reference values read back from saved data. Native normalization is reused
  and component comparisons use an absolute1e-14 bound; no new numerical kernel.
- State/API: immutable owning ImageAnalysisSettings values retain type, profile/
  channel names, complete normalized basis, residual marker and positive finite
  white. UNSET without stains becomes None. Wide white is retained exactly, while
  native RGB8 transform/application explicitly refuses unsupported white ranges.
  Set/clear uses staged history. Stored settings and explicit recipes are distinct.
- Store/UI: schema20 adds a sparse settings row using the existing bounded
  checksummed codec with a typed schema marker. Snapshots copy pure C++ values;
  borrowed encoded bytes live through SQL completion. GUI adoption is a deliberate
  Use saved stains action; existing parameter/result archives remain unchanged.
- State boundary: capture and native/binding/store/UI/test changes are drafted,
  not built or verified. ROOT1 evidence does not qualify these changed inputs.
- Next action: review the owning-value/caster, history/clear, snapshot charging
  and borrowed-buffer contracts; preflight public pytest selections and staged
  child imports. Then freeze compiler inputs for one cached build and run only
  the affected contracts before final packaging. Do not start another scope.
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

Latest task review (2026-11-04, completion-plan update):
Outcome/evidence: the [delivery queue](api-delivery-plan.md#execution-order-for-finishing-the-remaining-work)
retains all remaining R families and now makes contract, implementation,
verification and replan gates explicit. SETTINGS1 remains unverified; this
documentation task does not close implementation, research or release gaps.
Waste/action: **merge** execution and closeout rules into the single delivery
plan; **reuse** the existing inventory and scoped ROOT1 evidence; **remove**
any automatic rebuild/full qualification for this plan edit. No new reviewer,
benchmark, duplicated backlog or timing instrumentation is needed. The link
check found one pre-existing recovery anchor with no matching heading; it was
corrected to the existing evidence section before repeating the link check.
Next: complete the SETTINGS1 boundary review above, then one cached candidate
and affected checks. Reuse the pinned capture/runtime, existing stain authority
and build cache; do not inherit ROOT1 acceptance for changed compiler inputs.
After this slice, select the first demonstrated R4/R5 gap in the same queue.

Reusable implementation evidence: [R5-ROOT1 historical image facts](benchmarks/root-facts-r5/README.md). Its source/configuration and limits remain unchanged.

After each task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse and necessary checks before continuing/retrying.
Only openly licensed readers and required codecs are eligible.

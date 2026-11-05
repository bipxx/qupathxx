# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. R5-SETTINGS1 is locally verified in its recorded scope.

Current: **R5-DATAPROP1 — saved ImageData properties** (contract settled; implementation/capture preparation).

- Outcome: retain supported saved ImageData properties through native import,
  Python/ImGui inspection and project snapshot/reopen, keeping them separate from
  root/ROI facts, analysis defaults and executable workflow history. Unsupported
  representations need an explicit review/refusal policy; no silent narrowing.
- Entry/evidence: cpp/qupath_saved_data.cpp::properties currently discards every
  value except the reserved PROJECT_ENTRY_ID association. The existing pinned
  saved-data losses fixture contains synthetic="value" and reproduces this loss.
- Contract: immutable native ImageDataProperties owns a bounded tree of null,
  bool, signed64 integer, binary64 real, UTF-8 text, lists and string-keyed maps.
  Java wrapper widths normalize to these portable value types without numeric
  rounding; NaN/infinities and signed zero remain representable. Root/ROI facts,
  defaults and recipes stay separate. Unknown/cyclic/non-string-map Java values
  drop their whole top-level property only with explicit loss review; malformed
  framing/duplicates or exceeded limits refuse before publication.
- Bounds/API:4096 entries per collection,16384 total nodes,16 container levels,
 4096 UTF-8 bytes per key/text and1 MiB retained payload. Native-owned immutable
  shared values reuse the established worker-safe factory pattern. Python gets
  independent recursive dict/list copies; staged set/clear has ordinary history.
- Next action: capture the uncovered scalar/container/unknown/cycle/bound types
  once with the pinned runtime; reuse the existing synthetic string-loss fixture.
  Implement the native value and compact checksummed typed codec (fact maps lack
  these types), schema21 sparse row and thin Python/inspector consumers. Settle
  interfaces/ownership/store before one cached build. Confirm observer members
  from actual runtime/stubs. New capture does not repeat the existing string case.
- Reuse/dependencies: same pinned QuPath0.7 runtime, existing fixture and native
  stream/import-review authority; existing history, sparse transactional storage,
  native ownership and inspector patterns. No new reader, kernel or decoder.
  New capture is justified only for an uncovered type/behavior. Owner release,
  license and biological decisions do not block this engineering slice.
- Checks/stop: independent typed/key/value expectations; source-guarded review;
  clear/no-op/history and snapshot final release; exact save/reopen plus affected
  corruption/migration refusal; thin Python types and actual inspect flow. Settle
  interfaces before one cached build and reuse unaffected numerical evidence.
  Close only the declared property route; other R4/R5 formats/records/races and
  R6-R16 remain in the same queue.

Latest task review (2026-11-04, DATAPROP1 contract/baseline):
Outcome/evidence: pinned uncovered-type capture succeeds; fixture/probe hashes
pass. Previous SETTINGS1 installed product reproduces portable property loss.
All312 selected public cases collect, including41 new property cases. Native
factory/shared ownership, staged no-op/clear/history, snapshot charges and named
borrowed SQL buffers reviewed. Implementation remains unbuilt/unverified.
Waste/action: **reuse** the existing string fixture, parser/list helpers, native
factory pattern, cache and source/config evidence. **Retain** expected baseline
failure. **Merge** native/binding/store/UI into one candidate. A stopped draft
edit was repaired in place after inspecting actual exports and replacement
scope; do not rerun successful edits or use unscoped multi-match replacements.
Next: freeze compiler inputs for one cached build, then staged affected contracts
with a dedicated interpreter/.pth inherited by isolated children. Exercise only
new inspector flow, four affected native roles and installed typing/package
integrity. No numerical kernel, broad science or release campaign is needed.

After each bounded task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse and necessary checks before continuing/retrying.
Only openly licensed readers and required codecs are eligible.

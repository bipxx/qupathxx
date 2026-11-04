# Current implementation task

Follow the [single R0-R16 queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue),
[concrete next actions](api-delivery-plan.md#concrete-next-actions-and-completion-gates)
and [change-based validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. R0 is accepted; recent R2, R4-MERGE1 and
R5-QPPROJ1/QPDATA1/RELINK1 retain their scoped local evidence.

Current: **R5-SOURCE1 — implementation present, closeout pending**.

- Outcome: use the existing fingerprint worker to review source bytes, explicitly
  accept changed sources, and persist the association through save/reopen.
  Schema15, native review authority, Python bindings and ImGui controls are present.
  Ordinary project opening stays lazy; historical associations do not prove the
  identity of previously decoded pixels. Compound coverage remains primary hash
  plus dependency metadata, not hashes of every companion.
- Evidence: original candidate a66c8af5 has687 model passes/ten declared skips,
  passing stubs/typing and four native suites; the corrected project-test observer
  also passes. GUI correction ca009219 has two apply/save desktop passes;
  `desktop-ui-final.log` records the corrected declined-change case passing.
  Results remain under `build/source-verification-r5` in the root checkout, with
  distinct source/configuration identities. These are local engineering results.
- Next action: check final installed-product identity and generated stubs; close
  only any affected final-product behavior still uncovered. Reconcile the scoped
  evidence once and finish the source-review/storage API documentation. Preserve
  original failures and source identities; do not restart implementation.
- Reuse/dependency: reuse final `source-verification-r5-ui-env`, retained native
  results and the unchanged-core model/typing results within their actual scope.
  No new native input currently justifies a build, installation or platform run.
- Stop: one review/apply/save/reopen workflow has its necessary final-product
  checks and linked scoped record. Do not mark wider R5 or M4 complete.

Latest attempt closeout (2026-11-04):
Outcome/evidence: the last declined-change desktop rerun passes (one case,2.06s).
The test now checks source/baseline preservation; ordinary camera persistence can
legitimately dirty the project. The two apply cases use actual desktop Save.
Waste/cost: **reuse** the final installed product and passing independent results;
**remove** a full model/desktop replay for this test-only correction; **retain**
final identity/stub checks because the GUI product has a distinct build identity.
Plan change/next action: finish SOURCE1 closeout, then reconcile existing recovery
coverage before implementing a new fault. Killed-writer, hot-journal and abandoned
native-autosave tests already exist; do not recreate them from stale gap wording.

After SOURCE1: inspect the existing R5 recovery receipts/tests and correct active
coverage wording. Select a genuinely missing failure boundary only if one remains;
otherwise proceed to a demonstrated R4/R5 edit/property/plane gap, then R6/R7,
R8-R10, R11, R3, R12 and R13. Pull eligible R2 inputs forward as required;
retain R14-R16 full-target work and owner decisions.

After every bounded task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update this
card's state, next action, dependency, reuse and necessary checks **before** the
next task or retry. Keep only the latest closeout here; raw history stays in its
existing evidence. Only openly licensed readers and required codecs are eligible.

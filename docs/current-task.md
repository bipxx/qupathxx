# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. PLANE1 has local Windows engineering evidence.

Current: **R5-SOURCEAXES1 — retained planes when replacing image sources** (ready).

- Outcome: relink/review adoption must check retained object planes against a
  replacement's actual raw axes before publishing the source. Same-XY display
  compatibility alone does not establish Z/time/channel compatibility.
- Evidence/entry: project.cpp compatible() checks display width/height/pyramid/
  type/layout. apply_source_review() installs its candidate after source guards
  without comparing retained object planes. Reuse SOURCE1/RELINK1 and the new
  native plane model; candidate readers/metadata must not be opened twice.
- State: six baseline cases on695158a8 reproduce the missing Z/time/channel
  guard through both relink and review/apply. All six fail because no ValueError
  is raised. Raw results stay under root build/source-plane-replacement-r5.
- Next action: the compatible desktop save failure is an observer bug:
  options.inputs getter returns a Python copy, so append/extend never sent the
  save action. Build the event list separately and assign it once; retry only
  that case. Refusal desktop already passes after separating viewport dirty
  state from source state. Preserve prior GUI logs and reuse this product;
  source guards, staged/native checks, bindings and stubs do not change.
- Checks: reduction refusal preserves old reader/path/baseline and objects;
  compatible replacement and matching-byte move/no-op still work. Include a
  plane edit after review/before apply and appropriate stale/failure behavior.
  Reuse missing-source lazy reopen and existing source observations/owners.
- Build/reuse: settle the affected producer/consumer paths first. Use the existing
  cache and reader/source APIs. Run new and older affected contracts on the
  staged product before installation; generate stubs only if binding inputs
  change. Keep every passing result in its actual source/configuration scope.
- Stop: the declared source replacement workflow is safe through native/Python/
  affected desktop consumers and save/reopen. Other R4/R5 and R6-R16 remain open.

Latest attempt review (2026-11-04, SOURCEAXES1 desktop checks):
Outcome/evidence: six baseline failures prove the missing axis guard. A private
project.cpp fix passes93 staged contracts and three affected native suites.
Two desktop source actions execute, but test save/dirty expectations fail.
Waste/cost: **retain** both failures. **Reuse** the37.37s product and successful
staged/native checks. Inspect real GUI save controls/waits and distinguish source
adoption from viewport state before retrying; an arbitrary frame delay is not
proof of completed saving. Bindings/schema/stubs remain unchanged, so no stub
regeneration or typing/build campaign is needed. One build and one install so far.
Plan change/next action: establish an actual desktop apply/refuse/save sequence
on this product, then close the task only with saved source/plane evidence.

After each task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse and necessary checks before continuing/retrying.
Only openly licensed readers and required codecs are eligible.

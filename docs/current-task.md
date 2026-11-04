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
- Next action: a private publication guard is implemented in project.cpp;
  finish test/consumer review, then one cached build. It scans current planes
  without copying annotations, reads candidate raw metadata once only when
  needed, and preserves the existing matching-live-source no-op path. Run new
  and older affected contracts staged before installation. Bindings/schema/
  kernels are unchanged; reuse existing stubs and typing results.
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

Latest closeout (2026-11-04, PLANE1):
Outcome/evidence: [native planes](benchmarks/object-planes-r5/README.md) covers the
import/view/select/analyze/publish/save/reopen/export workflow, schema18 migration,
explicit unsupported cases and real ImGui/managed consumers. Source695158a8 adds
only a private exception correction to workflow source774abdc4; the evidence
records their scopes and retained failures. 972 distinct headless passing
identities, nine native suites plus the affected correction suite, three desktop
flows and unchanged-stub typing checks are recorded there.
Waste/cost: **retain** failures; **merge** producer/publication fixes before freezing
interfaces; **reuse** unchanged numerical/ownership/persistence, GUI and typing
checks instead of repeating healthy cohorts. Three cached builds took791.53s,
724.83s and45.03s; two installs and one stub generation. The broader contract
regression ran after the first install: move that staged check earlier next time.
**Remove** redundant extracted stages when permitted; automatic approval review
rejected the deletion as blocked by policy, so all four copies remain. Original
wheels/logs/products are retained. No platform campaign or independent-review loop was added.
Plan change/next action: SOURCEAXES1 is the next actual R5 boundary exposed by
retained planes; use the existing source authority rather than another framework.

After each task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse and necessary checks before continuing/retrying.
Only openly licensed readers and required codecs are eligible.

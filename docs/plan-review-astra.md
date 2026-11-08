# Plan review and adopted changes

Historical review: its paused-state wording and execution order describe the
2026-10-25 review, not current scheduling. Implementation subsequently resumed.
Use the [current execution card](current-task.md) and the single ordered
[API/workflow queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue).
The T1-T5 refinement is a planning reconciliation, not a new Astra review.

Reviewed on 2026-10-25 using **GPT-6 Astra, maximum reasoning effort**, explicitly
requested by the user. The independent reviewer read source/docs and local test
artifacts without editing files or starting builds. Its initial code baseline
was candidate `2694bfe`; public accepted main is `f2e8c2f`, while the intentionally
divergent local root main is `c86d323`. This review updates planning only; the
implementation goal stays paused.

## Findings and disposition

| Priority | Finding and evidence | Adopted change |
| --- | --- | --- |
| 1 | Competing current-state documents were stale: inventory named `82e159f`, current-task still foregrounded 1,233 tests, TMA contract said project CLI missing despite candidate 156, NDPI tail called the sampled recipe unexposed | Correct current inventory; make current-task a pointer; archive its old narrative and all implementation packets without changing their evidence |
| 1 | Partial status conflated missing code, missing engineering evidence, scientific discrepancies and release decisions | Explicit state vocabulary and per-workflow remaining deliverable; preserve all M0–M7 exit criteria and scientific exclusions |
| 1 | QPTMA reference/codec/project increments were being treated like separate deliverables while desktop completion remained open | Finish preview/apply/edit/save/restart/export as one vertical workflow using `GuiTma` and native codec/commands; no second workflow framework |
| 1 | CI concurrency key includes workflow/ref but not gate mode; modes on one ref cancel each other | Plan a small reviewed mode-aware concurrency fix and dedup ledger; retain same-SHA mode-specific refs until that fix, and preserve required promotion gates |
| 1 | Native project test initially took its disabled branch because it depended on a private compile flag | Require runtime capability/semantic marker plus the relevant installed cohort; retain the corrected tests and failures as evidence |
| 2 | Repeated auditors duplicate Git identity, JUnit membership, capability and receipt logic; suite-count/feature-flag chains grow with packets | Extract shared evidence primitives incrementally in touched paths, with small declarative domain contracts and negative controls. Do not replace scientific comparators with a generic count |
| 2 | Full campaigns/rebuilds can begin before UI/CLI/Python/persistence integration and stub packaging settle | Focused local checks → installed vertical slice → clean final artifact/source check → one frozen required platform campaign; rebuild changed binary inputs, re-audit compatible retained artifacts for auditor-only fixes |
| 2 | Scientific and environmental failures were easy to misread as missing features | Preserve sampled production recipe availability and its scoped NDPI results; distinguish current recipe from legacy 299/506 observation; eligibility failure does not mean a failed render target |
| 2 | Broad scientific grid gaps could trigger separate implementations in several algorithms | One scientific sampled-grid contract for origin, axes/units, halo, ROI policy, dtype, limits, recipe identity and replay; RGBA8 area filtering does not cover quantitative raw resampling |
| 2 | Many worktrees and inconsistent relative paths increase stale artifact/source mistakes | One active integration checkout plus one frozen candidate, isolated comparisons only when needed; inventory active references, use absolute artifact paths and retire safely after evidence/ref preservation |
| 2 | License, fixture rights, research labels/tolerances and hardware qualification have no automatic software solution | Dedicated decision/evidence lane with repository-owner/research-lead input and explicit dependencies; no unrequested license selection or weaker success definition |
| 3 | Original strict M0→M7 serial implementation order and starting calendar estimate no longer reflect the implemented system | Schedule remaining dependencies and complete workflows; retain every original milestone/backlog and re-estimate each remaining slice with assumptions |

## Resulting execution order

1. While implementation stays paused, finish this authoritative plan/status map.
2. On explicit resume, validate the final project-enabled QPTMA wheel and finish
   its ImGui workflow using the existing native authority.
3. Qualify the completed integrated candidate at the required platform gates.
4. Close the actual slide-annotation workflow and remaining M0–M4 evidence.
5. Resolve shared scientific sampling and reference gaps, then finish research
   workflows using current tissue/cell/classifier/record paths.
6. Complete batch/live/Python compatibility on those same workflows, then all
   remaining supported-release, licensing, hardware and clean-machine gates.

[API delivery paths](api-delivery-plan.md) expands this order into 26 API/workflow
families, existing entry points, easiest reuse routes and decisive acceptance.
The full objective remains in [the active plan](project-plan.md); no milestone
is declared complete by this review.

## Verification of this documentation change

The original M0–M7 criterion blocks are preserved, except that the two release
labels now explicitly require their exit evidence before acceptance. Original
performance targets and broader parity backlog are retained. The entire earlier
packet suffix is preserved in `project-plan-history.md`; earlier current-task
text is preserved in `current-task-history.md`. Local links in changed forward
documents and whitespace checks are verified. No source, build, runtime, CI YAML
or scientific tolerance was changed. This plan revision has not restarted the
paused implementation goal or launched qualification runs.

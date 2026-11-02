# Current implementation task

Work is paused. This process cleanup does not resume implementation or launch CI.

Latest pushed implementation: QuPath070V2 cell processing, with C++/Python,
saved-recipe replay, CLI/batch and ImGui/managed delivery. The installed product
matches all 506 nucleus/body masks in the pinned CMU-1 crop, preserves V1 output,
and passes the local checks in the [307 receipt](benchmarks/qupath-body-boundary-307/receipt.json).

Qualification308 froze source `ae7a4148`. Its engine41110186379,
Mac-native41110197405 and portable41110209770 runs completed successfully
(last checked 2026-11-01). Artifact authentication and complete acceptance
audits remain pending; a green run alone is not audited acceptance.

Next action when implementation resumes: collect the existing 308 artifacts
and run their prepared original auditors once. No new build or dispatch is
needed. Then continue the next research workflow using affected checks under
the [validation policy](api-delivery-plan.md#validation-and-build-policy).

The [306 closeout](benchmarks/original-source-cell-qualification-306/terminal307/receipt.json)
remains accepted for its declared engineering scopes. Neither that acceptance
nor the one-crop V2 result proves broader biology, arbitrary context/feature
parity, whole-slide resource/seam behavior, general speed or release readiness.

All M0-M7 remain Partial. The full scope stays in the [project plan](project-plan.md),
coverage in the [workflow inventory](feature-matrix.md), and ordering in the
[delivery queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue).
Readers and required decoders remain open-source only.

Earlier task narratives and superseded next-step instructions are preserved in
[history](process-status-history-through308.md). Update this short card rather
than copying campaign narratives into four plans.

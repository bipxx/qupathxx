# Current implementation task

Use the [single R0-R16 queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [ordered slices](api-delivery-plan.md#execution-order-for-finishing-the-remaining-work).
R0 is accepted; R1 inventory is reconciled with owner decisions open. Recent R2,
R4-MERGE1 and R5-QPPROJ1 have scoped local evidence. All M0-M7 remain Partial.

Next: **R5-QPDATA1, pinned native saved-data decoding contract**.

- Outcome: native import of the established QuPath0.7.0 saved-data contract,
  with explicit unsupported/loss reports, through existing project preview/apply.
- Entry/dependency: inspect pinned PathIO and object/ROI serializers, actual binary
  fixtures and their assisted GeoJSON expectations before choosing decoder types.
  No biological or release decision blocks this synthetic engineering task.
- Reuse: QPPROJ1 image adapters, private candidate, source guards, native object
  model/ID mappings, loss-aware geometry import and non-overwriting snapshots.
- Checks: establish binary record/version/geometry/class/plane expectations;
  compare direct results with the pinned independent exchange, preserve hierarchy,
  pixels, identities/calibration and reopen behavior; reject corrupt/truncated or
  unsupported records explicitly. Bindings/UI checks reuse the common decoder.
  Batch settled native changes into one cached build/install; no platform campaign.
- Stop: the declared pinned saved-data workflow works natively without its
  assisted exchange. Additional formats/properties/builders return to the same R5
  row. A generic Java runtime or plugin interpreter is outside this task.

After this slice, select uncovered R4/R5 integrity/editor gaps, then follow R6/R7,
R8-R10, R11, R3, R12 and R13; pull required eligible R2 inputs forward and retain
R14-R16 parity. Name the actual behavior, dependency and decisive check each time.

Latest closeout (2026-11-04, R5-QPPROJ1):
Outcome/evidence: [reviewed manifest plus assisted annotation import](benchmarks/qupath-project-r5/README.md)
has23 distinct new passes,242 selected model passes/four skips, two additional
targeted passes, three distinct selected desktop passes and three native suites.
Native ee7555e8 and observer78254e7b retain source identities; stubs/typing,
62 installed members and RECORD verify. Direct .qpdata decoding remains open.
Waste/cost: two native attempts/one success after missing ascii includes; separate
API files avoided another shared-header change. One final install with native-byte
preserving stub repackaging. Direct PathIO avoids empty fallback/cache writes;
index-only null serialization fixes observed absent bindings. Corrected only the
failed dirty-workspace test and reused passing results. No platform replay.
Plan change/next action: use pinned serializers/fixtures for QPDATA1 before coding;
keep native geometry and publication authorities. Finish public signatures before
installing. Use the project Python for capture conversion and explicit workdirs.

Perform the [waste review](api-delivery-plan.md#task-closeout-and-waste-review)
after each task or failed attempt, updating this card before continuing/retrying.
Only openly licensed readers/codecs are eligible. Source308 retains its own
platform scope. Biology, hardware, project license and release support remain
owner decisions; engineering verification does not prove overall speed superiority.

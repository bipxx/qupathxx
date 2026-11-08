# R7-AREA1 — physical tissue component filters

Locally verified at final source `40ece8a916cf7002834cf76451fd5e0d74febf69`.
All M0-M7 remain Partial. [Contract](../../tissue-area-limits.md) and
[workflow tests](../../../tests/test_tissue_area_limits.py) define this scope.

The shared calibrated count resolver now serves cell and tissue requests. Tissue
physical limits intersect existing pixel limits after level selection; detection,
morphology, components, retained masks and tables retain their existing path.
The ephemeral calibration stamp rejects stale publication. Saved results/replay
keep the frozen count recipe and captured provenance; no archive/schema change.
Native, owning nanobind, managed viewer, ImGui, SDK batch and CLI share the factory.

## Evidence and reuse

- [Native tests](native-tests.log): tissue, cell_calibration and project_batch,
  3/3 pass. Independently known 1/2/4-pixel components cover inclusive boundaries,
  empty/intersected filters, unknown/invalid/extreme calibration, anisotropic and
  1.5x level conversion, stale commit and frozen count replay.
- [Initial staged run](stage-tests.log): 121 passes, 7 explicit GUI skips and one
  observer failure. The CLI saved correctly; the test guessed the wrong directory.
  [Only that repaired node](stage-cli-repaired.log) passes using the emitted report
  path. Save/reopen, historical tissue records, calibrated cells, tables and stub
  surface checks retain this source-specific result; they were not rerun wholesale.
- [Actual desktop nodes](stage-gui-tests.log): 2 pass, covering the managed owner
  and inspector detection flow. The five other GUI skips remain unclaimed.
- [Strict valid typing](typing-positive.log) passes. [Invalid typing](typing-negative.log)
  retains exactly three intended argument/assignment errors, without broad suppression.
- [First installed wheel](installed-tests.log): 7 shipping checks pass.
  [Final installed CLI/batch check](installed-final-tests.log) passes after the
  heading repair. The final scrolled inspector [capture](controls-final.png) and
  [assertion](controls-final.log) show readable physical bounds and the expected
  one component/two kept pixels.
- [Shipping integrity](shipping-integrity.json): all480 raw compiler/build inputs
  match the installed source hash; all65 hashed wheel members match RECORD and
  installed bytes. Public facades/stubs match source. [Reuse proof](evidence-reuse.json)
  verifies that only the GUI heading input differs from the [staged identity](stage-environment.json).
  Numerical, API and persistence code is unchanged across that difference.

The stage reports77e2bc9f+dirty because it was configured before the code commit;
its raw input hash, not a relabeled revision, binds its evidence. The final wheel
reports40ece8a9. Build/install logs, the first wheel and both retained stages live
under the workspace build/tissue-area-r7 directory. The staged runtime uses the
existing isolated project-metadata-r5-probe-env; shipping checks use the existing
project-metadata-r5-env. No root editable-runtime import was used.

Reproduce the native scope with CTest's anchored tissue/cell_calibration/project_batch
selection. Run the linked Python module for the SDK scope; enable QUPATHXX_TEST_GUI=1
for the two GUI nodes. Use tests/typing/tissue_area_valid.py and tissue_area_invalid.py
with the existing config-only silent stub-import behavior. The retained logs record
exact selections/results. The installed source/RECORD check retains the complete
input/member maps rather than claiming other hosts or biological acceptance.

## Waste review and next action

Reuse the existing segmenter, count recipe, samplers, environments and cached build.
One public-stub generation and one full binding compilation were needed. Two wheels
and two installs were used: visible-control inspection was late and exposed a
clipped header in the [first capture](controls-first.png). Shortening it required
only affected GUI recompilation/relinks, with no repeated numerical campaign.
Inspect scrolled controls before packaging in future UI work.

Several read-only probes guessed filenames/tool syntax/API fields. The CLI observer
also guessed checkpoint layout; use discovered paths, actual declarations and emitted
report fields before execution. A raw-input reuse comparison first used Git-normalized
LF bytes instead of the compiler's CRLF bytes; preserving newline bytes corrected the
observer without a rebuild. The PPM viewer could not decode the capture, so unchanged
RGB bytes were encoded as PNG for inspection. Original captures/failures are retained.

Next: R7-FILTER1 traces existing pre-threshold tissue smoothing/median and grid/halo
contracts, chooses a demonstrated missing researcher workflow and extends the shared
request/kernel. Broader stain/specimen, vendor, whole-slide resource, host/release and
speed claims remain in the existing queue. No reader, model, generic reporting system
or new qualification campaign was introduced.

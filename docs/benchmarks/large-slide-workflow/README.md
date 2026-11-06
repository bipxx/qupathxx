# Representative large-slide tissue workflow

Local implementation evidence uses source `6da9917c+dirty` containing the
canonical-pyramid changes committed with this directory. The final installed
candidate's identity and package checks are recorded in [the delivery receipt](delivery.md).
The old published candidate1 does not provide this mode.

## Result and boundary

CMU-1.svs (CC0; SHA256
`00a3d54482cd707abf254fe69dccc8d06b8ff757a1663f1290c23418c480eb30`)
has level-zero dimensions46000x32914 and native level2 dimensions2875x2057,
downsample16.00048614487117. Processing legacy tiled reads at256/512 changed456
foreground samples and component counts684/692. The decoder returned different
pixels for the same nominal sample because integer level-zero tile origins
introduce different decode phases. The existing explicit area grid is bounded
and rejects this whole-slide level-zero input; it is not the workaround.

The opt-in `TissueParameters.canonical_pyramid=True` requires explicit level>0,
reads that level at origin zero once, and processes the immutable snapshot in
tiles. It budgets both RGBA output and decoder ARGB staging, uses algorithm
`qpx.pyramid-threshold-components/1`, input format
`qpx.rgba-pyramid-origin-zero/1`, and persists as QTM6. The input digest includes
the level geometry, crop bounds and full snapshot bytes, independently of tile
size. Other parameters still participate in result verification. Legacy modes
retain their algorithms, hashes and QTM1-5 encoding.

Both tile sizes give5913875 supported samples,1083798 foreground samples and688
components, identical labels/input hashes and zero differences against an
independent RGB optical-density threshold calculation. Cancellation leaves no
result or retained record. Native estimated work is102136112 bytes at tile256;
the observer process peaked at271642624 bytes, including Python/oracle/decoder
caches. The allowance bounds native planned work, not total process memory.
A coarse level must fit the snapshot and work limits (maximum512MiB work);
this does not provide streaming level-zero whole-slide cell analysis. Decoder
calls are synchronous; cancellation is checked before/after the snapshot read
and throughout processing, not inside a reader's read call.

## Run

Use the updated installed package, NumPy, a pinned slide and empty output paths:

```powershell
python -I examples/region_workflow.py C:/data/CMU-1.svs C:/data/large-study --roi 29185 2048 512 512 --whole-slide-tissue
```

Optional `--tissue-level 2 --tissue-memory-mib 128` selects the level/budget
explicitly; defaults choose the coarsest native level and128MiB. High-resolution
ROI statistics remain limited to1048576 pixels. Outputs retain both objects,
ROI measurements and tissue components, CSV files and verified saved replay.
Input formats without a native level above zero are rejected clearly.

In ImGui select a whole-slide rectangle, choose tissue level2, enable **Stable
pyramid snapshot**, set **Tissue memory (MiB)** to128 and run/keep tissue. Export
the tissue measurement table and save. For this slide, raise the overview source
RGBA budget to32MiB to show its coarsest level. This is a separate display budget.
Python `GuiOptions` exposes the same mode, level and work-byte settings.
The batch CLI accepts `--kind tissue --level 2 --tissue-canonical-pyramid
--tissue-work-bytes 134217728`; Python batch uses the same `TissueParameters`.

## Evidence

- [Canonical partitions/oracle/cancel/resource receipt](canonical-probe.json),
  [legacy phase failure](legacy-partition-probe.json),
  [decoder comparison](decoder-alignment.json); reproduction: `probe.py`.
- [Python full-slide tissue + bounded ROI/save/replay](sdk-workflow.json).
- [ImGui controls/result/CSV/save/replay](desktop-report.json),
  [captured overlay](desktop.png); reproduction: `desktop.py`.
 688 CSV component rows account for the foreground; the standalone application
 also opens the combined Python-produced project successfully.
- [Seven affected native suites](native-regressions.log).
- [Focused Python run](python-regressions.log):120 passed,8 skipped, one failed
 fixture-path assumption; [corrected single-case rerun](python-boundary-repair.log)
 passes. No native code was changed to hide that failure.
- `legacy.qpxproj` and [baseline](legacy-boundary.json) were produced by published
 source6219a08f. Snapshot charge11489 includes an88-byte project path. At the
 same relocated111-byte path both old and new binaries charge11512. The test
 adjusts only that documented path charge and checks admission/rejection exactly
 plus unchanged serialized mask SHA256.

This is selected engineering evidence on Windows x64, not biological acceptance,
general hardware/platform support, full feature parity or speed superiority.

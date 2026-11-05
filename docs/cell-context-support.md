# Nominal cell source-support versioning

R6-CONTEXT1 corrects eligibility at signed nominal boundaries. This is a sampling
contract change, not a new detector or a change to nominal RGB pixel bytes.
The active implementation/checklist is [current-task.md](current-task.md).

## Input contract

Keep ROI geometry, physical units, output lattice and detector arithmetic as
specified by NominalCellSampling/GridCellParameters. For new nominal requests,
source support is exactly the existing RGB sampler point phase's valid flag.
Padding is opaque black and cannot be identified using output alpha or RGB.
Only supported samples may enter the detector/feature domain. ROI membership
still uses its existing geometry policy; support does not replace that test.
Strict rejection of nonopaque source pixels remains unchanged.

Use one validated support phase per private grid. Its construction performs no
source-window scan or pixel IO; each point query is constant work. Actual pixel
reads still use the existing window/halo planner. Reuse the same phase authority
in both paths, including IntegerOrigin and AffineInverse coordinates.

## Retained identity and compatibility

The implementation changes these paths together:

- Private grid policy distinguishes legacy binary64-center support from exact
  RGB-phase support. Legacy area grids are unchanged.
- Corrected nominal input formats are `qpx.nominal-cell-rgb/2` and
  `qpx.nominal-affine-cell-rgb/2`. Existing `/1` formats retain legacy support.
  These strings participate in the existing input hash. Unknown format/policy
  combinations must be rejected at restore/encode boundaries.
- QCL9 denotes exact nominal support and uses QCL8's parameter/sampling layout.
  QCL5-8 retain their meanings and original reencoding version selection. New
  grid records must recover policy from retained input identity when encoding;
  reconstructing only NominalCellSampling would lose it.
- CellRequest.replay reconstructs the saved policy explicitly; it must not use
  the fresh-request default. Verification compares input policy as part of
  request identity, before comparing numerical results.
- ObjectCellFeatureSource retains nominal_exact_support identity in addition to its
  descriptor, algorithm and input hash. The new per-origin flag distinguishes the input-format
  version. Preserve identity in feature capture, validation, memory accounting,
  archive roundtrip and row/source matching.
- QCA9 adds an origin kind5 for corrected nominal sources, using kind4's extended
  parameter/sampling layout. Keep kinds0-4 unchanged so mixed legacy/corrected
  origins remain representable. Select version9 only when kind5 is present.
  A classifier container version alone must not upgrade every origin.
- CSV already emits the result input format; consumers use that native identity
  without duplicating support math. Expose classifier support identity to Python
  only through the existing owning manifest interface if needed for inspection.

The codec trace covers dispatch, encoding, decoding, origin capture and saved
request reconstruction. Installed consumer verification is tracked on the card. Public processing-policy enums need no new value; input
eligibility and detector arithmetic remain distinct.

## Decisive engineering expectations

Uniform opaque white8x8 has zero optical density. Use in-image ROI(0,0,8,8),
explicit extent(-4,0,12,8), step1.6, context0/2. Corrected support plus existing ROI
membership yields20 eligible core samples and zero cells. At step.4 and explicit
extent(-1,0,9,8), expect380 samples and zero cells. Context does not change either
core result. Test affine coordinates separately against their actual sampler
phase; do not assume all signed-origin phases coincide.

Pinned [legacy fixtures](../tests/fixtures/cell-context-legacy-v1/manifest.json)
were generated with installed2892db74. Both contexts retain1 false cell/25samples
and their original masks, measurements and input hashes. This known-wrong result
is required only for compatibility. Fixture hashes, original replay and
save/reopen/replay must remain exact; never regenerate them using corrected code.
New records must roundtrip with the corrected policy, including classifier
origins and mixed source versions. Existing nominal pixel/halo checks are reused.

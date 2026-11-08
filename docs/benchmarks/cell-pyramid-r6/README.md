# R6-SAMPLING1: nominal cells on lossless TIFF pyramids

Locally verified with installed **faa71c1fa6eb9e5bb6eab1ce11cb8389497d47d7**.
No native code, reader, codec or sampler change was necessary. This completes the
named lossless uint8 RGB TIFF SubIFD workflow; R6/M0-M7 remain Partial. It does not
establish vendor-wide, fluorescence/raw-channel, biological, resource or speed
acceptance.

The [retained recipe/runtime](recipe-and-runtime.json) reuses the pinned
nominal-cells-v1/source.rgb image and its existing provenance. The generated
BigTIFF has Deflate16x16 tiles, contiguous or separate RGB planes and two levels.
Level0 contains the retained256x192 image; level1 is deliberately uniform white
at128x96. An unintended preview-level substitution cannot satisfy the original
RGB,379-nucleus-pixel/825-body-pixel and three-cell oracle.

Nominal requests at downsample1.5 exercise IntegerOrigin/AffineInverse and
context0 or3/4. ROI(0,0,256,192) and.5um square calibration are declared explicitly.
TIFF versus dense RGB, masks, all retained compartment RGB/OD means and calibrated
sample areas agree exactly. Each reader's own input hash remains stable across
linked project save/reopen/replay; no equality between distinct reader identities
is assumed. The saved image remains a native TIFF link, not an embedded replacement.

| Boundary | Evidence |
| --- | --- |
| Installed SDK and native CLI | [Ten passes/one desktop skip](pyramid-sdk.xml): eight layout/coordinates/context combinations plus both layouts through CLI batch/checkpoint/resume. CLI fresh saves then reuses the unchanged checkpoint |
| Installed ImGui | [One selected pass](pyramid-gui.xml): separate RGB tiled pyramid, context3/4, detect/keep/recheck with matching masks, features and calibrated areas |
| Source/pixels/product | [Recipe/runtime](recipe-and-runtime.json) pins source pixels, test source and native binary, and reuses [CONTEXT1 package integrity](../cell-context-r6/integrity.json). Runtime/source and native hash match the already installed wheel |

The first [observer failure](pyramid-initial.xml) occurred after the pixel/cell/
feature/save assertions: it guessed ImageServer.reader, which the SDK does not
expose. The repair uses actual restored tiff_view/level metadata plus the project
image reader contract. GUI uses its existing identity transform; CLI follows the
existing terminal-JSON parsing pattern. No production guard or numerical
assertion was weakened, and the failed log is preserved.

Waste review: **reuse** retained pixels, expected masks, native TIFF/nominal
implementation, installed environment and source/package evidence. **Remove**
new corpus/decoder work, rebuilds, wheels and installations for this integration
coverage. Run only these ten SDK/CLI cases and the one actual UI case; do not
repeat CONTEXT1's whole campaign. Zero builds, stub generations or installs.

Reproduce using installed Python with
`-m pytest tests/test_nominal_cell_tiff_pyramid.py`; the desktop node requires
`QUPATHXX_TEST_GUI=1`. XML and raw logs retain the actual selections/results.
The tests generate temporary TIFF/project files from the pinned pixels, avoiding
another stored image corpus.

Next: R6-FEATURE1 traces retained compartment measurements and selects a concrete
missing definition against the pinned reference. Preserve existing means and
legacy records. Broader vendor/context/split-merge and specimen acceptance remain
in R6, with resource and speed claims in R12.

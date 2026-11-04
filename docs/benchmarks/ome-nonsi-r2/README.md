# R2-UNIT2 remaining physical OME length units

Native source `da240fc5248d905ff55fdbd7397cba3469c80998` extends the existing
TIFF quantity parser with `in`, `thou`, `li`, `ft`, `yd`, `mi`, `ua`, `ly`, `pc`
and `pt`. SI/legacy branches and arithmetic remain unchanged. Length factors
do not become time units; units remain case-sensitive. Positive-finite admission,
nonphysical pixel/reference-frame absence and raw stored samples are preserved.
Primary and companion metadata use the same conversion authority.

The [OME unit definitions](https://docs.openmicroscopy.org/ome-model/6.3.1/developers/ome-units.html)
identify these convertible lengths. Expected conversion factors were captured
from the actual OME XML6.5.1 JAR in the verified pinned QuPath0.7.0 runtime,
using [the probe](../../../tools/upstream/ome_length_units.groovy).
The [reference/manifest](../../../tests/fixtures/ome-units-6.5.1/manifest.json)
retain JAR, runtime, probe and output identity, with a predeclared relative
tolerance of2e-15. The parsec factor retains OME's approximation; it does not
silently substitute a more precise astronomical value. Independent decimal
definitions also check the captured reference.

Evidence:

- [Before](before.xml): all ten added conversion cases fail on the previous
  installed BinaryOnly reader, with absent calibration.
- [Installed reader checks](reader.xml):380 pass, six declared desktop skips.
  All114 calibration cases pass, including31 new reference/conversion,
  quantity-domain, exact-symbol, boundary and companion-normalization cases.
  The exact current OME admission is215 installed passes and six declared
  desktop skips; relevant fixture, source/fault and admission controls also pass.
- [Native](native.xml): raw_image and source_identity pass.
- [Identity](validation.json): all434 compiler inputs are fingerprinted; only
  cpp/tiff_server.cpp changes relative to R2-META1. All59 installed package
  members match the wheel, stubs match source and public signatures are unchanged.
  Prior products were checked before cache reuse; current native/object/test
  products are retained locally with hashes.
- The first [reference probe](reference.log) failed because Groovy JSON was
  unavailable. The [successful probe](reference-final.log) uses bundled Gson;
  neither attempt required a new runtime dependency. Both outcomes are retained.

Reproduce the reference with the pinned console executable: `script --args
OUTPUT_DIRECTORY tools/upstream/ome_length_units.groovy`; verify the runtime
with tools/fetch_qupath.py first. Reader/native command JSON files record actual
test invocations. Build selected the existing six package/runtime/test targets
once after code settled, without ALL_BUILD, stub regeneration or a platform
campaign. The wheel and build cache remain ignored local artifacts.

Waste review: shared parser, existing SI/companion fixtures, package cache and
admission contract were reused. Avoidable cost was the failed probe's unavailable
JSON import; use the bundled serializer for subsequent QuPath captures. Test
duration was13.65s and native duration0.68s; no overall time-saving claim is made.
No passing checks were repeated after the successful installed run. Future unit
work extends this module/contract; it does not create a second calibration suite.

Next: R2-CODEC1, JPEG-compressed quantitative TIFF. The current CMake reader
configuration forces jpeg/old-jpeg off, and the quantitative contract explicitly
rejects JPEG. Confirm an openly licensed pinned codec/dependency route and an
independent scalar/RGB fixture before changing it; account for symbol/runtime
coexistence with OpenCV/OpenSlide. Other codec/photometric forms remain explicit
separate gaps. This is local Windows engineering evidence; all M0-M7 remain
Partial and source308 retains only its original platform acceptance.

# R2-SI1 OME calibration unit conversion

Native source `88a0053069111a3920a7ea6544b1f04147145f53` extends the existing
TIFF quantity parser for OME2016-06 SI-prefixed metres/seconds and day intervals.
Main and companion metadata share that parser. Public APIs, schema layouts and
stored samples are unchanged. Existing supported conversions/defaults retain
their binary64 arithmetic. Unknown/pixel/reference-frame units and invalid,
overflowing or zero-underflow values remain absent; representable positive
subnormal calibration is retained.

The [OME unit system](https://docs.openmicroscopy.org/ome-model/6.3.1/developers/ome-units.html)
defines the canonical symbols and prefix range. The
[independent tests](../../../tests/test_ome_calibration_units.py) compute SI
expectations using Decimal powers and compare legacy conversions exactly.

- [Before](before.log): the installed307 baseline loses picometre calibration;
  the expected regression fails (one selected case,82 deselected).
- [Installed reader checks](reader.xml):183 pass, including all83 new unit cases.
  Two unchanged compound-desktop cases have explicit opt-in skips.
- [Native raw-image suite](native.xml): passes in the TIFF-enabled build.
- [Validation identity](validation.json): actual installed native/wheel hashes,
 434 compiler inputs with only `cpp/tiff_server.cpp` changed,59 installed members
 matched byte-for-byte, unchanged generated stubs and retained tested products.

One incremental build/install was used. No full platform campaign, stub
regeneration or repeated GUI/numerical workflow qualification was needed.
This is local reader engineering evidence, not broader OME/BinaryOnly support,
biological acceptance, general speed or a supported release claim. The earlier
308 platform acceptance retains its own frozen source/scope.

Waste review: reused existing decoder, metadata parser, fixture generator,
build cache and installed-test dependencies. The inherited local ALL_BUILD
selection unnecessarily refreshed unrelated test targets; subsequent local
builds should name package/runtime and affected test targets explicitly.
Next bounded task: R2-META1, explicit BinaryOnly metadata companions with bounded
IO, complete source provenance and change rejection, through the current reader.

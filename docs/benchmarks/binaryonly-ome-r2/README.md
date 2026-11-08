# R2-META1 BinaryOnly OME metadata companions

Native source `4e7b6e8dc0907c1c3e1603ef5beee34f3759fa98` supports BinaryOnly
TIFF headers referring to a full XML metadata companion or master TIFF.
Observer `51b5172570ad503b4236ec899846f1e47c7c9f1c` records the final independent
fixtures and admission tests. Only `cpp/tiff_server.cpp` changed among the434
compiler inputs relative to R2-SI1; the public API, generated stubs and existing
source/archive schema layouts are unchanged.

The [OME specification](https://docs.openmicroscopy.org/ome-model/6.2.2/ome-tiff/specification.html#partial-ome-xml-metadata)
defines the metadata-file/UUID relationship. The reader verifies the partial
header and master UUIDs, resolves paths against the containing metadata file,
and includes external XML in the same observed/guarded fileset as pixel files.
Masters require full Image metadata; missing/ambiguous/recursive references,
unsafe XML declarations, physical aliases, malformed/incomplete mappings and
oversized inputs reject. UUID-only directory discovery remains unsupported.

Limits remain64 total files (including primary and metadata),1 MiB per XML,
4 MiB aggregate XML and the existing catalog/mapping/cache/decode budgets.
Metadata reads are chunked with cancellation checks, opening guards verify
before reader publication, and operation guards include metadata even for
cached pixels, outside-image requests and new projections. No new decoder,
metadata service, source codec or per-frontend reader was introduced.

Evidence:

- [Before](before.xml): three independent fixture checks pass; all six BinaryOnly
  entry cases fail on the old reader. Physical stored samples are verified
  independently through tifffile against the scalar coordinate formula.
- [Combined targeted checks](combined.xml):355 distinct final cases pass across
  reader, source/fault, CLI, project, UI and evidence-control runs. This includes
  all51 BinaryOnly cases,83 unit cases and the exact declared OME cohort of184
  installed plus six desktop cases. It is a union of named executions, not a
  claim that one full repository suite was rerun.
- [Native](native.xml): raw_image and source_identity pass. The
  [selected build targets](build-exit.json) cover package/runtime and those two
  test executables. No full platform campaign or ALL_BUILD was used.
- [Identity](validation.json): installed native/wheel hashes, all59 installed
  package members, unchanged stubs, compiler fingerprint and retained products.
  [Headless declarations](headless-desktop.xml) plus the original two headless
  desktop declarations satisfy the exact six-case skip contract.

The initial reader run retains two failed batch test expectations. Natural-grid
hashes use the saved document's effective calibration, so a file-only X-spacing
edit may leave the measured inputs equal and permit pixel-verified reuse.
Explicit-grid recipes also capture source metadata. The corrected
[four batch checks](batch-final.xml) preserve that distinction and require a
refresh for a changed time interval in both modes. No native batch behavior or
raw hash format was changed to force a pass. The same built reader was reused;
unaffected passing tests were not rerun after that test-only correction.

Waste review: reused the scalar fixture generator, file observations/guards,
source codecs, analysis/project/batch and existing desktop controls. One build
selected six targets instead of ALL_BUILD. Only changed or previously uncovered
checks were added; desktop capability is checked before generating fixtures.
The existing OME admission list owns the new cases; named collection and module
coverage replace the old fixed total, with omission/false-acceptance controls.
No separate qualification or efficiency framework was added.

This is local installed Windows engineering evidence. Biological accuracy,
whole-slide budgets, broader format/color coverage and supported release remain
open;308 acceptance retains its own source. Next: R2-UNIT2, establish the remaining
convertible non-SI OME length-unit factors from the pinned reference before
extending the same quantity parser. Pixel/reference-frame units remain nonphysical.

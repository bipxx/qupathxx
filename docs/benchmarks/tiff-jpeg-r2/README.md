# R2-CODEC1 JPEG-compressed quantitative TIFF

Native source `e97d46277f292c8314e92043610b093cb1446fe3` adds the pinned
libjpeg-turbo3.1.3 codec through the existing libtiff reader. Observer
`c3dd9ce46c2a9651db63913bd55762c43ec66abf` adds mandatory package notice checks.
Only cmake/TiffReader.cmake and new cmake/TiffJpeg.cmake differ among435 compiler
inputs from R2-UNIT2; there is no second reader, new public signature or stub change.

The source archive SHA256 is
`3a13a5ba767dc8264bc40b185e41368a80d5d5f945944d1dbaa4b2fb0099f4e5`.
[Upstream licensing](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/3.1.3/LICENSE.md)
and [IJG terms](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/3.1.3/README.ijg)
are retained byte-for-byte in the package, alongside attribution.
This software is based in part on the work of the Independent JPEG Group.

JPEG code builds as a separate upstream CMake project, static/PIC, with hidden
ELF/Mach-O symbols; no upstream files are patched. The selected OpenCV core/ml
SDK disables JPEG. JPEG::JPEG carries the generated header and build-order
dependency. Its pinned API supplies both8/12-bit entry points, allowing libtiff
configuration before the separate archive exists; successful linking exercises
that dependency. The reader's existing byte-aligned type contract remains:
this slice establishes8-bit scalar/RGB JPEG, not12-bit sample support.

Independent [fixture generation](../../../tools/reference_jpeg_tiff.py) builds
classic TIFF tags/offsets and Pillow JPEG streams without native qupathxx/libtiff
or an imagecodecs dependency. Expectations come from decoded lossy streams;
Pillow's TIFF decode separately verifies the assembled files. Cases cover strips,
tiles/partial edges, scalar/RGB, full streams/shared JPEGTables, big-endian
MINISWHITE, cached raw/display reuse, padding/array ownership, malformed JPEG,
decode quota, OME calibration, project analysis/replay and CLI NumPy export.

Evidence:

- [Before](before.xml): eight independent fixture checks pass and eight native
  JPEG reads fail on the preceding installed reader.
- [Installed](reader.xml):409 pass, six declared desktop opt-in skips. All22 new
  JPEG cases pass. Exact OME admission is237 installed cases with six declared
  desktop skips; no current desktop validation is claimed.
- [Native](native.xml): raw_image and source_identity pass.
- [Coexistence](coexistence.log): native JPEG read, OpenSlide vendor JPEG versus
  its independent binding, and OpenCV forest train/save/reload/apply execute in
  one interpreter. The two existing selected workflow cases pass.
- [Linux codec smoke](linux-summary.json): WSL Ubuntu/Ninja builds the same
  CMake JPEG/TIFF dependency and decodes eight fixtures exactly. Linked JPEG
  functions are local, verifying nonpreemptible codec symbols. The small
  [harness](linux-smoke/CMakeLists.txt) is a codec build/link check; it does not
  build or qualify the qupathxx Linux app/Python/GUI stack.
- [Identity/notices](validation.json): compiled source/fingerprint, native/wheel
  hashes, all62 installed package members, exact upstream notices and unchanged
  stubs. Seven binary/object/archive products remain retained locally with hashes
  and their actual cache paths. Future cache-preservation checks use cache_path,
  rather than assuming every artifact lives under Release.

One cached six-target Windows package build/install and one Linux codec-only
build were needed. [Commands](linux-commands.json) retain the exact Linux scope;
Windows reader/native command JSON files record their targeted invocations.
[Windows build output](build.log) retains upstream's missing-NASM warning:
these local x86 builds use the C fallback, so no JPEG SIMD or speed advantage is
claimed. Enabling/measuring SIMD belongs to an identified R12 resource workload.

Waste review: kept the shared reader and codec authority, reused installed
consumer/controller paths, and added no public API or test-only imagecodecs
dependency. The first fixture approach requested imagecodecs, so a small
independent tag writer replaced it. The first Linux harness checked a libtiff
child-directory variable at parent scope and failed despite enabled JPEG;
its [failed configure](linux-00.log) is retained. Checking the actual generated
header fixed only the harness; production code and passing Windows tests were
not rebuilt/rerun. Reader time12.42s; native time0.57s per its log. No full
platform campaign, per-frontend numerical campaign or overall speed claim.

Next: R4-MERGE1 true object-merge workflow. QuPath0.7.0 mergeAnnotations removes
the merged inputs, creates/selects a new annotation and applies a common class.
The current apply_boolean intentionally retains operands and target identity,
so that accepted operation does not supply the distinct merge workflow. Capture
the pinned upstream metadata/hierarchy behavior first, then reuse immutable ROI
union, commands, selection and persistence for atomic merge/undo/redo. Add only
the missing operation and affected consumer checks. Other reader/photometric/
codec gaps remain R2; this codec slice does not close R2 or any M0-M7 milestone.

# Native JPEG plugin: local Windows delivery

The selected reader is a separate native image module, `org.qupathxx.jpeg`
version1.0.0, contract `jpeg8-oriented-decoded-samples/1`. The
[profile/build/use guide](../../../plugins/jpeg/README.md) defines its supported
interpretation and refusals. It uses the existing C ABI, saved-source and
scientific authorities. Host, engine, bindings, PNG and GUI products stay frozen.

## Exact candidate and evidence

Application baseline is main `7c981bf9`; retained engine/SDK source identity is
`bc8ca7384be33c6308130affeec66e8d84cf8b40f2bfaf91dc244651f57990eb`.
The JPEG product-input context is
`9b5318a16f1be46a748fc393296815f7454dba02ccdaa1bcf322c94df0cb5790`.
It binds the adapter/decoder/memory backend, CMake/profile/manifest recipe and
exact libjpeg source lock independently of the engine identity.

The final DLL was built from the admitted source kit outside the checkout,
with tests OFF and no SDK/core discovery or link. Its SHA-256 is
`e43f57501ba6664958ddcdb8163ec76effa3ff290c8bafc7747f4d560ad4362b`.
Those new bytes received their own complete native validation; earlier binary
results were not transferred. The runtime manifest binds its actual DLL hash.

Workspace evidence is in `build/jpeg-reader/`: `preparation.json`, `before.json`,
initial/corrected build logs and source snapshots, `negative-open-initial.json`,
`relocation.json`, `final-inputs.json`, `validation-final-μ-图/validation.json`,
package admission reports and `results.json`. Logs retain their actual execution
paths; the final record maps the retired outside comparison directory.

## Interpretation and decisive checks

RAW means canonical decoded UInt8 Gray/RGB from a lossy input, never recovered
precompression samples. Display is separate oriented RGBA8 with opaque in-image
alpha. Outside-image samples are zero/invalid. EXIF1–8 transforms raw/display,
dimensions and calibration axes consistently. Explicit rational EXIF or JFIF
inch/cm density establishes physical scale; unknown units do not establish it.
Conflicts are compared exactly before conversion to micrometers.

| Check | Result |
| --- | --- |
| Supported decoding |40 exact profiles: Gray/RGB/YCbCr, baseline/progressive, three subsampling modes and valid restart markers |
| Independent pixels | Pillow13.0.0/libjpeg-turbo3.2.0 decoded/oriented oracle matches pinned3.1.3 product exactly; four analytic DCT anchors include unequal RGB64/128/192 |
| Geometry/calibration | Both TIFF byte orders, EXIF1–8, rational and JFIF calibration, axis swaps, one-axis and unknown-unit cases |
| Buffer/ABI behavior | Exact raw/display crop, padding, channel subset, canaries, packed-layout refusals, structure/text sizes and invalid requests |
| Refusals |29 exact statuses: corrupt/truncated/concatenated/trailing input, warning recovery, wrong restart, non8-bit/lossless/arithmetic, CMYK/YCCK, ICC/XMP/MPO and malformed/conflicting/unsupported metadata |
| Work/cancel/Unicode | A valid12450-byte JPEG expands to3MiB raw and refuses a32KiB decoder-work allowance with no handle; before/during cancellation and UTF-8 source/directory paths pass |
| Native consumers | Eight ABI/SDK/CLI routes: attach RGB/Gray, exact raw/display/calibration, native RegionAnalysis Keep/Save/reopen/replay/resave, offline provider, changed module/source and host catalog/region |
| Exact identity | Equal samples from changed DLL bytes still require implementation review and do not authorize an old retained digest; unchanged relocated module preserves identity |

The offline test uses valid display/raw requests and requires the actual
provider-unavailable diagnostic. The changed-source test runs the same valid
requests successfully before mutation and requires `WorkflowInputChanged`
afterward. Historical values, result IDs and digests stay exact.

The root handoff also checks unchanged built-in/family catalogs, P6 input,
simultaneous PNG/JPEG catalog and exact regions, cross-location saved-project
reopen and incorrect module-hash refusal. It preserves all56 frozen inputs:
14 archives,38 binding objects, region host, SDK core, PNG DLL and PNG manifest.

## Source, licenses and package

The local Windows x64 MSVC1944/compiler19.44.35225.0 Release `/MD` package has
199 exact admitted members, including a complete selected source kit. The
171 unmodified upstream code/header/template/build/notice members match the
already admitted libjpeg-turbo3.1.3 archive SHA-256
`3a13a5ba767dc8264bc40b185e41368a80d5d5f945944d1dbaa4b2fb0099f4e5`.
No upstream test image, SIMD implementation, Java, TurboJPEG, optional tool,
model, specimen or restricted derivative is included. The codec's standalone
static build has SIMD/arithmetic/tools/tests/Java/fuzzing disabled.

Adapter code remains AGPL-3.0-only. Upstream applicable IJG/BSD-3-Clause/Zlib
terms, full `LICENSE.md`, `README.ijg` and per-file notices retain their identities
and ancestry in the existing material inventory. This software is based in part
on the work of the Independent JPEG Group. Plugin isolation does not waive
upstream or adapter distribution/source obligations.

`build/jpeg-reader/package-ready` and `jpeg-reader-windows-x64.zip` contain the
same199 admitted members. Stage/ZIP checks use the existing guard and inventory.
The removal proof deletes only byte-verified package-owned files from a private
deployment copy, preserves user images/projects/results/notes and confirms offline
project access afterward. Earlier inventory groups, including PNG and frozen
release profiles, remain unchanged. The named outside build is retired into the
task evidence directory with verified source/worktree boundaries. The corrected
process-use check confirms both the original and retired paths are unused;
`retirement-check.json` records that independent final verification.

## Corrections and limits

Pre-build review moved saved-marker checks into `term_source`, before libjpeg
finalization frees their storage, and explicitly selected codec `/MD`. The first
combined build passed; all positive pixels passed before a wrong-format
`JERR_NO_SOI` error mapping failed. A thin native probe confirmed the other28
negative statuses. The affected decoder and strengthened offline test consumer
were rebuilt once, preserving original numeric and status assertions. The final
source-kit DLL then passed its own full validation. Initial failures/logs remain.
Existing admission/deployment regression tests pass18 with one Windows symlink
capability skip. A unittest invocation initially discovered zero pytest functions;
the corrected pytest invocation ran the actual suite.
The first cleanup process check contained a Python/PowerShell quoting error and
emitted nonterminating errors. A fail-closed check using character codes verifies
both paths and preserved hashes; no further move/delete or native build occurs.
The receipt/archive are refreshed from the same qualified module bytes.

This is local Windows plugin evidence. Automatic suffix routing and TIFF/Zarr
recipe admission are unchanged. It does not establish ICC/CMYK or all JPEG
encodings, broader platform support, Java/Groovy compatibility, UI completion,
full QuPath parity, biological/clinical or speed acceptance, a wheel or publication.

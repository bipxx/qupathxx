# Native PNM/PAM plugin: FP01 local Windows delivery

FP01's first raster outcome is a separately built reader, `org.qupathxx.pnm`
version1.0.0, contract `pnm-pam-uint-samples/1`. Its
[profile/build/use guide](../../../plugins/pnm/README.md) declares exact source
and display semantics. FP00's repository planning census retains
all178 upstream entries without claiming they are implemented. Other format
packets remain outstanding; this delivery does not close the full roadmap.

## Candidate and source identity

Application baseline is main `7c981bf9`; retained engine/SDK source identity is
`bc8ca7384be33c6308130affeec66e8d84cf8b40f2bfaf91dc244651f57990eb`.
The PNM product-input context is
`4e1b5928756bb3e0b3ea92de73effbf7741158ff0ba9a9a428902282f710da4b`.
It binds exactly the decoder/ABI source, CMake, compiler-profile header and
manifest recipe. Module source/qualification is independent of the engine.

The final module was built from the complete selected source kit outside the
checkout with tests OFF and no SDK/core discovery/link. SHA-256:
`22d33110476e845b22f04f7a6074ebfbbbd286583264ba10da6a86144008bc1a`.
Those installed bytes received their own full native validation; earlier binary
evidence was not transferred. The actual DLL hash binds the runtime manifest.

Workspace evidence: `build/pnm-reader/before.json`, initial/consumer-only build
logs and source snapshots, `relocation.json`, `final-source.json`,
`validation-final-μ-图/validation.json`, package admission and final `results.json`.
Actual execution paths remain in logs; final results map the retired outside
comparison into the task evidence directory.

## Scientific interpretation and checks

P1–P7 cover plain/binary PBM/PGM/PPM and supported semantic PAM tuples. Raw reads
retain unsigned codes exactly in UInt8/UInt16 native-endian HWC order. PBM
`Blackness`1=black differs explicitly from PAM Gray0=black. Binary16-bit input
is big endian; row padding is ignored. Alpha0 retains source samples and hidden
display RGB; in-image validity stays1 and out-of-image samples are zero/invalid.

Default BT.709 source interpretation converts Gray/RGB to sRGB display RGBA8.
Explicit sRGB/linear variants affect display only; alpha is linear opacity and
raw values remain exact. Native source options and their identity are persisted.
The host represents unknown X/Y/Z/time calibration as absent optional values;
the plugin does not infer scale from comments or dimensions.

| Decisive check | Result |
| --- | --- |
| Format/sample profiles |79 positive profiles cover P1–P7, packed/plain bits, opposite PBM/PAM polarity, nonfull maxvals, all six canonical PAM tuples, alpha and UInt16 byte order |
| Independent expectations | Authored original arrays, literal transfer/half-up anchors, Decimal70 normative transfer math and Pillow13 raw decoding for representable cases; no tolerance relaxation |
| Region/ABI behavior | Exact raw/display, crop/padding/channel slice, canaries, text/structure sizes, packed layout, coordinate overflow and invalid requests |
| Strict input refusals |36 exact statuses for malformed/truncated/overmax/signed/float samples, extra/concatenated data, unsupported tuple/field/magic and64KiB header bounds |
| Delimiters/metadata | Raster-leading whitespace/# bytes preserved; exact one final delimiter, no guessed extra CR/LF byte; PAM repeated tuple meaning and LF/CRLF lines checked |
| Work/cancellation/Unicode | Normal decode verifies an8,203-byte P4 with327,680 raw+RGBA bytes; a32KiB work budget then refuses actual decoded expansion with ample path/wrapper margin. Synchronous cancellation and Unicode source/directory paths pass |
| Native consumer routes | Nine ABI/SDK/CLI routes: RGB and original16-bit gray attach/read, RegionAnalysis Keep/Save/reopen/replay/resave, missing provider, changed module/source/options and host catalog/region |
| Implementation/options | Changed module bytes with equal pixels require review and invalidate old result reuse. Changed manifest defaults preserve saved explicit BT709; fresh sRGB attachments retain raw codes but acquire distinct display/result identity |

Offline checks use valid requests and require the native provider-unavailable
diagnostic. Changed-source checks first prove the exact same requests work,
then require `WorkflowInputChanged` after mutation. IDs, historical values and
input digests stay exact throughout.

## Packaging and preservation

The local Windows x64 MSVC1944/compiler19.44.35225.0 Release `/MD` package contains
20 admitted members: module, manifest, exact selected adapter/test/control sources,
ReaderPluginKit, full AGPL text and component/build provenance. The module has
platform runtime dependencies and no external decoder or application-core link.
The independent decoder and authored fixtures retain AGPL-3.0-only. Published
Netpbm specifications are references; their licenses are not reassigned and no
upstream implementation, sample image or restricted material enters this package.

The stage and ZIP share20 exact admitted members through the existing material
guard/inventory. Root handoff checks coexistence of PNG/JPEG/PNM, unchanged
built-in image/matrix/annotation catalogs, P6 behavior and bad-hash refusal, plus
cross-location project replay. Removal deletes only byte-verified package-owned
files from a private deployment, preserves user images/projects/results/notes and
checks offline reopen afterward. Earlier inventory groups remain unchanged.

All58 frozen native/PNG/JPEG inputs retain exact hashes:14 archives,38 binding
objects, host/SDK core and both earlier plugin DLL/manifest pairs. The named
outside build is retired only after absolute path, source/worktree and fail-closed
active-process checks. No engine, GUI, binding or wheel build occurred.

## Corrections and remaining scope

Pre-build review aligned descriptor/manifest naming, global header bounds and
allocation-failure status; budget fixtures isolate decoded expansion and source
options follow existing native authority. Initial compilation and all direct
pixel/refusal cases passed. The first SDK consumer incorrectly compared unknown
calibration optionals to0; only that consumer was corrected to require absent
X/Y/Z/time, then rebuilt once. Every sample/type/identity assertion remains.
Initial failures and source snapshots are retained.

Automatic suffix routing, TIFF/Zarr-specific recipe admission and the separate UI
workflow retain their existing scope. BMP, WebP, scientific/store/family/writer/
remote packets and the full format-roadmap goal remain unfinished. No new
platform, biological/clinical/speed claim, source commit or publication is made.

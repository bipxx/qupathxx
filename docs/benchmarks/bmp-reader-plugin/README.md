# Native BMP plugin: FP01 local Windows delivery

`org.qupathxx.bmp` version1.0.0, contract `bmp-infoheader-uint8-samples/1`,
addresses the ordinary BMP gap demonstrated by FP00. Its
[profile/build/use guide](../../../plugins/bmp/README.md) defines the supported
headers, stored samples, calibration and refusal boundaries. Other roadmap
packets remain outstanding; this delivery does not close the full format goal.

## Candidate and source identity

Application baseline is main `7c981bf9`; retained engine/SDK source identity is
`bc8ca7384be33c6308130affeec66e8d84cf8b40f2bfaf91dc244651f57990eb`.
The BMP product-input context is
`d6af1e3d25b12b2aa14505948dc0a08d408f01f656828d3aa2e75e05bcf93a61`.
It binds the decoder/ABI source, CMake, compiler-profile header and manifest
recipe. The module's source/qualification is independent of the engine.

The final module was built from the selected source kit outside the checkout,
with tests OFF and no SDK/core discovery/link. SHA-256:
`d479d459e76e03208c01b161064edbd7679ed48994a2f0780327491b43bfc209`.
Those installed bytes received their own complete native validation; earlier
binary evidence was not transferred. The manifest binds the actual DLL hash.

Workspace evidence: `build/bmp-reader/before.json`, initial build/source logs,
`relocation.json`, `final-source.json`, `validation-final-μ-图/validation.json`,
package admission and final `results.json`. Original paths remain in the logs;
results map the retired outside comparison into the task evidence directory.

## Scientific interpretation and decisive checks

The selected profile is BM plus exactly40-byte BITMAPINFOHEADER, planes1 and
BI_RGB. Indexed1/4/8-bit input retains literal UInt8 palette indices; RGB24/32
retains exact RGB codes after BGR channel ordering. The32-bit X byte is unused,
never alpha. RGBA display uses original palette/RGB codes and alpha255. No gamma,
ICC or source-alpha inference occurs. In-image raw validity is1; outside padding
is zero/invalid and display padding transparent black.

Top-down/bottom-up files produce canonical upper-left coordinates. Positive
pixels-per-meter fields independently supply `1000000 / ppm` micrometers per
pixel; zero remains unknown. SDK unknown calibration optionals stay absent.

| Decisive check | Result |
| --- | --- |
| Format/sample profiles |62 exact profiles:1/4/8/24/32-bit depths, top/down row order, DWORD and packed-bit/nibble padding, compact/full palettes, RGB and independently present X/Y densities |
| Independent expectations | Independently authored original RGB/index arrays and byte encoder; Pillow13 verifies every supported display raster exactly; raw indices remain the original codes |
| Alpha/metadata meaning | X=0/17/255 always produces alpha255; palette reserved bytes refuse; no synthetic alpha/channel or unknown calibration |
| Region/ABI behavior | Exact raw/display, crop, padding, channel slice, canaries, descriptor/text/structure sizing, packed layout, coordinate overflow and invalid requests |
| Strict refusals |40 exact statuses cover malformed/truncated headers/palette/raster, offsets/gaps/trailers, invalid indices/density/planes/file sizes and unsupported DIB/depth/compression/optimization palettes |
| Resource/cancel/Unicode | Normal open verifies an8,254-byte indexed BMP with327,680 raw+RGBA bytes;32KiB work then refuses decoded expansion with ample path/wrapper margin. Cancellation and Unicode source/directory paths pass |
| Native consumer routes | Eight ABI/SDK/CLI routes: RGB/index attach/read, RegionAnalysis keep/save/reopen/replay/resave, missing provider, changed implementation/source and host catalog/region |
| Exact identity | Equal pixels from changed DLL bytes still require implementation review and a different result-input digest; source mutation refuses currentness while preserving exact historical results |

Offline reads use valid native requests and require the actual provider-unavailable
diagnostic. Source-change checks prove those same requests work before mutation,
then require `WorkflowInputChanged`. Stored IDs, means and digests remain exact.

## Packaging and preservation

The local Windows x64 MSVC1944/compiler19.44.35225.0 Release `/MD` package contains
20 admitted members: module, manifest, exact adapter/test/control sources,
ReaderPluginKit, full AGPL text and component/build provenance. The module uses
the platform runtime and no external decoder or application-core library.
Independent source and authored fixtures retain AGPL-3.0-only. Microsoft format
definitions are references; no upstream implementation/sample image is packaged,
and the adapter's license is not assigned to those specifications.

Stage/ZIP checks bind the same20 members through the existing material inventory.
Handoff checks cover BMP/PNM/PNG/JPEG coexistence, unchanged built-in image/matrix/
annotation catalogs, built-in P6 behavior, bad-hash refusal and cross-location
project replay. Removal deletes only verified package-owned files from a private
deployment, preserves user images/projects/results/notes and checks offline
reopen. All earlier material groups remain unchanged.

All60 frozen native/PNG/JPEG/PNM inputs retain exact hashes. The named outside
build is retired only after absolute path, source/worktree and fail-closed active
process checks. No engine, GUI, binding or wheel build occurred.

## Corrections and remaining scope

The first compilation and all62 positive profiles passed. A short-header fixture
had declared DIB size0, correctly reporting unsupported instead of its intended
truncation error. Only that fixture was corrected to a truncated declared40-byte
header with accurate file size; expected I/O4 remains unchanged. The same module
then passed every refusal/integration case. No native or C++ test rebuild was
needed; the failed evidence is retained. A read-only evidence lookup used the
wrong fixture JSON filename, then was corrected from the runner's actual path.

Automatic routing, TIFF/Zarr-specific recipe admission and separate UI work keep
their existing scope. WebP, additional scientific/store/family/writer/remote
packets and the full format goal remain unfinished. No new platform, biological,
clinical or speed claim, source commit or publication is made.

# Native PNG plugin: local Windows delivery

The selected PNG reader is implemented as an independently built native module,
`org.qupathxx.png` version1.0.0, contract `png-static-native-samples/1`.
Its [profile/build/use guide](../../../plugins/png/README.md) defines the exact
supported PNG interpretation. It uses the existing host C ABI and saved-source
authorities; application, engine, Python bindings and GUI sources are unchanged.

## Candidate identity

Base application source is main `7c981bf9`; its retained engine/SDK identity is
`bc8ca7384be33c6308130affeec66e8d84cf8b40f2bfaf91dc244651f57990eb`.
The plugin product-input context is
`904bf18964d0a5b2434ea2b12b5ba0f4ce522a6c7f9e5639561e8dc2206d170d`.
It binds the module/decoder sources, CMake/profile/manifest recipe and exact
libpng/zlib locks, separately from the host's scientific identity.

The final module was rebuilt from the staged code-only source kit outside the
application checkout, using the relocated ReaderPluginKit and no core-library
discovery/link. Its SHA-256 is
`098cb3a15d3e7e5811498023515c41c71404170c6bbb675ef7681483491bfbbf`.
It received its own full validation; an earlier in-checkout binary's passing
results were not transferred. The module's manifest records its actual bytes.

Workspace evidence is under `build/png-reader/`: `before.json`, build logs and
source snapshots, `relocation.json`, `validation-final-μ-图/validation.json`,
package admission manifests and the final `results.json`. Paths in retained logs
describe the actual execution locations; retired comparison paths are mapped by
the final handoff record.

The existing admission/deployment regression suites pass25 tests; one existing
Windows symlink-capability test skips because this host cannot create its link.
All36 selected zlib files also match the already admitted source archive exactly;
`zlib-origin.json` records the archive SHA-256 and member comparison.

## Result fidelity

All legal static PNG color/depth combinations, including Adam7, are supported.
Raw reads preserve source scalar values: sub-byte gray/index values are unscaled
UInt8, palette samples remain original indices, and16-bit samples remain UInt16
in native-endian HWC order. An alpha-zero source pixel still has valid source
samples. Outside-image samples are zero with validity0.

Display uses a distinct straight-alpha RGBA8 representation: low-bit gray scales
to255,16-bit values use their high byte, and palette/tRNS interpretation changes
display only. Explicit pHYs meter units calibrate each positive axis; unknown
units never invent a physical scale. The plugin refuses animation, ICC/Exif,
non-sRGB gamma/chromaticities and unsupported extended color/calibration/placement
interpretation rather than silently applying or discarding those semantics.

Opening bounds encoded input, native owner overhead, libpng allocations and
decoded buffers within the declared work budget (maximum256MiB). Decode errors
and longjmps remain inside C; no C++ destructor is jumped over. Callbacks and
FILE pointers are not retained after decoding. Raw reads support cancellation
and validate exact layouts, channel ranges, coordinates and fill lengths.

## Native validation

The final candidate passes all eight executable routes below. The validator
records module, manifest, ABI-test, SDK-consumer and host-CLI hashes before and
after execution and requires them to remain unchanged.

| Route | Decisive evidence |
| --- | --- |
| Direct C ABI |33 authored positive profiles cover all legal color/depth combinations plain/Adam7, sRGB and known/unknown pHYs. Exact full/cropped/padded RGBA, native raw values, channel slices, channel names, metadata, alpha and validity; short/null/bad ABI buffers and cancelled work. Independent Pillow display checks complement literal source samples |
| Refusal and work bounds |10 malformed/unsupported inputs keep exact status expectations. A valid, independently decoded1024x1024 compressed PNG fits the encoded limit but fails the32KiB decoded-allocation limit with the custom allocator error and no handle |
| UTF-8 | A native `PNG-μ-图.png` filename and a Unicode validation directory preserve exact sample/display bytes. Test entry points use Windows wide arguments converted to owned UTF-8 |
| SDK create | Attach PNG, native display/exact UInt8 and UInt16 reads, calibration, RegionAnalysis, Keep and Save through frozen public authorities |
| SDK reopen | Exact saved reader/source/raw/record identity, retained analysis replay/verification and resave |
| Missing provider | Fresh-process offline project metadata and an honest unavailable source; existing records survive |
| Changed implementation | Same-version changed module bytes require native source review. Equal pixels do not authorize old retained identity |
| Late source change | Identical valid display/raw requests succeed before mutation and then raise source-currentness errors; retained payloads remain exact |
| Host CLI | Shared catalog exposes PNG; rendered PAM bytes equal the authored RGBA oracle |

The direct ABI plus five fresh-process SDK modes and two CLI commands form the
eight executable routes; the first three table rows describe assertions within
those routes. No hidden scientific assertion, identity or tolerance is relaxed.
Python is development fixture/oracle/packaging control, not shipped PNG logic.

## Packaging and preservation

The local handoff package includes the module, exact runtime manifest, adapter/
codec notices and a complete selected source kit. Adapter code remains
AGPL-3.0-only; libpng1.6.59 retains libpng-2.0 terms and zlib1.3.2 retains Zlib
terms. Their selected source hashes and per-component ancestry extend the
existing `tests/fixtures/manifest.json`; no second licensing database or
noncommercial specimen/model asset is introduced.

`prepare_png_reader_sources.py` copies exact cached code inputs offline, with
explicit opt-in only for fixed-commit libpng downloads. It never downloads
upstream specimen images. `stage_png_reader_plugin.py` binds the selected
validation/module/product-input hashes, retains source and notices and invokes
the existing material guard. Final per-file/archive admission, built-in checks,
removal preservation and54 frozen-file comparisons are recorded in
`build/png-reader/results.json`.

## Corrections and scope

Initial CMake configuration omitted zlib's unconditional contrib controller;
only that build script was added, with every optional contrib implementation
disabled. Linking then exposed libpng finding the unconfigured adjacent
zconf.h; copied public headers now retain the configured prefix, matching the
existing host recipe. Completed objects were retained.

Three test-only coverage builds strengthened late-source requests, decoded-work
bounds and Windows UTF-8 paths. A non-PNG signature expectation was corrected
to Unsupported while malformed-PNG expectations remain exact. The first staging
check also needed native versus portable path-key handling for the compiled
snapshot; source hashes themselves were unchanged. A final documentation check
also added package-relative links and a source-root LICENSE copy. This changes
packaging/documentation only; the same qualified module bytes remain. Original
failures and passing candidate evidence remain in the workspace.

Support is local Windows x64, recorded MSVC1944/compiler19.44.35225.0, Release
`/MD`. Configure the manifest and choose `org.qupathxx.png` explicitly; legacy
automatic extension routing is unchanged. Typed raw availability does not grant
PNG access to existing TIFF/Zarr-only quantitative recipes. No GUI change,
ICC/animation support, broader format/platform/biological/performance acceptance,
wheel, signing or publication is claimed by this handoff.

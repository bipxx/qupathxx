# Native QOI image plugin

`org.qupathxx.qoi` version1.0.0, contract `qoi-rgba8-native-samples/1`, implements
QOI specification1.0 through the existing native image C ABI. It is an optional,
independently authored C++ module with no application core, Python, Java or codec
runtime dependency. Configure its absolute manifest before catalog/source use;
select the provider explicitly. Existing automatic selection and frozen products
keep their own support scope.

## Samples, display and geometry

The reader supports the six QOI operations (RGB, RGBA, index, difference, luma and
run), original byte wraparound, the initial pixel/cache and upper-left row order.
Raw values are canonical straight RGBA8, including hidden RGB under zero alpha.
QOI's channel hint is informative: a hint of3 never discards encoded alpha or
changes the four-channel native raw lattice. Both valid hints3/4 are retained in
the native vendor description alongside the colorspace. Every in-image raw pixel
is valid regardless of opacity; out-of-image raw codes/validity are zero.

Colorspace0 supplies encoded sRGB RGB with linear alpha; display RGB codes are
unchanged. Colorspace1 supplies linear channels; display RGB uses the standard
linear-sRGB transfer and nearest8-bit quantization, while alpha remains linear.
Raw codes are never converted, rescaled or premultiplied. The display transfer
does not qualify a quantitative recipe to interpret encoded/linear RGB as another
physical value domain. No ICC profile, acquisition calibration or world frame is
inferred. All physical calibration optionals are absent. There is one level,
series, Z and time plane. QOI does not acquire TIFF/Zarr view capabilities.

## Validation and work

Input must have positive dimensions, a valid channel/colorspace hint, sufficient
chunks for exactly the declared raster and exactly one terminal marker. Truncated
chunks, invalid terminal markers, runs beyond the raster, forbidden consecutive
identical index chunks, extra chunks, concatenation and trailing data refuse.
Unknown signatures/hints refuse explicitly. No color/alpha fallback is selected.

The entire encoded input, decoded RGBA raster, array bookkeeping and conservative
path/owner/IO/lookup allowances must fit the supplied work limit (maximum256MiB).
Full decode is bounded; oversized images refuse. Cancellation is checked during
input/decode and raw reads. Output requests/strides/capacities/coordinates are
validated before writing. Display uses the v1 synchronous RGBA entry point; raw
reads additionally expose its synchronous cancel callback. No request, host
buffer or cancel pointer survives a call. The private shared RGBA8 module helper
does not replace native source identity, project/replay or scientific authorities.

## Build and use

Windows x64, exact ReaderPluginKit MSVC1944/compiler19.44.35225.0, Release `/MD`:

```powershell
cmake -S plugins/qoi -B BUILD -G "Visual Studio 17 2022" -A x64 `
  "-DQUPATHXX_READER_PLUGIN_KIT_PREFIX=ABS_KIT"
cmake --build BUILD --config Release
cmake --install BUILD --config Release --prefix INSTALL
```

Keep `plugins/image-common` beside `plugins/qoi` in the source kit. The module
fetches no dependency and does not discover a native SDK or Python. Optional
`QUPATHXX_QOI_BUILD_TESTS=ON` adds direct ABI checks and an installed native SDK
project consumer. `QUPATHXX_QOI_ORACLE_INCLUDE` may additionally point to the exact
locally provided development reference; it is never needed by the module.

```text
qupathxx_region --reader-plugins ABS_MANIFEST --readers
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.qoi INPUT.qoi 0 0 0 512 512 OUTPUT.pam
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.qoi --raw INPUT.qoi OUTPUT.npy --channels 4
```

Save/reopen and exact source/implementation review reuse the host services. The
source-matched region frontend exports exact native raw arrays through the shared
`region_export.hpp` service. Plugin NPY output requires complete valid samples;
its schema2 stdout report includes the existing exact native reader snapshot.
The display PAM export is atomic and new-only. These frontend changes have their
own source/runtime evidence and are not added to an older frozen executable.
The module is process-pinned; close its users before removing/replacing package-owned
files, and preserve images/projects/outputs. A reader does not qualify a writer.

## Source and terms

The [official QOI specification](https://qoiformat.org/qoi-specification.pdf) is
published under CC0 by its [author](https://qoiformat.org/). The independent reader,
shared helper, authored fixtures and test/build tools retain
[AGPL-3.0-only](../../LICENSE). The separately pinned
[MIT reference implementation](https://github.com/phoboslab/qoi) is a development
oracle only: no upstream implementation, benchmark image or asset is required,
linked or bundled with this module. [Reference identities](reference-lock.json)
keep those distinct terms, revisions and hashes. Actual package/source identity,
notices and material admission belong to the existing inventories and the
[delivery evidence](../../docs/benchmarks/qoi-reader-plugin/README.md).

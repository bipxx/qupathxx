# Quantitative TIFF and OME-TIFF

The optional native reader preserves stored sample values separately from the
RGBA8 preview. Enable `QUPATHXX_BUILD_TIFF=ON` when building. `has_tiff` reports
availability. The reader uses pinned libtiff 4.7.1, prefixed zlib 1.3.2 and
tinyxml2 11.0.0; dependency notices ship in enabled installations.

```python
import qupathxx as q

server = q.open_tiff("sample.ome.tif")
metadata = server.raw_metadata
request = q.RawRegionRequest()
request.region = q.RegionRequest(0, 0, 0, 512, 512)
request.first_channel = 0
request.channels = 2
request.z = 0
request.t = 0
raw = server.read_raw(request)
values = raw.numpy()       # read-only native-endian HWC, owns its native buffer
valid = raw.valid_numpy()  # read-only HW mask; outside-image padding is zero
q.save_raw_numpy(raw, "new-region.npy")
stats = q.raw_region_statistics(server, request)
```

`numpy(copy=True)` creates a writable independent array. Retained arrays remain
valid after closing the reader or releasing the Python buffer object. Samples
retain their integer or floating dtype; valid NaN and infinity remain present.
Statistics report finite and nonfinite counts separately and use float64 means,
minima and maxima. In particular, statistics do not promise exact uint64 integer
arithmetic. Optional ROI masks select pixel centers; `.npy` exports contain the
whole requested rectangle and do not serialize the separate validity mask.

The [reader licensing policy](reader-license-policy.md) retains this open-source
libtiff/zlib/tinyxml2/libjpeg-turbo stack and excludes proprietary decoder dependencies.

## Supported inputs

| Property | Contract |
| --- | --- |
| Containers | TIFF and BigTIFF; one-file OME-TIFF and explicit UUID/FileName multi-file OME (feature-branch synthetic engineering qualification; further fault/resource controls open) |
| Types | uint8/16/32/64, int8/16/32/64, float32/64. Plain 1- and 4-bit grayscale TIFF expand to exact UInt8 codes |
| Layout | Strips or tiles; contiguous or separate planes; both byte orders |
| Photometric | Grayscale MINISBLACK/MINISWHITE or RGB, with source alpha per pyramid level in preview. Plain TIFF also supports one lossless unsigned 4-, 8- or 16-bit palette index |
| Orientation | Plain TIFF normalizes all eight TIFF orientations. OME-TIFF requires top-left. SubIFDs must use the base IFD's orientation |
| Compression | Uncompressed, LZW, PackBits, Deflate. Verified CCITT Groups 3 and 4 for plain bilevel TIFF. 8-bit scalar/RGB JPEG with full streams or JPEGTables |
| OME axes | All six Z/C/T orders, explicit TiffData mappings, grouped RGB samples |
| Pyramid | Native SubIFDs with increasing integer isotropic scales |
| Plain TIFF pages | Separate series; no guessed Z/time axes |

Raw and display requests use the same oriented top-left frame. The reader
permutes the exact native samples and swaps the dimensions and X/Y calibration
for orientations that exchange axes. [TIFF 6.0 Orientation tag](https://www.itu.int/itudoc/itu-t/com16/tiff-fx/docs/tiff6.pdf)

Bilevel TIFF supports both FillOrder values. Raw reads preserve the original
sample codes and the view maps 0 and 1 to black or white according to the
photometric tag. Cropped pixels outside the raster keep their separate
invalid flags. [TIFF 6.0 bilevel images](https://www.itu.int/itudoc/itu-t/com16/tiff-fx/docs/tiff6.pdf)

Plain 4-bit grayscale uses most-significant-bit FillOrder and lossless
compression. Raw reads preserve codes 0–15. The default view maps the full
range to grayscale and applies the photometric polarity. Packed grayscale
pyramids require the same bit depth at each level.
[LibTIFF grayscale coverage](https://libtiff.gitlab.io/libtiff/specification/coverage.html)

Palette TIFF raw reads expose the stored indices. Packed 4-bit indices use
most-significant-bit FillOrder and expand to UInt8 values 0–15. The default view maps them
through the selected level's 16-bit ColorMap tables and scales the components
to RGBA8. A custom channel or contrast selection displays the index values.
The reader checks the original photometric tag at selected TIFF and BigTIFF
directories. A palette declaration whose ColorMap is missing or unusable
fails before export, including at pyramid levels.

Old-style JPEG (compression6), JPEG2000, YCbCr, OME palette, packed grayscale OME-TIFF, other bit-packed and complex
samples and noninteger/anisotropic pyramids are rejected by this
reader. Existing OpenSlide format support remains available separately.
[R2-CODEC1](benchmarks/tiff-jpeg-r2/README.md) adds JPEG through the existing
libtiff path with pinned static libjpeg-turbo3.1.3 and packaged licenses/attribution.
Raw values are the decoded lossy samples, not the encoder's original input.
This software is based in part on the work of the Independent JPEG Group.

Multi-file opening uses the same APIs and retains companion observations.
See the [slice contract and limits](multifile-ome-tiff.md) and
[engineering qualification](benchmarks/multifile-ome-210-qualification.json). Unresolved UUID-only
references and incomplete mappings reject explicitly. Explicit BinaryOnly
headers now resolve a full XML metadata companion or master TIFF through the
[guarded dataset path](benchmarks/binaryonly-ome-r2/README.md). XML dependencies
are part of source provenance and change rejection, including cached reads.
Ordinary `.tif` auto-selection retains OpenSlide; `.ome.tif`/`.ome.tiff` selects
the quantitative reader when enabled. Choose `reader="tiff"` explicitly for
other TIFFs. Unsupported or incomplete OME mappings fail explicitly.

OME physical sizes and time intervals are normalized to micrometers and
seconds when supported units and positive finite values are present. Missing,
unknown or invalid calibration remains absent. Raw sample arrays are never
scaled by calibration or display settings.

For an OME time series without a declared `Pixels TimeIncrement`,
the reader can use a complete `Plane DeltaT` table. It requires every channel
and Z plane at a given time index to agree after unit conversion. The series
must start at zero and increase at every index. Each timestamp must agree
with its index times the first interval within one binary64 representable
value. Incomplete, offset and irregular
plane timing stays unspecified in the regular time-interval field.

When a stack omits `Pixels PhysicalSizeZ`, a complete
`Plane PositionZ` table can supply regular Z spacing under the same
zero-origin and increasing-index checks. Each Z plane must have the same
position across channels and timepoints after conversion to micrometers.
Every position needs an explicit physical length unit. OME defaults an
omitted position unit to `reference frame`.

For a multi-file stack, every distinct resolved metadata document must
independently supply the same regular step for an axis. Files that point to
one BinaryOnly master use that master. A missing, incomplete or conflicting
table leaves the affected inferred calibration absent.
[OME Plane and Pixels metadata](https://www.openmicroscopy.org/Schemas/Documentation/Generated/OME-2016-06/ome_xsd.html)

The [R2-SI1 conversion](benchmarks/ome-calibration-units-r2/README.md) recognizes
the OME2016-06 SI prefix range (yotta through yocto) for metres and seconds,
plus minutes, hours and days. Results are micrometres and seconds. The existing
`um` alias, canonical micro symbol and angstrom conversion remain supported;
[R2-UNIT2](benchmarks/ome-nonsi-r2/README.md) additionally normalizes `in`,
`thou`, `li`, `ft`, `yd`, `mi`, `ua`, `ly`, `pc` and `pt` using pinned OME
conversion factors (including its approximate parsec). Units are case-sensitive.
`pixel` and `reference frame` do not establish physical calibration. Positive
finite normalized values, including representable subnormals, are retained;
overflow and conversion to zero remain absent. Companion metadata is compared
using the same normalized quantities. Raw sample values are unchanged.

## Display views and persistence

`server.metadata` describes the RGBA8 preview; `server.raw_metadata` describes
stored quantitative data. `TiffView` selects series, Z, time, three channel
indices, and raw lower/upper display ranges. A channel of -1 disables that
color; automatic values resolve when opening. Integer defaults span the dtype;
floating defaults span 0..1. MINISWHITE inversion affects preview only.
Nonfinite displayed samples are transparent.
Alpha is used when displayed channels share a source TIFF plane; the selected
pyramid level determines whether its samples are premultiplied.

`server.with_tiff_view(view)` creates an independently closable immutable view
of the same series and shares its native index, file handle and decoded-block
cache. `Document.set_tiff_view(view)` replaces the document's display view while
preserving annotations, undo history and calibration. Its revision changes and
old RGB analysis becomes stale. Open another series as a new image.

`Project.add_tiff_image` stores an external TIFF link and its resolved view.
Project schema 12 adds checksummed `tiff_views`; schemas 1–11 remain readable
and upgrade transactionally on save. Layout preferences do not determine image
axes or display selection. External image bytes are not embedded or permanently
frozen by saving a project. Reads reject file changes observed after opening;
relink/reopen explicitly to accept a changed source.

## Native UI and live Python

F2 opens Quantitative channels. Display selects channels and Z/time; Data
selects a raw rectangle or selected area/nucleus and channels, then measures or
exports a new `.npy` file. Results retain the captured axes. Raw data Z/time can
follow the display or remain explicit. Metadata opening, display preparation,
measurement and export run in native workers; document changes publish on the
UI owner. Cancel stops unfinished work. Existing exports are preserved.

The managed `ViewerSession` interface supplies `inspect_raw`, `read_raw` and
`set_tiff_view`. Results carry native-owned metadata and raw buffers and obey
the session's result and region budgets. Raw values have no hidden Python
worker callbacks, following the neighboring naparixx ownership approach.
Ad-hoc raw statistics are not saved analysis records and existing RGB
classifiers do not automatically become multichannel quantitative classifiers.

## Memory and native command line

Raw output including its validity mask is bounded to 256 MiB. Decode and LRU
cache budgets default to 64 MiB each and can be configured through
`TiffOpenOptions`; decode has a 64 KiB minimum, cache can be zero, and each is
capped at 256 MiB. Limits apply per native dataset, shared across its views.
`tiff_read_stats` reports decoded blocks/bytes, cache hits and peak cache charge.
These counters are not a process RSS measurement. Preview conversion uses
256-square working blocks. Catalog and XML limits reject excessive metadata.

```text
qupathxx_region --raw sample.ome.tif new-region.npy --z 2 --time 1 --channel 0 --channels 2 --width 512 --height 512
qupathxx_region --raw-project study.qpx 3 new-region.npy --channel 0 --channels 2 --width 512 --height 512
qupathxx_region --project-region study.qpx 3 0 0 0 512 512 preview.pam
qupathxx_app sample.tif --reader tiff --tiff-series 0
python -m qupathxx sample.tif --reader tiff --tiff-series 0
```

The native raw command emits a JSON receipt and writes a NumPy v1 HWC file via
an atomic new-file operation. Coordinates are level-zero; width and height are
output pixels at the selected native pyramid level. SIGINT/SIGTERM cancel
unfinished reads/writes (exit 130). The GUI also accepts Reader: TIFF when
adding an image.

With `--raw-project`, use the native image ID reported by
`qupathxx_format_import project-images study.qpx`. The saved project selects
the reader and TIFF series. Z and time follow the saved view by default.
`--z` and `--time` override them independently, and `--channel` and
`--channels` address original sample channels. The project and its linked
inputs are protected during publication.
If the document has saved axis calibration, the JSON receipt and optional
bundle include `project_axes` for the actual Z and time indices exported.
It records document Z spacing, Z position and the timestamp in seconds.
Unknown positions are explicitly null. The reader's acquisition calibration
and the array bytes retain their original meaning.

`--project-region` uses the saved reader's display view, including its
channels and contrast ranges, to write RGBA8 PAM. Supply level-zero X and Y,
the pyramid level, and output width and height at that level. This route is
for display previews and uses the reader's normal padding.
Append `--receipt` to write a JSON record to standard output after the PAM
is published. It includes the project SHA-256 and image ID, the observed
reader source, the saved TIFF or OME-Zarr view when present, the crop coordinates
and the SHA-256 of the exact PAM. Optional reader implementations are identified
by their saved binding in `reader_source.reader`.

References: [OME-TIFF specification](https://docs.openmicroscopy.org/ome-model/6.2.2/ome-tiff/specification.html),
[OME schema](https://www.openmicroscopy.org/Schemas/Documentation/Generated/OME-2016-06/ome_xsd.html),
[libtiff 4.7.1](https://libtiff.gitlab.io/libtiff/releases/v4.7.1.html),
[zlib](https://www.zlib.net/), and
[tinyxml2 11.0.0](https://github.com/leethomason/tinyxml2/releases/tag/11.0.0).

The [channel panel capture](images/quantitative-tiff.png) shows the native
display controls. Synthetic correctness and cache observations are recorded in
the [headless](benchmarks/tiff-reader-windows.json) and
[desktop](benchmarks/tiff-reader-gui-windows.json) probes.

The [local verification receipt](benchmarks/tiff-reader-local-windows-linux.json)
records the frozen wheel, complete suite, native configurations, cross-platform
exports, alpha regression, launcher checks and remaining limits.


[Raw analysis records](raw-analysis-records.md) extend the reader with saved
per-channel statistics, exact input digests, original object/TMA context,
explicit replay verification, CSV snapshots and captured project batches.
The Data, Results and Records tabs share native workers with Python commands.


## Interleaved block reuse

Adjacent requested channels stored in the same interleaved TIFF IFD share each
decoded strip/tile during a raw read, including when the LRU is disabled or the
block exceeds its capacity. Separate-plane samples still decode their distinct
physical blocks. Output channel order, native dtype/bits, edge padding and file
identity checks are unchanged. Cancellation is checked before decoding and
between copied rows. This reduces redundant decoding, not the caller's output
buffer or configured decoder/cache memory requirements.


## Coarse quantitative measurements with a smaller work budget

On a quantitative document, use the existing ROI count recipe and work ceiling;
Python, native batch/CLI and the desktop use the same sampler and record path.
For example, with a supported image having at least three raw channels:

```python
sampling = q.RawGridSampling()
sampling.samples_x, sampling.samples_y = 7, 5
options = q.ScientificRawOptions()
options.first_channel, options.channels = 1, 2
options.work_bytes = 768 * 1024
request = q.RawAnalysisRequest.for_grid_roi(document, object_id, sampling, options)
result = q.analyze_raw(request)
record_id = document.keep_raw_analysis(request, result)
```

Coarse grids can use bounded chunk capture and verified rereads. Dense grids keep
the full-window path when chunk work/memory would be worse. Exact values, source
bits, channel support and archived numerical recipes are preserved. Admission
includes captured request/context and can reject before decoding when the work
ceiling does not fit. Reader/cache and process memory remain separate budgets.
This strategy can perform more reads/hashing; it is not a speedup promise or
complete whole-slide support. See the [contract](scientific-grid-contract.md#bounded-coarse-grid-materialization)
and [current evidence](current-task.md).

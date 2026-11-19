# I23 OME-Zarr 0.5 producer audit

The selected workflow is to open a local multiscale image from the direct
`ome_zarr.writer.write_image` API as original typed samples, with an aligned
pyramid and calibrated XY/Z/T axes. This audit used [ome-zarr-py 0.18.0](https://github.com/ome/ome-zarr-py/releases/tag/v0.18.0)
and Zarr 3.1.6. The version and compressor were left to the writer. The test
explicitly supplied two resolutions and a small chunk shape to exercise reads
across chunk boundaries. The [OME-Zarr 0.5 specification](https://ngff.openmicroscopy.org/0.5/)
and [upstream writer guide](https://ome-zarr.readthedocs.io/en/latest/basic/write_image.html)
describe this version of the direct writer API.

The repository's [fixture generator](../../../tools/ci/make_ngff05_producer_fixture.py)
uses synthetic UInt16 samples that distinguish time, channel, Z and both image
axes. It independently reopens the store with upstream Zarr, compares level zero
with the generated input, and records exact reference samples for both levels.
The [producer receipt](../../../build/gui-aggregation/catalogue-closure/ngff05/producer-01/producer.json)
contains 43 store member hashes, the original metadata and raw sample hashes.
An independent generation produced the same receipt and byte-identical `.npy`
references for both levels.
The [retained native reader check](../../../build/gui-aggregation/catalogue-closure/ngff05/producer-01/retained-reader.json)
records exit 1, `OME-Zarr: required store metadata absent: .zgroup`, and no
published image.

| Observed input | Required reader behavior |
| --- | --- |
| Root `zarr.json`, `attributes.ome.version` `0.5`, one multiscale image | Explicitly select the 0.5 profile and use root `zarr.json` as the primary key in the existing directory-store identity. |
| `s0` and `s1` arrays with matching `dimension_names`, regular grid, `uint16`, fill `0` | Keep TCZYX axis order, dimensions and exact sample values. A chunk absent in the observed inventory uses the typed fill. A changed or damaged member fails. |
| Default chunk-key encoding with `/` separator, `bytes` little-endian, then `zstd` with checksum false | Read the `s0/c/` prefix plus five numeric chunk indices from the observed inventory. Decode one complete frame per chunk with exact output length and bounded memory. |
| XY pixel sizes 0.5 and 0.75 µm, Z spacing 2 µm, time step 3 s | Retain calibration and select raw planes explicitly. |
| Level `s1` has 2× XY scale and translations of 0.25 µm in X and 0.375 µm in Y | Test the default writer's pixel-center convention on both axes before admitting the level. |

The existing plugin ABI exposes a level-zero pixel lattice and scalar downsample.
For this export the level-1 sample at index `i` is centered at base index
`2*i + 0.5`, so its footprint begins at base pixel `2*i` and requests can use
the ABI's `floor(x / 2)` mapping. The candidate must validate the center offset
before using that mapping. Other translations need an explicit representable
frame. Test an asymmetric axis case, the first and final reduced-level samples,
odd level-zero origins, and crops across chunk seams against the saved arrays.

## Local native validation

The optional `org.qupathxx.ngff05` reader implements
`ngff05-zarr3-uint16/1` using the shared native store, metadata and region
services. The [30-command installed CLI check](../../../build/gui-aggregation/catalogue-closure/ngff05/integration-05/results.json)
passed all C/Z/T planes at both levels, cross-chunk crops, odd and final
reduced-level origins, calibrated raw metadata, exact RGBA projection, typed
fill, and refusal for damaged frames, unsupported metadata and output inside
the source store. The [existing file-export suite](../../../build/gui-aggregation/catalogue-closure/ngff05/legacy-region-export.log)
passed its TIFF NumPy and PAM behavior.
The [NGFF0.4/Zarr2 regression](../../../build/gui-aggregation/catalogue-closure/ngff05/legacy_ngff_source_tests.log)
passed its existing exact raw and refusal checks against the changed shared
native source with the new plugin profile disabled.

The retained standalone CLI required a store-aware overlay. The module and CLI
use the same colocated SDK OpenSlide DLL, as required by the native loader's
resolved-path check. The SDK archive and host runtime supplied the existing
application authorities. The new codec is the exact upstream
[Zstandard 1.5.7](https://github.com/meta/zstd/releases/tag/v1.5.7) archive
with the BSD-3-Clause license option. The
[intake receipt](../../../build/gui-aggregation/catalogue-closure/ngff05/inputs/zstd-intake.json)
identifies the archive, source revision and notice.

## Relocatable Windows handoff

The [Windows ZIP](../../../build/gui-aggregation/catalogue-closure/ngff05/qupathxx-ngff05-1.0.0-windows-x64.zip)
contains the module, store-aware region CLI, exact OpenSlide DLL, source changes,
build recipes, Zstandard source and component notices. [Archive admission](../../../build/gui-aggregation/catalogue-closure/ngff05/qupathxx-ngff05-1.0.0-windows-x64.admission.json)
verified all 155 owned paths and their exact material records. Exact base SDK
and store archive prerequisites remain separately retained, with corresponding
source availability and digests recorded in the package.

The [extracted package](../../../build/gui-aggregation/catalogue-closure/ngff05/deployment-01/results.json)
passed all 30 CLI checks in a fresh path containing a space. The
[independent offline rebuild](../../../build/gui-aggregation/catalogue-closure/ngff05/rebuild-check-01/results.json)
passed the same checks with the rebuilt module and CLI and the Python install
absent from the native process search path. [Removal](../../../build/gui-aggregation/catalogue-closure/ngff05/uninstall-01.json)
verified all 155 installed file hashes before removal and preserved separate
user export, source and notes files.

## Indexed shards

The pinned writer produced a second [23-file store](../../../build/gui-aggregation/catalogue-closure/ngff05/sharded-producer-01/producer.json)
using regular outer shards, UInt16 bytes/Zstandard inner chunks, and a
little-endian UInt64 index with CRC32C. The first shard spans two channels and
four XY inner chunk positions. One of its inner chunks is omitted by the writer
because it contains only the declared fill value. The
[frozen 1.0.0 reader](../../../build/gui-aggregation/catalogue-closure/ngff05/sharded-baseline.json)
refused this layout before reading samples.

Provider 1.1.0 uses the same host ABI and explicit new contract
`ngff05-zarr3-uint16/2`. It validates divisibility, checks the shard index
checksum, bounds its memory at 65,536 entries, and uses guarded byte-range reads
for an index and each requested inner payload. It supports start or end
index placement and leaves unreferenced gaps in place. The typed fill applies
when the shard or an indexed inner chunk is absent. Malformed sentinels,
overlapping or out-of-file ranges, and damaged encoded frames fail.
The [Zarr sharding specification](https://zarr-specs.readthedocs.io/en/latest/v3/codecs/sharding-indexed/index.html)
defines the on-disk representation. The parser and test fixtures are
independently authored and the module uses the already-admitted Zstandard
dependency.

The [46-command shard suite](../../../build/gui-aggregation/catalogue-closure/ngff05/sharded-integration-02/results.json)
and the [30 regular checks](../../../build/gui-aggregation/catalogue-closure/ngff05/sharded-regular-02/results.json)
passed for 1.1.0. The [legacy NGFF0.4 suite](../../../build/gui-aggregation/catalogue-closure/ngff05/sharded-legacy.log)
passed with the updated shared source.

The [1.1.0 Windows ZIP](../../../build/gui-aggregation/catalogue-closure/ngff05/qupathxx-ngff05-1.1.0-windows-x64.zip)
passed [admission for all 157 members](../../../build/gui-aggregation/catalogue-closure/ngff05/qupathxx-ngff05-1.1.0-windows-x64.admission.json).
The [relocated install](../../../build/gui-aggregation/catalogue-closure/ngff05/sharded-deployment-01/results.json)
and [independent offline module rebuild](../../../build/gui-aggregation/catalogue-closure/ngff05/sharded-rebuild-check-01/results.json)
each passed all 46 shard checks. [Uninstall](../../../build/gui-aggregation/catalogue-closure/ngff05/sharded-uninstall-01.json)
removed the 157 original package files and preserved the test's three user
files. The original 1.0.0 package retains its exact identity for saved reader
bindings.

## Float32 images in 1.2.0

The pinned producer wrote calibrated Float32
[regular](../../../build/gui-aggregation/catalogue-closure/ngff05/float32-regular-producer-01/producer.json)
and [indexed-shard](../../../build/gui-aggregation/catalogue-closure/ngff05/float32-sharded-producer-01/producer.json)
stores with signed intensities and signed zero. The native reader returns
original Float32 bits for numeric analysis and export. Display uses a [0,1]
window and gives pixels with nonfinite selected samples zero alpha.
The parser rounds decimal fills directly to binary32 and accepts the typed
IEEE bit-pattern and nonfinite forms defined by Zarr.
[Zarr data types and fill values](https://zarr-specs.readthedocs.io/en/latest/v3/data-types/index.html)

The [regular](../../../build/gui-aggregation/catalogue-closure/ngff05/float32-regular-integration-01/results.json)
and [indexed-shard](../../../build/gui-aggregation/catalogue-closure/ngff05/float32-sharded-integration-01/results.json)
suites each passed 38 commands. They covered exact producer arrays, fill
rounding and nonfinite payloads, source bounds and malformed metadata. The
same module passed the existing UInt16
[regular](../../../build/gui-aggregation/catalogue-closure/ngff05/float32-uint16-regular-01/results.json)
and [shard](../../../build/gui-aggregation/catalogue-closure/ngff05/float32-uint16-sharded-01/results.json)
checks. The [NGFF 0.4 native regression](../../../build/gui-aggregation/catalogue-closure/ngff05/float32-legacy.log)
and [standalone UInt16 N5 bridge regression](../../../build/gui-aggregation/catalogue-closure/ngff05/float32-n5-regression-01/results.json)
passed.

The [1.2.0 Windows ZIP](../../../build/gui-aggregation/catalogue-closure/ngff05/qupathxx-ngff05-1.2.0-windows-x64.zip)
contains 158 admitted files and uses contract `ngff05-zarr3-image/3`. The
relocated installation passed both [regular](../../../build/gui-aggregation/catalogue-closure/ngff05/float32-regular-deployment-01/results.json)
and [shard](../../../build/gui-aggregation/catalogue-closure/ngff05/float32-sharded-deployment-01/results.json)
checks. An offline module-only rebuild passed the same
[regular](../../../build/gui-aggregation/catalogue-closure/ngff05/float32-regular-rebuild-check-01/results.json)
and [shard](../../../build/gui-aggregation/catalogue-closure/ngff05/float32-sharded-rebuild-check-01/results.json)
checks with the retained CLI. [Removal](../../../build/gui-aggregation/catalogue-closure/ngff05/float32-uninstall-01.json)
preserved three user files. Additional codecs, index encodings, transforms
and OME-Zarr layouts remain separate catalogue items.

## Bytes-only and gzip chunks in 1.3.0

Pinned ome-zarr and zarr-python generated four calibrated producer stores
with bytes-only or gzip pipelines, covering regular chunks and indexed
shards, UInt16 and big-endian Float32. The retained 1.2.0 reader refused all
four. The new reader reuses the native bytes and gzip decoders, caps the
encoded length of uncompressed chunks, and validates gzip's level and complete
payload. CRC32C shard indexes keep the existing contract.
[Zarr bytes codec](https://zarr-specs.readthedocs.io/en/latest/v3/codecs/bytes/index.html) ·
[Zarr gzip codec](https://zarr-specs.readthedocs.io/en/latest/v3/codecs/gzip/index.html)

| Fixture | Exact integration checks |
| --- | --- |
| [Bytes-only regular UInt16](../../../build/gui-aggregation/catalogue-closure/ngff05/bytes-regular-producer-01/producer.json) | [30](../../../build/gui-aggregation/catalogue-closure/ngff05/bytes-regular-integration-01/results.json) |
| [Bytes-only sharded Float32](../../../build/gui-aggregation/catalogue-closure/ngff05/bytes-sharded-producer-01/producer.json) | [43](../../../build/gui-aggregation/catalogue-closure/ngff05/bytes-sharded-integration-01/results.json) |
| [Gzip regular Float32](../../../build/gui-aggregation/catalogue-closure/ngff05/gzip-regular-producer-01/producer.json) | [46](../../../build/gui-aggregation/catalogue-closure/ngff05/gzip-regular-integration-01/results.json) |
| [Gzip sharded UInt16](../../../build/gui-aggregation/catalogue-closure/ngff05/gzip-sharded-producer-01/producer.json) | [35](../../../build/gui-aggregation/catalogue-closure/ngff05/gzip-sharded-integration-01/results.json) |

The four existing Zstandard suites passed 152 checks. The [NGFF 0.4
native regression](../../../build/gui-aggregation/catalogue-closure/ngff05/bytes-gzip-legacy.log)
passed with the changed shared code.

The [Windows 1.3.0 ZIP](../../../build/gui-aggregation/catalogue-closure/ngff05/qupathxx-ngff05-1.3.0-windows-x64.zip)
uses contract `ngff05-zarr3-image/4` and admits 158 files. The relocated
install and offline module rebuild each passed the four new suites with the
unchanged CLI. The [removal test](../../../build/gui-aggregation/catalogue-closure/ngff05/bytes-gzip-uninstall-01.json)
preserved three user files.

## UInt8 images in 1.4.0

Pinned ome-zarr and zarr-python produced calibrated UInt8
[regular Zstandard](../../../build/gui-aggregation/catalogue-closure/ngff05/uint8-regular-producer-01/producer.json),
[gzip shard](../../../build/gui-aggregation/catalogue-closure/ngff05/uint8-sharded-producer-01/producer.json)
and [bytes-only shard](../../../build/gui-aggregation/catalogue-closure/ngff05/uint8-bytes-producer-01/producer.json)
stores. The optional native provider accepts the standard single-byte
serializer with no endian field, preserves one-byte UInt8 samples in raw
exports and projects the original byte values directly to display channels.
Fill is an integer in the range 0–255.

The three suites passed [39](../../../build/gui-aggregation/catalogue-closure/ngff05/uint8-validation-02/uint8-regular-integration-01/results.json),
[44](../../../build/gui-aggregation/catalogue-closure/ngff05/uint8-validation-02/uint8-sharded-integration-01/results.json)
and [40 checks](../../../build/gui-aggregation/catalogue-closure/ngff05/uint8-validation-02/uint8-bytes-integration-01/results.json).
They covered exact UInt8 NPY and RGBA, invalid fill and serializer metadata,
mixed pyramid dtypes and storage corruption. All 306 earlier UInt16 and
Float32 checks passed, plus the [native NGFF 0.4](../../../build/gui-aggregation/catalogue-closure/ngff05/uint8-legacy.log)
and [standalone N5](../../../build/gui-aggregation/catalogue-closure/ngff05/uint8-n5-regression-01/results.json)
regressions.

The [Windows 1.4.0 ZIP](../../../build/gui-aggregation/catalogue-closure/ngff05/qupathxx-ngff05-1.4.0-windows-x64.zip)
admits 158 files. The relocated install and offline module rebuild each
passed all three UInt8 suites. [Removal](../../../build/gui-aggregation/catalogue-closure/ngff05/uint8-uninstall-01.json)
preserved all three user files. The ABI is unchanged and the exact reader
contract is `ngff05-zarr3-image/5`.

## Int16 images in 1.5.0

The producer generated calibrated [regular Zstandard](../../../build/gui-aggregation/catalogue-closure/ngff05/int16-regular-producer-01/producer.json),
[big-endian gzip shard](../../../build/gui-aggregation/catalogue-closure/ngff05/int16-sharded-producer-01/producer.json)
and [big-endian bytes shard](../../../build/gui-aggregation/catalogue-closure/ngff05/int16-bytes-producer-01/producer.json)
fixtures. The reader exports signed samples as Int16 and checks integer fill
against the full range from −32768 to 32767. The display maps that same
range to 0–255 independently of the raw export.

The three producer suites passed [37](../../../build/gui-aggregation/catalogue-closure/ngff05/int16-regular-integration-01/results.json),
[42](../../../build/gui-aggregation/catalogue-closure/ngff05/int16-sharded-integration-01/results.json)
and [38 commands](../../../build/gui-aggregation/catalogue-closure/ngff05/int16-bytes-integration-01/results.json).
They verified exact signed endpoints, byte order, calibrated geometry,
fill, RGBA and refusal of invalid metadata. All 429 prior UInt8, UInt16
and Float32 checks passed. The [legacy native NGFF 0.4](../../../build/gui-aggregation/catalogue-closure/ngff05/int16-legacy.log)
and [standalone N5](../../../build/gui-aggregation/catalogue-closure/ngff05/int16-n5-regression-01/results.json)
regressions passed.

The [Windows 1.5.0 ZIP](../../../build/gui-aggregation/catalogue-closure/ngff05/qupathxx-ngff05-1.5.0-windows-x64.zip)
admits 158 files and uses contract `ngff05-zarr3-image/6`. The relocated
install and offline module rebuild each passed all 117 signed checks. The
[removal test](../../../build/gui-aggregation/catalogue-closure/ngff05/int16-uninstall-01.json)
preserved three user files.

## Producer channel display in 1.6.0

The [pinned producer](../../../build/gui-aggregation/catalogue-closure/ngff05/omero-display-producer-01/producer.json)
wrote two named channels with custom colors and contrast windows. The
[1.5.0 render](../../../build/gui-aggregation/catalogue-closure/ngff05/omero-display-frozen.pam)
used generic RGB and full-range windows. The 1.6.0 reader uses the
declared linear color windows, activity and inversion. It adds RGB
contributions and clips the result to the display byte range. Label-only
metadata keeps the existing default projection. The host's active Z and time
stay authoritative. Producer `defaultZ` and `defaultT` cannot be exposed
through the current reader ABI. [OME-Zarr OMERO metadata](https://ngff.openmicroscopy.org/0.5/#omero-md)

The [45-command check](../../../build/gui-aggregation/catalogue-closure/ngff05/omero-display-integration-01/results.json)
verified overlap and clipping, inactive and inverted channels, Unicode
channel names and exact raw samples. Malformed colors and contrast windows,
unsupported transfer families and unsupported greyscale settings produce a
display error while raw reads still work. Embedded-NUL channel labels
are rejected to avoid truncation on the ABI boundary. The 546 previous native
NGFF 0.5 checks and the [NGFF 0.4 source suite](../../../build/gui-aggregation/catalogue-closure/ngff05/omero-display-legacy.log)
passed.

The [Windows 1.6.0 ZIP](../../../build/gui-aggregation/catalogue-closure/ngff05/qupathxx-ngff05-1.6.0-windows-x64.zip)
admits 159 files and uses contract `ngff05-zarr3-image/7`. The
[relocated install](../../../build/gui-aggregation/catalogue-closure/ngff05/omero-display-deployment-01/results.json)
and [offline build](../../../build/gui-aggregation/catalogue-closure/ngff05/omero-display-rebuild-check-01/results.json)
passed the same display checks. [Removal](../../../build/gui-aggregation/catalogue-closure/ngff05/omero-display-uninstall-01.json)
preserved all three user files.

## Int32 and UInt32 in 1.10.0

Reader 1.9.0 refused the pinned producer's big-endian gzip Int32 and
transposed, whole-shard CRC32C UInt32 images. Reader 1.10.0 preserves both
types through the existing ABI. The producer oracle verifies each calibrated
pyramid level, both time and Z planes, and exact display endpoints. Tests also
verify sparse chunk and inner-shard fills, rejected out-of-range or noninteger
fills, and rejected byte serializers without a byte order.

The retained region host passed 966 commands across 25 profiles. The combined
host passed 94 commands on the two new profiles with each of the built,
relocated and offline-rebuilt modules. The 128-member Windows package ships
the module, manifest, dependency, matching source and notices. The native host
is a separate installation. Removal preserved it, the host's existing OpenSlide
DLL and the exported user array. [Delivery receipt](../../../build/gui-aggregation/catalogue-closure/ngff05/int32-uint32-candidate/results.json)

## Int64 and UInt64 in 1.11.0

The 1.10.0 reader refused the pinned producer's big-endian gzip Int64 and
little-endian transposed, whole-shard CRC32C UInt64 stores. The 1.11.0 module
uses the existing typed ABI and enforces integer-only fills. The source checks
fill range with the same conversion used for missing bytes. The two oracles
check exact typed exports across calibrated levels, signed and unsigned
endpoints, values beyond binary64 precision, sparse fills, and range and byte
order refusals. The fixture producer authors the extreme base-level values
after building the pyramid to avoid a floating-point resampling round trip.

The retained host passed 1,064 commands across 27 profiles. The combined
host passed 98 commands on the new profiles with each of the built, relocated
and offline-rebuilt modules. All 11 earlier archives still pass admission.
The new 128-member package contains the module and matching source and notices.
It uses a separate native host. Removal left the host, its existing OpenSlide
DLL and the exported array byte-identical.
[Delivery receipt](../../../build/gui-aggregation/catalogue-closure/ngff05/int64-uint64-candidate/results.json)

## Float64 in 1.12.0

The optional reader now accepts the pinned producer's big-endian gzip Float64
store and its little-endian transposed indexed shards with whole-shard CRC32C.
Native export preserves binary64 words, including signed zero, subnormals,
infinities, and signed quiet and signaling NaN payloads. The fixture producer
writes those anchors after building the reduced level. Sparse fill checks
cover numeric rounding, named nonfinite values, exact hexadecimal words and
malformed values. The display checks confirm that nonfinite pixels are
transparent and raw export still preserves their original words.

The retained host passed 1,170 commands across 29 profiles. The combined host
passed the 106 new Float64 commands with the built, relocated and offline-rebuilt
module. The built-in NGFF 0.4 native suite passed from the updated source.
The 128-member module package includes matching source and notices. Removal
preserved the separate native host, its OpenSlide DLL and the exported array.
[Delivery receipt](../../../build/gui-aggregation/catalogue-closure/ngff05/float64-candidate/results.json)

## Int8 in 1.13.0

The optional reader now accepts the pinned producer's signed byte images
with regular CRC32C chunks and with transposed, checksummed indexed shards.
It retains Int8 source bytes, signed sparse fills, calibrated pyramid levels
and display endpoints. One-byte serializers accept an omitted or valid
redundant endian field. Range violations and mixed signed/unsigned pyramids
refuse. The selected profile now covers each of the ten real scalar sample
types represented by the native reader ABI.

The retained host passed 1,259 commands across 31 profiles. The combined host
passed the 89 new commands with the built, relocated and offline-rebuilt
module. Each of the 13 earlier archives still passes admission. The new
128-member module package includes matching source and notices. Removal
preserved the separate host, its OpenSlide DLL and the exported Int8 array.
[Delivery receipt](../../../build/gui-aggregation/catalogue-closure/ngff05/int8-candidate/results.json)

## Transpose codec in 1.7.0

Pinned producer stores exercised
[regular chunks](../../../build/gui-aggregation/catalogue-closure/ngff05/transpose-regular-producer-01/producer.json),
[transposed inner chunks](../../../build/gui-aggregation/catalogue-closure/ngff05/transpose-inner-producer-01/producer.json)
and [transpose before indexed sharding](../../../build/gui-aggregation/catalogue-closure/ngff05/transpose-outer-producer-02/producer.json).
The last fixture has a second, noncommuting transpose inside each shard.
The reader composes per-pipeline axis orders into byte and index strides.
Chunk keys, host metadata and exports remain in logical image-axis order.
[Zarr transpose codec](https://zarr-specs.readthedocs.io/en/latest/v3/codecs/transpose/index.html)

The [regular](../../../build/gui-aggregation/catalogue-closure/ngff05/transpose-regular-integration-01/results.json),
[inner](../../../build/gui-aggregation/catalogue-closure/ngff05/transpose-inner-integration-01/results.json)
and [outer](../../../build/gui-aggregation/catalogue-closure/ngff05/transpose-outer-integration-01/results.json)
suites passed 42, 47 and 42 commands. They verified exact pixels, equivalent
composed transposes, fills and malformed-order refusals. All 591 existing
NGFF 0.5 checks and the [native NGFF 0.4 suite](../../../build/gui-aggregation/catalogue-closure/ngff05/transpose-legacy.log)
passed.

The [Windows 1.7.0 ZIP](../../../build/gui-aggregation/catalogue-closure/ngff05/qupathxx-ngff05-1.7.0-windows-x64.zip)
admits 159 files and uses contract `ngff05-zarr3-image/8`. The relocated
installation and offline module rebuild passed all three new suites.
[Removal](../../../build/gui-aggregation/catalogue-closure/ngff05/transpose-uninstall-01.json)
preserved all three user files.

## Chunk payload CRC32C in 1.8.0

The pinned producers wrote [regular UInt16 bytes with a checksum](../../../build/gui-aggregation/catalogue-closure/ngff05/crc32c-regular-producer-01/producer.json),
[big-endian Int16 gzip shards with a decoded checksum](../../../build/gui-aggregation/catalogue-closure/ngff05/crc32c-inner-producer-01/producer.json),
and [transposed Float32 Zstandard shards with both checksums](../../../build/gui-aggregation/catalogue-closure/ngff05/crc32c-double-producer-01/producer.json).
The reader checks encoded bytes before decompressing and decoded bytes
before sampling. Checks apply to regular chunks and individual chunks inside
indexed shards. Whole-shard payload checksums remain a separate contract
because verification would require reading the complete shard.
[Zarr CRC32C codec](https://zarr-specs.readthedocs.io/en/latest/v3/codecs/crc32c/index.html)

The [regular](../../../build/gui-aggregation/catalogue-closure/ngff05/crc32c-regular-integration-01/results.json),
[inner](../../../build/gui-aggregation/catalogue-closure/ngff05/crc32c-inner-integration-01/results.json),
and [double-checksum](../../../build/gui-aggregation/catalogue-closure/ngff05/crc32c-double-integration-01/results.json)
suites passed 31, 40, and 54 commands. They verified exact samples and sparse
fills and refused corrupt payloads before export, including recompressed gzip
with a valid gzip trailer and a stale decoded CRC32C. All 722 earlier NGFF 0.5
checks and the [native NGFF 0.4 suite](../../../build/gui-aggregation/catalogue-closure/ngff05/crc32c-legacy.log)
passed.

The [Windows 1.8.0 ZIP](../../../build/gui-aggregation/catalogue-closure/ngff05/qupathxx-ngff05-1.8.0-windows-x64.zip)
admits 159 files and uses contract `ngff05-zarr3-image/9`.
The relocated installation and offline module rebuild each passed all 125
checks with the retained native host. [Removal](../../../build/gui-aggregation/catalogue-closure/ngff05/crc32c-uninstall-01.json)
preserved all three user files.

## Whole-shard CRC32C in 1.9.0

The pinned writer produced checksummed shards with a
[start index and big-endian Int16 gzip chunks](../../../build/gui-aggregation/catalogue-closure/ngff05/whole-shard-start-producer-01/producer.json)
and an [end index with transposed Float32 Zstandard chunks](../../../build/gui-aggregation/catalogue-closure/ngff05/whole-shard-end-producer-01/producer.json).
The reader verifies the serialized shard in 1 MiB slices before using its
index. Scans are capped at 128 MiB per shard and 512 MiB per raw region request.
The index and inner chunks keep their independent checksums. [Zarr CRC32C codec](https://zarr-specs.readthedocs.io/en/latest/v3/codecs/crc32c/index.html) · [Zarr sharding codec](https://zarr-specs.readthedocs.io/en/latest/v3/codecs/sharding-indexed/index.html)

The [start](../../../build/gui-aggregation/catalogue-closure/ngff05/whole-shard-start-integration-01/results.json)
and [end](../../../build/gui-aggregation/catalogue-closure/ngff05/whole-shard-end-integration-02/results.json)
suites passed 25 commands. They checked exact raw samples, valid padding
across scan slices, and refusal of corruption outside the requested region.
The index and inner checks rejected corruption even with a recomputed outer
checksum. The 847 prior NGFF 0.5 commands and the
[native NGFF 0.4 suite](../../../build/gui-aggregation/catalogue-closure/ngff05/whole-shard-legacy.log)
passed.

The [Windows 1.9.0 ZIP](../../../build/gui-aggregation/catalogue-closure/ngff05/qupathxx-ngff05-1.9.0-windows-x64.zip)
admits 160 files and uses contract `ngff05-zarr3-image/10`.
The relocated installation and offline rebuild each passed the 25 new commands.
[Removal](../../../build/gui-aggregation/catalogue-closure/ngff05/whole-shard-uninstall-01.json)
preserved all three user files.
## Blosc chunks in 1.14.0

The optional Windows module reuses the admitted C-Blosc 1.21.6 and LZ4 1.9.4
static build. Zarr-Python and ome-zarr emitted a big-endian UInt16 regular
store and a transposed Int16 shard store with CRC32C on both sides of
compression. The earlier reader rejected the codec. The downloaded producer
tools served as development oracles, with synthetic data.

The [regular](../../../build/gui-aggregation/catalogue-closure/ngff05/blosc-reg-check-01/results.json)
and [sharded](../../../build/gui-aggregation/catalogue-closure/ngff05/blosc-shard-check-01/results.json)
suites passed 26 and 49 commands. The
[frame checks](../../../build/gui-aggregation/catalogue-closure/ngff05/blosc-frames-01/results.json)
decoded actual LZ4, LZ4HC and BloscLZ blocks with three shuffle modes across
chunk boundaries, and rejected seven malformed frame or metadata cases.
The [gzip](../../../build/gui-aggregation/catalogue-closure/ngff05/blosc-gzip-regression-check-02/results.json)
and [Zstandard](../../../build/gui-aggregation/catalogue-closure/ngff05/blosc-zstd-regression-check-02/results.json)
regressions passed 46 and 30 commands.

The [Windows ZIP inventory](../../../build/gui-aggregation/catalogue-closure/ngff05/qupathxx-ngff05-1.14.0-windows-x64.admission.json)
admits 140 files, including source and notices. The
[relocated module](../../../build/gui-aggregation/catalogue-closure/ngff05/blosc-relocated-frames-01/results.json)
and [offline rebuild](../../../build/gui-aggregation/catalogue-closure/ngff05/blosc-offline-frames-01/results.json)
each passed the ten frame cases. When installing alongside the host's matching
OpenSlide DLL, retain ownership of that preexisting file with the host.
The [removal check](../../../build/gui-aggregation/catalogue-closure/ngff05/blosc-uninstall-02.json)
removed 139 newly installed members and preserved the host, its existing DLLs
and a saved NumPy result.

## Blosc Zstandard in 1.15.0

Zarr-Python 3.1.6 defaults to Zstandard when the producer explicitly uses
`BloscCodec()`. The optional module reads that profile with one bounded
dictionary-free Zstandard frame per compressed split. C-Blosc still owns
its block layout and unshuffle. The original module's source identity remains
bound to its own version and digest.
[Zarr-Python BloscCodec](https://zarr.readthedocs.io/en/v3.1.6/api/zarr/codecs/#zarr.codecs.BloscCodec)

The [regular](../../../build/gui-aggregation/catalogue-closure/ngff05/v115-default-regular/results.json)
and [sharded](../../../build/gui-aggregation/catalogue-closure/ngff05/v115-default-shards/results.json)
checks passed 40 and 57 commands. They retain UInt64 values above 2^53,
including big-endian regular chunks and transposed sharded chunks with both
CRC32C stages. The [frame suite](../../../build/gui-aggregation/catalogue-closure/ngff05/blosc-zstd-frames-01/results.json)
proves actual compression for both layouts, earlier Blosc families and
refusal of a damaged inner Zstandard frame.
[Standalone Zstandard](../../../build/gui-aggregation/catalogue-closure/ngff05/v115-standalone-zstd/results.json)
and [gzip](../../../build/gui-aggregation/catalogue-closure/ngff05/v115-gzip/results.json)
regressions passed 30 and 46 commands.

The [ZIP inventory](../../../build/gui-aggregation/catalogue-closure/ngff05/qupathxx-ngff05-1.15.0-windows-x64.admission.json)
admits 138 Windows package members. The
[relocated package](../../../build/gui-aggregation/catalogue-closure/ngff05/v115-relocated-frames/results.json)
and [offline rebuild](../../../build/gui-aggregation/catalogue-closure/ngff05/v115-offline-frames/results.json)
each passed 13 frame checks. The [removal check](../../../build/gui-aggregation/catalogue-closure/ngff05/v115-removal.json)
removed 137 newly installed members and preserved the host, its shared DLLs
and a saved export.

## Blosc zlib and Snappy in 1.16.0

The module reads all six compressors listed in the Zarr 3 Blosc codec,
within the local OME-Zarr image profile and its existing limits. Blosc zlib
uses the same SDK zlib as standalone gzip and checks exact compressed extent.
The pinned Snappy 1.2.2 source builds as a private static dependency with its
original BSD-3-Clause notice. [Zarr Blosc specification](https://zarr-specs.readthedocs.io/en/latest/v3/codecs/blosc/index.html)

The Zarr-Python producer wrote big-endian UInt64 regular chunks and
transposed UInt64 shards with both CRC32C stages. The
[regular](../../../build/gui-aggregation/catalogue-closure/ngff05/v116-zlib-regular-check/results.json)
and [sharded](../../../build/gui-aggregation/catalogue-closure/ngff05/v116-zlib-shards-check/results.json)
suites passed 40 and 57 commands. The
[19 frame checks](../../../build/gui-aggregation/catalogue-closure/ngff05/v116-frames-02/results.json)
cover every compressor, actual Snappy blocks produced by PyArrow in both
layouts and refusal of trailing bytes inside zlib and Snappy splits.
[Standalone Zstandard](../../../build/gui-aggregation/catalogue-closure/ngff05/v116-zstd-check/results.json)
and [gzip](../../../build/gui-aggregation/catalogue-closure/ngff05/v116-gzip-check/results.json)
regressions passed 30 and 46 commands.

The [Windows ZIP inventory](../../../build/gui-aggregation/catalogue-closure/ngff05/qupathxx-ngff05-1.16.0-windows-x64.admission.json)
admits 142 files. The [relocated package](../../../build/gui-aggregation/catalogue-closure/ngff05/v116-relocated-frames/results.json)
and [offline rebuild](../../../build/gui-aggregation/catalogue-closure/ngff05/v116-offline-frames/results.json)
each passed all 19 frame cases. The
[removal check](../../../build/gui-aggregation/catalogue-closure/ngff05/v116-removal.json)
removed 141 newly installed files and preserved the host, its shared DLLs
and a saved NumPy export.

## Bytes-only and big-endian shard indexes in 1.17.0

The module honors the declared byte order and optional CRC32C in a fixed-size
UInt64 index at either end of the shard. Range, overlap, fill-sentinel and
entry-count guards remain in the shared index decoder. Whole-shard CRC32C is
verified independently when declared. [Zarr sharding specification](https://zarr-specs.readthedocs.io/en/latest/v3/codecs/sharding-indexed/index.html)

The producer checks used a [big-endian index at the start with an outer checksum](../../../build/gui-aggregation/catalogue-closure/ngff05/v117-index-start-check-v117-02/results.json),
a [little-endian bytes-only end index](../../../build/gui-aggregation/catalogue-closure/ngff05/v117-index-end-check-v117/results.json)
and a [big-endian CRC32C end index](../../../build/gui-aggregation/catalogue-closure/ngff05/v117-index-end-crc-check-v117/results.json).
The 38 new cases compare exact arrays and reject malformed indexes. The
[default index](../../../build/gui-aggregation/catalogue-closure/ngff05/sharded-producer-01-check-v117/results.json)
and [legacy](../../../build/gui-aggregation/catalogue-closure/ngff05/v117-legacy-shards/results.json)
checks passed 13 and 46 cases. [Blosc regressions](../../../build/gui-aggregation/catalogue-closure/ngff05/v117-frame-regression/results.json)
passed 19 compressed-frame cases.

The [Windows ZIP inventory](../../../build/gui-aggregation/catalogue-closure/ngff05/qupathxx-ngff05-1.17.0-windows-x64.admission.json)
admits 143 files. Both the
[relocated package](../../../build/gui-aggregation/catalogue-closure/ngff05/v117-index-start-relocated/results.json)
and [offline rebuild](../../../build/gui-aggregation/catalogue-closure/ngff05/v117-index-start-offline/results.json)
passed the three new producer profiles. The
[removal check](../../../build/gui-aggregation/catalogue-closure/ngff05/v117-removal.json)
removed 142 newly installed files and preserved the existing host, its DLLs
and the exported array.

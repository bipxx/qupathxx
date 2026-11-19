# Current implementation task

## Active goal - close all other qualified catalogue

Owner selected eligible formats AND application/integration gaps on2026-11-16.
The full objective persists across packets. The [format roadmap](format-plugin-coverage-plan.md#full-format-roadmap-2026-11-14)
and [R2-R16 queue](api-delivery-plan.md#execution-order-for-finishing-the-remaining-work)
own remaining scope. No100% QuPath, universal biological/speed/platform or
Java/Groovy claim; NC/restricted/unknown required materials stay unadmitted.

## Verified locally - index recompressed Matrix Market ZIP exchange files

The existing Matrix Market zlib build option now enables mixed stored and
Deflate ZIP32 members. Native indexing bounds the archive and its total
expanded bytes, checks the local and central records, and verifies lengths,
complete streams and CRCs. The shared format catalog reports this capability.

The 45 CLI checks verified identical source and QMX bytes from plain, stored
and mixed inputs, including a multi-chunk Deflate member. A reopened spatial
project retained exact values above 2^53, UInt64 maximum, explicit zero and
saved positions after the source QMX moved offline. Six malformed Deflate
cases refused without output. Native project, catalog and region checks
passed. The first CLI run lacked the fixture's ENVI manifest. The first
project test used a working directory too deep for its long scratch case.
Both reruns used the same binaries with corrected inputs and paths. The next
Windows checkpoint can stage this source delta with the existing zlib payload.

[Prior refusal](../build/native-format-closeout/validation-packet/matrix-deflate/baseline.log)
[CLI and saved replay receipts](../build/native-format-closeout/validation-packet/matrix-deflate/roundtrip-envi/validation.json)
[Native project suite](../build/native-format-closeout/validation-packet/matrix-deflate/native-project-short.log)

## Verified locally - Windows packed TIFF and BigTIFF routing handoff

Eight archives match compiled source
`9b3d96d18b0e2ac59e3c6feb1dacfb932775c3415d4e569d9fd4e6e42f62e296`.
One cached build relinked the native consumers, reusing all 15 dependency
payloads and the admitted providers. The corresponding source configured
offline with Python off.

The extracted runtime passed all 109 TIFF CLI checks. An independent fixture
verified all three two-bit foreground IDs, exact pixel counts and sums, and
saved replay. A consumer linked against the extracted SDK opened the three OME
BigTIFF suffixes for preview and reopened a calibrated spatial import at the
correct pixels. The fresh-host checker passed 60 commands locally and removal
preserved all 88 research files. Eight older handoffs kept their original
preflight scope. No retries were needed. Hosted validation still awaits
release authorization and a prepared tag.

[Windows handoff](../build/native-format-closeout/validation-packet/current-native-packed-tiff-install/windows-native-handoff/handoff.json)
[Packaged TIFF CLI checks](../build/native-format-closeout/validation-packet/current-native-packed-tiff-install/handoff-tiff-cli.log)
[Fresh-host checker local receipt](../build/qpx-host-packed-report/handoff.json)

## Verified locally - route OME BigTIFF through preview and spatial import

Automatic RGB nominal preview and direct spatial import now recognize
`.ome.btf`, `.ome.tf2` and `.ome.tf8` through the existing native TIFF selector.
The baseline opened all three explicitly and sent the automatic RGB route to
OpenSlide. The new native fixture verifies actual BigTIFF sample reads and RGB
preview, separate physical X/Y pitches, calibrated spatial point placement,
and exact backed counts above 2^53 after project reopen.

All four affected suites passed after one cached build. The routing check
also covers mixed case and still requires the OME suffix for `.tf8`. A docs
patch had an empty hunk and was corrected before the diff check. The Windows
handoff above includes this fix and the two-bit TIFF workflow.

[Prior RGB route refusal](../build/native-format-closeout/validation-packet/ome-bigtiff-routing/baseline-rgb.log)
[Native routing and spatial verification](../build/native-format-closeout/validation-packet/ome-bigtiff-routing/native-tests.log)

## Verified locally - preserve two-bit TIFF labels

The existing packed reader now preserves codes 0–3 as UInt8 for lossless
MSB-first two-bit plain grayscale and indexed TIFF. The CLI replay verified
strip and tile boundaries, orientation, both grayscale polarities and exact
palette display. Native mask import retained three foreground IDs and exact
per-channel sums after reopening the project, even with an identical color
for every palette entry.

All 109 TIFF CLI checks and the native region, label workflow and label CLI
suites passed after one cached build. The first missing-palette test expected
the later native diagnostic. The fixture refused at LibTIFF open, and the
corrected assertion verifies that refusal and absence of output. A targeted
rerun also verified the sums by exact channel ID. The Windows handoff above
includes the two-bit workflow.

[Prior reader refusal](../build/native-format-closeout/validation-packet/tiff-two-bit/baseline.log)
[TIFF CLI checks](../build/native-format-closeout/validation-packet/tiff-two-bit/tiff-cli-final.log)
[Native suites](../build/native-format-closeout/validation-packet/tiff-two-bit/native-tests.log)
[Label channel sums](../build/native-format-closeout/validation-packet/tiff-two-bit/label-channel-sums.log)

## Verified locally - Windows OME-TIFF XML and ROI handoff

Eight archives match compiled source
`878a4dc98857dbfffc69b55877e48ccebb42b7b28400999bf92e9f425abcd88c`.
One cached build relinked the changed native targets. Offline source
configuration matched with Python off. The handoff reuses all 15 dependency
payloads and includes the existing admitted OME ROI 1.3.0 package with notices.

The extracted runtime passed all 101 TIFF CLI checks. An independent fixture
verified both exact XML export modes, an aligned saved point and mask, exact
raw samples, and clean refusal of implicit orientation. The existing provider,
spatial restore and SDK replay checks passed. The fresh-host checker passed
53 commands locally and removal preserved all 74 research files. Seven older
handoffs retained their original preflight scope. Preflight refused a missing
OME ROI archive and an incorrect module digest. No retries were needed.
Hosted validation still awaits release authorization and a prepared tag.

[Windows handoff](../build/native-format-closeout/validation-packet/current-native-ome-roi-install/windows-native-handoff/handoff.json)
[Packaged TIFF CLI checks](../build/native-format-closeout/validation-packet/current-native-ome-roi-install/handoff-tiff-cli.log)
[Fresh-host checker local receipt](../build/qpx-host-ome-roi-report/handoff.json)

## Verified locally - align stored OME-TIFF ROIs with the normalized raster

The raster reader opens coherent oriented TIFF images with referenced ROIs.
The explicit stored-coordinate import binds exact XML bytes and the selected
image ID, then maps geometry and mask cell edges using the TIFF raster axes.
It uses the admitted OME ROI 1.3.0 module and the existing native annotation
transaction. The CLI and native preview use the same implementation.

All 101 TIFF CLI checks passed. Independent asymmetric fixtures proved exact
raster samples, point registration, rectangle coverage and disconnected mask
coverage across all eight orientations after project reopen. A two-series
BinaryOnly fixture refused the wrong series and replayed the selected one.
Mixed orientations, changed XML and implicit import of matching rotated XML
refuse. The native region suite and four existing import controls passed.
The first replay exposed the lazy project reader wrapper. A scoped rebuild
used the existing native reader accessor and passed the same replay checks.
The Windows handoff above packages the XML export and ROI registration changes.

[TIFF CLI checks](../build/native-format-closeout/validation-packet/ome-roi-orientation/tiff-cli-final.log)
[Native region checks](../build/native-format-closeout/validation-packet/ome-roi-orientation/native-region.log)
[Native import controls](../build/native-format-closeout/validation-packet/ome-roi-orientation/native-import.log)

## Verified locally - resolve BinaryOnly metadata for OME export

`ome-tiff-export-xml --resolve-metadata` exports exact XML from a verified
standalone or TIFF companion and reports the resolved source and SHA-256.
The default continues to export the embedded pointer. The resolver reuses
the existing UUID and input guards and refuses missing, conflicting or nested
companions and output collisions.

All 94 TIFF CLI checks and the native region suite passed. A separate run
used the admitted OME ROI provider and reopened the companion's point at
`[2, 3]` in the matching project. One cached build sufficed. The TIFF fixture
library emitted its existing NumPy warnings. The Windows handoff above includes
both XML export modes and saved replay on oriented rasters.

[TIFF CLI checks](../build/native-format-closeout/validation-packet/ome-xml-resolved/tiff-cli.log)
[Native region checks](../build/native-format-closeout/validation-packet/ome-xml-resolved/region-native.log)
[Exact companion ROI handoff](../build/native-format-closeout/validation-packet/ome-xml-resolved/workflow.json)

## Verified locally - export embedded OME-TIFF XML

`ome-tiff-export-xml` preserves the bounded first-IFD OME XML text and reports
its SHA-256. It reuses the TIFF reader and new-only output protection and
does not open the raster. It preserves `BinaryOnly` references in the exported
document.

All 89 TIFF CLI checks and the native region suite passed. They covered exact
Classic TIFF and BigTIFF text, malformed metadata, and output collisions. The
admitted OME ROI 1.3.0 package imported an extracted point into its matching
native image and reopened it at the same coordinates. The first module lookup
found an intermediate DLL. The installed test used the admitted package hash.
After adding the header's license identifier, a cached relink passed the six
targeted CLI checks and the complete ROI handoff again. The Windows handoff
above includes this route.

[TIFF CLI checks](../build/native-format-closeout/validation-packet/ome-xml-export/tiff-cli.log)
[Exact ROI handoff](../build/native-format-closeout/validation-packet/ome-xml-export/workflow-final.json)

## Verified locally - Windows OME-TIFF raster handoff

The seven archives match compiled source
`fe6a90faa806aee9636d3037fc3ec983181ab89113db6b6bb71e76a8016f511b`.
One cached relink packaged both OME-TIFF changes. The matching source configured
offline with Python off. Staging reused all 15 dependency source payloads and
the existing admitted provider binaries.

The extracted runtime passed all 83 TIFF CLI checks, the existing matrix and
provider workflows, SDK replay and manifest-owned removal. An independent
rotated bit fixture verified exact raw samples, XY pitch and MINISWHITE display
from the saved project. Contradictory OME metadata refused. The fresh-host
checker passed all 46 commands locally and removal preserved 66 research files.
Six earlier handoffs passed preflight with their original scope. No retries
were necessary. Hosted validation awaits release authorization and a prepared tag.

[Windows handoff](../build/native-format-closeout/validation-packet/current-native-ome-raster-install/windows-native-handoff/handoff.json)
[Packaged TIFF CLI checks](../build/native-format-closeout/validation-packet/current-native-ome-raster-install/handoff-tiff-cli.log)
[Fresh-host checker local receipt](../build/qpx-host-ome-report/handoff.json)

## Verified locally - read packed OME-TIFF bit images

The TIFF reader admits OME `Type="bit"` when every selected IFD carries
one-bit grayscale samples and the optional `SignificantBits` is one. It reuses
the plain TIFF unpacking and polarity paths, preserving exact UInt8 0/1 raw
codes. Conflicting sample depth, mixed bit and byte planes, and contradictory
OME metadata refuse before export.

All 83 TIFF CLI checks passed, including stripped and tiled bit stacks, two
photometric polarities, orientation/calibration and byte-identical project
replay. The native region export suite passed. The cached build reused LibTIFF
with no retries. The fixture library emitted its existing NumPy deprecation
warnings. The combined Windows handoff above records the newer archive evidence.

[Retained baseline refusal](../build/native-format-closeout/validation-packet/ome-bit/baseline.log)
[TIFF CLI checks](../build/native-format-closeout/validation-packet/ome-bit/tiff-cli.log)
[Native region export suite](../build/native-format-closeout/validation-packet/ome-bit/region-native.log)

## Verified locally - normalize coherent OME-TIFF rasters

The reader now accepts coherent OME-TIFF orientations through the existing
TIFF raster decoder. OME XY dimensions and pitch describe the stored raster
axes. Normalization swaps dimensions and pitch together where needed and
preserves the C/Z/T plane map. This checkpoint refused referenced ROIs on
non-top-left images. The newer alignment check above adds that route. Mixed
orientations and incompatible stored geometry still refuse.

All 77 TIFF CLI checks and the native region export suite passed. The new
cases covered all eight orientations, anisotropic calibration, exact C/Z
samples, compound SubIFD replay through either file, and byte-identical raw
project replay. The fixture library emitted existing NumPy deprecation
warnings. One cached build reused LibTIFF with no retries. The staged native
handoff predates this reader change.

[Retained baseline refusal](../build/native-format-closeout/validation-packet/ome-raster-orientation/baseline.log)
[TIFF CLI suite](../build/native-format-closeout/validation-packet/ome-raster-orientation/tiff-cli.log)
[Native region export suite](../build/native-format-closeout/validation-packet/ome-raster-orientation/region-native.log)

## Verified locally - Windows spatial restore correctness handoff

The seven archives match compiled source
`eff21391c5cdbfe61d478c2615f1158294fa47fd302eff542cc7a2a478e8d31b`.
One cached relink combined the three changed compiled inputs. The matching
source configured offline with Python off. Staging reused the 15 dependency
source payloads and existing admitted provider binaries.

The extracted runtime passed 36 matrix and spatial checks and 32 more on a
channel-1 ENVI project. Both native spatial import commands refused a same-size
replacement of a verified PPM source and published after explicit adoption.
Provider checks, SDK replay, checksums and owned-file removal passed. The
fresh-host checker passed all 40 commands locally, including channel selection
and both sides of the native source check. Removal preserved all 59 research
files. Five earlier layouts passed preflight with their original scope. No
build or check retries were necessary. Hosted validation awaits release
authorization and a prepared release tag.

[Windows handoff](../build/native-format-closeout/validation-packet/current-native-spatial-guards-install/windows-native-handoff/handoff.json)
[Packaged install and removal](../build/native-format-closeout/validation-packet/current-native-spatial-guards-install/handoff-archive-installation-removal.json)
[Fresh-host checker local receipt](../build/qpx-host-spguard-report/handoff.json)

## Verified locally - honor verified native spatial import sources

The saved-source guard now applies to verified native images in both spatial
import paths. The baseline accepted changed PPM pixels at the same dimensions.
The corrected routes reject that replacement before publication and both save
after explicit review and adoption. The input project and image bytes remain
intact, and the adopted CLI workflow reproduces the original position TSV.

Both affected native suites and six CLI checks passed. The CLI driver initially
expected different wording for unaccepted adoption. Matching the actual
diagnostic was the only retry. The staged Windows handoff predates this fix.

[CLI verification and adoption](../build/qpx-verified-spatial/validation.json)
[Native verified-source tests](../build/qpx-verified-spatial/qupathxx_backed_spatial_import_tests.log)

## Verified locally - preserve the channel of restored spatial points

Backed spatial import now uses native document plane validation for explicit
channels. It records the selected plane in the restore report and preserves
that plane on saved points. The previous all-channel preview metadata shape
stays exact. The selected-count route continues to construct an all-channel
request through its existing API.

The two-band ENVI fixture passed 37 CLI checks. Its channel-1 counts and
positions survived indexing, attach, ZIP exchange, restore, saved GeoJSON and
byte-identical position re-export. A wrong-channel export and an unavailable
channel import refused without writing. Both native spatial suites passed.
The first driver queried the index summary for frame data. It reused the saved
project and index and read the authoritative plane with `matrix-info`. The
packaged handoff retains its prior source identity.

[Channel round-trip receipt](../build/native-format-closeout/validation-packet/spatial-channel-roundtrip/validation-final.json)
[Native backed spatial suite](../build/native-format-closeout/validation-packet/spatial-channel-roundtrip/backed-native.log)

## Verified locally - Windows direct Matrix Market ZIP handoff

The seven archives match compiled source
`618bc962d01300edf8c3e16fc859959041334f8a28745a70c5819b3f9b50df23`.
One cached relink combined ZIP indexing and catalog discovery with gzip enabled.
The matching source configured offline with Python off. Staging reused all 15
dependency source payloads and all admitted provider binaries.

The extracted install passed 36 matrix and spatial checks, including exact
direct ZIP indexing and changed-member CRC refusal. Core and supplemental
provider checks, SDK replay, archive checksums and manifest-owned removal passed.
The fresh-host checker passed all 30 commands locally. Region, Batch and Format
Import reported identical capabilities. Direct ZIP and plain indexing produced
the same QMX, and removal preserved all 47 research files. Four older handoffs
passed preflight with their original check selection. No build or check retries
were needed. Hosted validation awaits release authorization and a prepared tag.

[Windows handoff](../build/native-format-closeout/validation-packet/current-native-matrix-zip-install/windows-native-handoff/handoff.json)
[Packaged matrix and spatial checks](../build/native-format-closeout/validation-packet/current-native-matrix-zip-install/archive-replay/latest%20research/matrix/validation.json)
[Fresh-host checker local receipt](../build/qpx-host-zip-report/handoff.json)

## Verified locally - index the native Matrix Market export archive

`matrix-market-index-zip` streams the three stored members of the native export
through the shared matrix parser. The fixed ZIP headers, directory and CRCs
are checked before QMX publication. Direct and manually extracted inputs
produced byte-identical QMX and source identity. Eleven CLI checks passed,
including malformed CRC, truncation, layout, SQLite companions and existing
outputs. The runtime catalog reports the route as `can_import_zip`.

The legacy project fixture could not write under the long CTest directory.
It passed with a short fixture root. The existing matrix suite passed with gzip
enabled and disabled, and both catalog suites passed. No archive was rebuilt.

[Native CLI checks](../build/native-format-closeout/validation-packet/matrix-market-zip/validation.json)
[Native project suite](../build/native-format-closeout/validation-packet/matrix-market-zip/project-short.log)

## Verified locally - discover Matrix Market gzip support

`qupathxx_format_import --formats` reports `can_import_gzip` for the built-in
Matrix Market provider. It uses the same native catalog as the other entry
points. The `catalog` subcommand still lists separately configured providers.
Both native catalog suites passed with gzip enabled and disabled. The enabled
Region, Batch and Format Import reports matched byte for byte. The disabled
import CLI reported false and refused extra arguments.

Both checks reused the same cache with no retry. This change has its own
source-scoped evidence. The existing gzip archive retains its earlier identity
and remains ready for a hosted run once a release tag is prepared.

[Enabled runtime catalog](../build/native-format-closeout/validation-packet/matrix-market-catalog/formats-enabled.json)
[Disabled runtime catalog](../build/native-format-closeout/validation-packet/matrix-market-catalog/formats-disabled.json)

## Verified locally - Windows gzip Matrix Market handoff

The seven archives match compiled source
`055ecf83e120e928f718a1427b0d5bb415f83873d139944b07ae701da6c5bb3b`.
One cached relink enabled gzip with Python off. The corresponding source
configured offline with the same options. The package reused 15 admitted
dependency source payloads, including zlib, and the existing provider binaries.

The extracted runtime passed 32 matrix and spatial checks, 21 core provider
checks, eight guarded DICOM checks and both desktop reopens. The matrix run
verified all three encoded gzip digests, refused a damaged trailer and restored
and re-exported exact counts and coordinates. SDK replay, nine checksums and
removal passed. The first run held a SQLite handle during the offline-input
check. Closing it allowed the test to finish, with the completed provider and
offline configuration checks reused.

The fresh-host checker now selects the gzip round trip from the pinned handoff
receipt. All 25 commands passed locally against these exact archives. Removal
deleted 532 installed files and preserved all 45 research files. Three older
handoffs passed preflight with their original check selection. Hosted validation
awaits a prepared release tag carrying the staged assets.

[Windows handoff](../build/native-format-closeout/validation-packet/current-native-gzip-install/windows-native-handoff/handoff.json)
[Packaged matrix and spatial checks](../build/native-format-closeout/validation-packet/current-native-gzip-install/archive-replay/latest%20research/matrix-resumed/validation.json)
[Fresh-host checker local receipt](../build/qpx-host-gzip-report/handoff.json)

## Verified locally - gzip Matrix Market exchange inputs

`QUPATHXX_BUILD_MATRIX_MARKET_GZIP` enables single-member gzip transport for
each input in the existing native Matrix Market importer. It reuses pinned
zlib 1.3.2, hashes the original encoded bytes and bounds encoded and expanded
size. The prior plain fixture retained the exact QMX hash. The native suite
checked the exact expansion limit and refused corrupt, truncated and trailing
streams before QMX publication. Nine CLI checks confirmed the all-gzip, mixed,
multi-buffer and malformed cases. The default-disabled build passed its native
suite and refused compressed inputs with the build option in its diagnostic.

The cached build compiled only the importer and affected test for each option
state. The enabled binaries and results were retained before the disabled check.
No retry was needed. The gzip route is locally verified and awaits a matching
package when selected for delivery.

[Enabled CLI checks](../build/native-format-closeout/validation-packet/matrix-market-gzip/validation.json)
[Disabled native suite](../build/native-format-closeout/validation-packet/matrix-market-gzip/native-disabled.log)

## Verified locally - fresh-host position round trip

The Windows fresh-host checker selects backed position replay when the pinned
archive receipt records that capability. It restores an independently authored
Matrix Market exchange on a saved ENVI image, moves the original QMX and TSV
offline, and verifies exact positions in observation order and exact sparse
counts through UInt64 maximum. The wrong frame refuses before writing.

The exact seven staged archives passed all 24 commands in a new local prefix.
Removal deleted 532 installed files and preserved 41 research files. Both
older archive layouts passed preflight with their original check selection.
Only the installed SDK example built. No retry was needed. The hosted run
awaits a prepared release tag with the exact assets.

[Fresh-host checker local receipt](../build/qpx-host-pos-report/handoff.json)
[Command results](../build/qpx-host-pos-report/commands.json)

## Verified locally - packaged restored position export

The Windows runtime, SDK and corresponding source match compiled fingerprint
`da1b22a2d3203d249eee161d790918e0dbc8cde010d3fc31cacbed4068d766f5`.
One cached relink covered eight entry points and six changed compiled inputs.
The seven-archive handoff reused 15 admitted dependency source payloads and all
provider binaries. The extracted source configured offline with Python off.

The extracted runtime passed 31 matrix and spatial checks, including exact
saved-frame TSV re-export with the original QMX and TSV offline. The runtime
also passed 21 core provider checks, eight guarded DICOM checks and both
supplemental desktop reopens. The SDK consumer replayed its saved project.
Removal deleted 512 installed files and preserved 106 research files. All nine
checksums and the seven-archive fresh-host preflight passed locally.

The staging check initially expected the earlier CLI binary hash. It reused
the installed files and matched the settled relink to the cache. The replay
harness also needed the existing NumPy test environment for its oracle. The
retry reused the completed offline configuration. Hosted validation awaits a
prepared release tag carrying the staged assets.

[Windows handoff](../build/native-format-closeout/validation-packet/current-native-position-install/windows-native-handoff/handoff.json)
[Archive checksums](../build/native-format-closeout/validation-packet/current-native-position-install/windows-native-handoff/SHA256SUMS.txt)
[Archive installation and removal](../build/native-format-closeout/validation-packet/current-native-position-install/handoff-archive-installation-removal.json)
[Packaged matrix and spatial checks](../build/native-format-closeout/validation-packet/current-native-position-install/archive-replay/latest%20research/matrix/validation.json)

## Verified locally - re-export restored spatial positions

`project-matrix-export-positions --frame-id` replays coordinates from saved
backed point detections through the native frame and exact observation-ID
join. The existing embedded-coordinate route remains available offline. The
restored ENVI project exported the original TSV byte for byte with the input
QMX and TSV offline. The CLI refused a wrong frame, wrong plane, existing output
and unavailable plugin. Both native spatial suites passed, including matrix-axis
order from shuffled input, complete-cohort refusal and publication guard checks.

The first configure used an incompatible CMake version. The cache's recorded
executable recovered it. Reused compiled objects after qualifying the shared
writer's namespace, then rebuilt only the test after correcting its required
guard callback. This source change has its own CLI receipt. The prior Windows
handoff awaits hosted validation and no matching new package was staged here.

[Native CLI receipt](../build/native-format-closeout/validation-packet/spatial-position-reexport/validation.json)
[Backed spatial suite](../build/native-format-closeout/validation-packet/spatial-position-reexport/backed-spatial-final.log)

## Verified locally - Windows supplemental provider handoff check

The candidate workflow fetches the ENVI and DICOM assets. Its runner validates
them against the pinned seven-archive receipt and checks that the DICOM package
uses the exact OpenSlide DLL already installed with the runtime. The older
five-archive receipt still passes preflight. Missing and same-size corrupt ENVI
archives refused before creating either output directory.

The relocated install passed all 19 commands. The saved ENVI project replayed
exact UInt16 samples. The saved DICOM project replayed the exact compound crop
and refused export after a companion replacement that remained directly
readable. Both plugin projects reopened on the desktop. Removal deleted 532
installed files and preserved 32 research files. Only the SDK example built.
The first pass used the wrong expected refusal diagnostic. The corrected pass
also checked the replacement crop and retained the first run's evidence while
removing its duplicate installed files. The hosted run awaits a prepared
release tag carrying the exact pinned assets.

[Install and removal receipt](../build/qpx-sup2-report/handoff.json)
[Recorded native commands](../build/qpx-sup2-report/commands.json)

## Verified locally - current native Windows spatial and DICOM handoff

The spatial restore refused an existing destination WAL without changing the
WAL or publishing a project or managed matrix. One cached build relinked all
eight shipped entry points after nine compiled inputs changed since the frozen
handoff. A fresh seven-component Windows install matched the current source
fingerprint and used the existing admitted ENVI and DICOM provider binaries.

The installed CLI passed 28 matrix and spatial checks and eight guarded DICOM
checks. The desktop reopened both saved projects without changing their bytes.
Removal deleted 296 installed files and preserved all 50 research files. There
were no build or validation retries for the component install.

The matching runtime, SDK and source archives authenticated against their
reviewed member inventories. Staging reused the 15 dependency source payloads
and independently admitted optional provider packages. The extracted source
configured offline with Python disabled. The extracted runtime and SDK passed
the SDK consumer replay, 21 checks on the core optional provider pack, all 36
matrix and DICOM checks, and both supplemental desktop reopens. Removal deleted
512 installed files and preserved all 105 research files. All nine checksums
passed. The archive verifier needed to pass the desktop image ID as a string.
It reused the completed CLI runs and offline configuration on retry. The
handoff is local and remains separate from the earlier frozen archive.

[Current Windows archive handoff](../build/native-format-closeout/validation-packet/current-native-spatial-install/windows-native-handoff/handoff.json)
[Archive checksums](../build/native-format-closeout/validation-packet/current-native-spatial-install/windows-native-handoff/SHA256SUMS.txt)
[Archive installation and removal](../build/native-format-closeout/validation-packet/current-native-spatial-install/handoff-archive-installation-removal.json)

[Installed check and removal receipt](../build/native-format-closeout/validation-packet/current-native-spatial-install/installation-removal.json)
[Current compiled input manifest](../build/native-format-closeout/validation-packet/current-native-spatial-install/SOURCE-INPUTS.sha256)

## Verified locally - restore exported counts and positions to a project

`project-matrix-import-positions` calls the native backed spatial importer.
The backed workflow now opens admitted reader-plugin links from saved projects
and guards their sources across preview and publication. The ENVI fixture
round-tripped three named points and the complete sparse count matrix from
Matrix Market and an exported position TSV. The new project retained its own
exact QMX copy. With the exchanged QMX and TSV offline, it re-exported the ZIP
byte for byte and returned all three original coordinates.

One cached Windows build passed the native backed suite and 28 CLI checks.
The import refused an unknown observation ID, an existing output and a missing
ENVI provider. No build or validation retries were needed. Only the existing
saved reader and level-zero pixel position workflow was exercised.

[Spatial round-trip receipt](../build/native-format-closeout/validation-packet/matrix-spatial-roundtrip/cli/validation.json)
[Native backed suite](../build/native-format-closeout/validation-packet/matrix-spatial-roundtrip/native.log)

## Verified locally - re-index Matrix Market from the native CLI

`matrix-market-index` uses the shared native Matrix Market importer with
explicit QPX or Tenx catalogues. A saved project exported to ZIP and re-indexed
through the CLI with identical ordered features, barcodes and UInt64 counts.
The result reopened with the reported QMX digest and source identity. The CLI
refused the wrong catalogue, duplicate coordinates and existing destinations.

The former SDK import published a QMX alongside a pre-existing destination WAL.
The shared importer now checks database companions before staging and before
publication. The CLI refused the same case, preserved the sentinel WAL and
wrote no QMX. One cached build was enough. The native project suite and all
21 round-trip CLI checks passed. Coordinates can be supplied separately in
the existing spatial import workflow.

[CLI round-trip receipt](../build/native-format-closeout/validation-packet/matrix-market-index/cli/validation.json)
[Native test suite](../build/native-format-closeout/validation-packet/matrix-market-index/native.log)

## Verified locally - saved sparse Matrix Market export

`project-matrix-export-mtx` writes a new ZIP with all sparse count entries,
feature metadata and observation IDs. Native reimport preserved explicit
zeros, empty rows and columns, and exact values through `UINT64_MAX`. The CLI
matched the stored SQLite rows, repeated byte-identically, refused continuous
matrices, and protected existing outputs, project files, sidecars and hardlinks.

One cached Windows build covered the changed CLI and native project test.
The test suite's legacy long-path fixture needed a shorter working directory.
CLI check fixes matched the hardlink diagnostic and picked the continuous
association by its actual domain. The final native suite and 13 CLI checks
passed. Coordinates remain available through the saved-position export. The
new CLI retains its own source identity apart from the frozen Windows handoff.

[CLI export and native reimport receipt](../build/native-format-closeout/validation-packet/matrix-market-export/cli-sdk-verified/validation.json)
[Native project suite](../build/native-format-closeout/validation-packet/matrix-market-export/native-short.log)

## Verified locally - optional Windows DICOM WSI handoff

The 43-member plugin ZIP contains the tested module and manifest under
`openslide-sdk/bin`, the matching SDK, corresponding source, build sources,
ReaderPluginKit and license notices. Delivery membership and the archive
checksum pass against the material inventory.

The module rebuilt offline from the extracted package. The original archive
binary and the rebuilt binary each passed eight native workflow checks in
fresh Windows installation directories. Both reproduced exact Leica-4 and
concatenation crops and refused a changed companion before export. Removal
preserved all 28 research files in each case. The engine and frozen host
archives were reused. No retry or additional dependency download was needed.
The handoff remains local and has its own archive and module identity.

[Windows DICOM WSI archive](../build/qpx-dicom-plugin/delivery/qupathxx-dicom-wsi-1.0.0-windows-x64.zip)
[Archive verification](../build/qpx-dicom-plugin/delivery/verification.json)
[Archive installation and removal](../build/qpx-dicom-plugin/delivery/archive/installation-removal.json)

## Verified locally - optional DICOM WSI companion guard

The optional Windows provider uses OpenSlide for display and the existing
compound-source ABI for project links. Both concatenation entry files produced
the exact crop, and the Leica-4 crop matched the existing real-slide result.
Changing an unrelated series left the saved crop unchanged. Replacing the
companion at the same geometry made saved-project export refuse before writing.
An explicit review classified the change as `changed_dependencies`. Adoption
required acceptance, produced a new project, and reopened to the approved
crop. Export refused a destination hard-linked to the companion.

The initial build needed `dumpbin` discovery for the import graph. The frozen
host also required the two unused raw ABI slots to return `UNSUPPORTED`.
After correcting both, the existing host passed without an engine rebuild.
The reviewed code and dependency pins are recorded with the provider. The
metadata scan bounds bytes read. Decoder heap limits and further DICOM profiles
have separate scope.

[Provider build and use](../plugins/dicom_wsi/README.md)
[Installed verification receipt](../build/qpx-dicom-plugin/verification.json)

## Verified locally - native DICOM series discovery probe

The OpenSlide DLL exports display and metadata calls. Its corresponding source
supplied the 16 selected libdicom 1.3.0 (MIT) and uthash 2.3.0 (BSD-1-Clause)
code and notice files for a standalone native probe. It found the matching
instances from either concatenation entry, found all five Leica-4 companions,
excluded an unrelated series and recognized an extensionless Unicode filename.
It refused a duplicate SOP Instance UID and identified the replacement file at
the same path. The exact inputs and binary are recorded in the probe receipt.

The initial link needed C linkage for the libdicom header. The narrow CRT API
missed the Unicode path, so the probe now uses wide file IO. The decoder's
metadata subset exceeded the probe's 8 MiB read allowance on Leica-4. Reading
the earlier series and instance tags before image geometry passed within that
allowance. The app and frozen OpenSlide binary were reused unchanged.

Next wire selected discovery into an optional ABI1 `SOURCE_FILES` provider
and reuse the host's compound observations across open, read and saved reopen.
Validate decoder and discovery agreement on ambiguous members, bound native
metadata allocations, and require the previous same-geometry replacement to
refuse before publishing a crop. Verify that an unrelated series can change
without invalidating the project.

[Native discovery receipt](../build/qpx-dicom-probe/verification.json)
[Selected code and notice hashes](../build/qpx-dicom-probe/source-inputs.json)

## Verified locally - DICOM concatenation and a companion identity gap

The frozen Windows reader assembled two lossless JPEG 2000 instances into one
`TILED_FULL` level. A crop across the instance boundary reproduced the authored
reference from either input filename and through a saved project. With the
second instance removed, direct crop, saved-project crop and new-project import
refused and wrote no output. An unrelated series in the same directory did not
substitute for it. Restoring the original file reproduced the crop exactly.

Replacing the second instance with different pixels under its original series,
instance and geometry metadata changed the saved-project crop while leaving
the primary and project bytes unchanged. The crop receipt reports `primary_file`.
Use this reproducer for the next complete-fileset design. Select the decoder's
actual members, reuse the existing compound reader and project verification
authorities, and confirm that unrelated series do not enter the identity.

The first probe expected the wrong OpenSlide error text. The corrected probe
checked the actual one-instance refusal and each CLI's exit status. A typo in
the roadmap anchor delayed the documentation patch. No build or download was
needed.

[Concatenation and replacement receipt](../build/native-format-closeout/validation-packet/dicom-wsi-concatenation/verified/verification.json)
[DICOM full-tile instance ordering](https://dicom.nema.org/Medical/dicom/current/output/chtml/part03/sect_C.7.6.17.3.html)

## Verified locally - DICOM JPEG 2000 and uncompressed RGB transfer syntaxes

The frozen Windows OpenSlide runtime read three authored `TILED_FULL` profiles:
uncompressed RGB8, JPEG 2000 lossless YBR_RCT, and JPEG 2000 lossy YBR_ICT.
An RGBA crop across the tile seam matched the authored RGB samples exactly in
the first two profiles. The lossy crop matched the independent frame decode
exactly. All three respected the image's right edge and retained distinct
0.7 µm X and 1.3 µm Y calibration in a saved project. Reopening reproduced
the original crop and left the project unchanged.

Small authored frames replaced a large upstream download. The fixture tools
ran from their existing development cache. No engine build, new runtime
dependency, or validation retry was needed. This check used single-instance
fixtures.

[Transfer syntax verification receipt](../build/native-format-closeout/validation-packet/dicom-wsi-jpeg2000/verification.json)
[DICOM JPEG 2000 encoding contract](https://dicom.nema.org/medical/dicom/current/output/chtml/part05/sect_8.2.4.html)

## Verified locally - explain missing DICOM pyramid companions on project reopen

The shared native compatibility check now reports the observed and expected
DICOM pyramid level counts and directs users to restore the original series
files to one directory. The frozen CLI rejected the incomplete Leica-4 series
with a generic relink message. The updated CLI reported 1 of 3 expected levels
and refused to write a crop. Restoring the companions produced the original
level-one crop byte for byte and left the project unchanged.

The native project suite passed. A second cached rebuild was needed to correct
the diagnostic wording for a single observed level. The frozen handoff retains
its source identity. Full companion identity and same-geometry substitutions
remain outside this check.

[Saved-project companion check](../build/native-format-closeout/validation-packet/dicom-wsi-leica4/saved-project-companion-diagnostic/verification.json)
[Native project suite](../build/native-format-closeout/validation-packet/dicom-wsi-leica4/companion-project-tests.log)

## Verified locally - sparse DICOM whole-slide viewing

Review closed the M06 admission investigation with schema reuse terms still
unresolved. Independent open producers exist, and the delivered Matrix
Market/TSV and AnnData routes cover eligible exports.

W02 reused the frozen Windows OpenSlide reader on the six-file CC0 Leica-4
series. The 8 × 8 level-zero crop matched an independent JPEG frame decode
exactly. A saved project retained the 23,374 × 22,079 dimensions and both
1.05105387047 µm pixel axes. Reopening produced the same crop and preserved
the project bytes. Level 1 worked with companion files present and refused
when only the primary file was available. The reader reports primary-file
provenance and refuses raw-sample export. No rebuild or download retry was
needed. Further W02 claims require their own transfer syntax or companion
fixture, starting with an affected workflow.

[DICOM verification receipt](../build/native-format-closeout/validation-packet/dicom-wsi-leica4/verification.json)
[OpenSlide fixture license](https://openslide.cs.cmu.edu/download/openslide-testdata/DICOM/)

## Verified locally - accumulated native changes from one Windows install

The five-archive Windows native handoff for `4ce0b856` is staged and verified
locally. It includes the runtime, SDK, matching application and dependency
sources, and separately versioned optional provider packs. The source ZIP
configured offline with Python disabled and reproduced the build's exact
source fingerprint. The runtime dependency scan covered 11 PE binaries.
From the extracted archives, the SDK consumer saved and replayed its project,
and the four optional provider packs passed 21 workflow checks. Removal deleted
505 installed files and preserved 56 research files. All seven handoff checksums
passed. Publication and a fresh-host run are open.

The Windows candidate workflow now accepts a pinned five-archive handoff.
The runner passed locally from the exact ZIPs in a relocated prefix: the SDK
consumer and native CLI replayed the saved analysis, VTI returned exact UInt16
samples, and the desktop reopened the project. Removal deleted 505 installed
files and preserved all 20 research files. A wrong receipt pin refused before
creating either output directory. The SDK example was the only build. The
hosted run awaits a prepared release tag carrying these exact assets.

[Fresh-host runner local receipt](../build/qpx-handoff-report/handoff.json)

[Current Windows handoff](../build/native-format-closeout/validation-packet/current-native-workflow-install/windows-native-handoff/handoff.json)
[Archive checksums](../build/native-format-closeout/validation-packet/current-native-workflow-install/windows-native-handoff/SHA256SUMS.txt)
[Archive install and removal receipt](../build/native-format-closeout/validation-packet/current-native-workflow-install/handoff-archive-installation-removal.json)

One cached Python-off build brought all eight installed entry points onto the
current source. The relocated seven-component stage and admitted VTI provider
passed 126 checks across TIFF, migration, axes, TMA and stain workflows. The
installed desktop reopened the saved VTI project without changing it, and the
installed spatial-counts CLI saved and reopened the BOM-bearing position input.

Manifest-owned removal deleted 320 files and preserved all 784 research and
validation files byte-for-byte. No build or validation retries were needed.
The earlier handoff archives are a separate frozen source. This check produced
no new runtime archive or wheel.

The integration checkpoint captures all 1,063 compiled input paths. Comparing
the current install with the earlier handoff found 17 changed compiled inputs,
with no paths added or removed. The install has the earlier component membership
plus 31 files for the VTI provider. The first comparison hit the Windows default
text encoding on a Unicode path and passed after decoding the manifests as UTF-8.
Staging checked those 17 inputs against the frozen source and matched the 15
changed install members to the verified build. It reused 15 authenticated
dependency payloads. The runtime scanner required its canonical candidate
filename, so verification used a byte-identical hard link. MSBuild failed in
the deep SDK test directory. A shorter consumer build path passed and reused
the completed offline configuration and provider replay. No engine rebuild
or dependency fetch was needed.

[Current install and removal receipt](../build/native-format-closeout/validation-packet/current-native-workflow-install/installation-removal.json)
[Installed workflow checks](../build/native-format-closeout/validation-packet/current-native-workflow-install/pytest.log)
[Current compiled inputs](../build/native-format-closeout/validation-packet/current-native-workflow-install/SOURCE-INPUTS.sha256)

## Delivered locally - export a project's saved stain model with an offline image

`--export-stain-model` writes a new model JSON from saved image defaults. The
shared writer retains the exact basis, white and model digest, and rechecks the
project and its protected sources before publishing. The original image may
be offline. Applying the model to another project and exporting it again
produced byte-identical JSON.

Four CLI checks and both native model suites passed after one cached build.
The staged route refused the missing command before the change. No plugin,
binding, or dependency rebuild was needed, and there were no validation retries.

[Prior command refusal](../build/native-format-closeout/validation-packet/export-saved-stain-model/baseline.log)
[Offline export and transfer checks](../build/native-format-closeout/validation-packet/export-saved-stain-model/pytest.log)
[Native model suites](../build/native-format-closeout/validation-packet/export-saved-stain-model/native.log)

## Delivered locally - save an approved stain model from the native CLI

`--apply-stain-model` saves an approved JSON model as one image's defaults in
a new project. The shared native operation verifies its digest, preserves the
exact normalized basis and white values, and refuses incompatible image types
and protected destinations. The other image's settings remained absent.

Three targeted checks passed after one cached CLI build. The estimate, apply
and batch replay matched the explicit model's measurements. The frozen Python
reader could not open the newly written schema, so the test independently checked
the exact saved SQLite payload and reopened it through the current native CLI.
The replay comparison uses separate CSV destinations for the same generation.

[Prior CLI refusal](../build/native-format-closeout/validation-packet/apply-stain-model/baseline.log)
[Estimate, apply and replay checks](../build/native-format-closeout/validation-packet/apply-stain-model/replay.log)

## Delivered locally - batch with each image's saved stain defaults

The native batch now accepts `--saved-stains` for ROI, tissue and cell runs.
It resolves each image's transform from the captured project. Missing or
unsupported settings and conflicting explicit models refuse before output.
Two images matched direct stain measurements and reused their checkpoints.
Changing one model changed its captured input and recomputed its measurements.
Tissue and cell exports matched the explicit-transform controls.

One cached Python-off build passed the three stain batch checks and both
native batch suites. The first changed-model assertion looked at the unchanged
pixel digest instead of the captured input digest. The tissue/cell comparison
was corrected to exclude run time and elapsed time. The native suite's deep
Windows work path failed cleanup, so the same binary passed in a shorter path.

[CLI validation](../build/native-format-closeout/validation-packet/batch-saved-stains/full-mode-checks.log)
[Native batch suite](../build/native-format-closeout/validation-packet/batch-saved-stains/native-short.log)
[Batch recipe suite](../build/native-format-closeout/validation-packet/batch-saved-stains/native.log)

## Delivered locally - read positions TSV with a UTF-8 signature

The staged CLI rejected a positions TSV with a leading UTF-8 BOM. The native
parser now accepts one signature at the start of the header. The input digest
still covers the original bytes. A second signature fails before publication.

One cached build passed both native spatial suites and eight command checks.
The checks reopened a spatial-counts project and indexed, attached and exported
an optional CSV matrix with exact coordinates and the count `9007199254740993`.
An earlier matrix-attachment probe confirmed that the existing save guard
already protects QMX companion names. There were no validation retries for
the position parser change.

[Staged parser refusal](../build/native-format-closeout/validation-packet/positions-tsv-bom/baseline.log)
[Native command reports](../build/native-format-closeout/validation-packet/positions-tsv-bom/commands.json)
[Native spatial suites](../build/native-format-closeout/validation-packet/positions-tsv-bom/native.log)

## Delivered locally - verify legacy revisions on TMA source-object rows

Both staged exporters accepted a stale `object_revision` on a row identified
by `source_object_id` when `--allow-unversioned` was supplied. Both now check
the revision. If an `object_id` column accompanies the fallback revision, it
must identify the same core. Matching rows produce a verified result with
the exact input digest. Refused rows create no output.

The 37 case/cohort export checks and native TMA suite passed after one cached
Python-off build. Both baseline failures were reproduced before the change.
No build or validation retries were needed.

[Staged export failures](../build/native-format-closeout/validation-packet/tma-legacy-revision/baseline.log)
[Export checks](../build/native-format-closeout/validation-packet/tma-legacy-revision/pytest.log)
[Native TMA suite](../build/native-format-closeout/validation-packet/tma-legacy-revision/native.log)

## Verified locally - QuPath migration with a directory-backed native reader

Seven commands with the admitted NGFF 0.5 module and producer store verified
whole-directory review binding. A change to array metadata with the root
unchanged invalidated the review, and output inside the store was refused.
A fresh review saved and reopened the project with all 96 original UInt16
samples on the selected C/Z/T plane. The source fixture stayed byte-identical.

The first catalog probe used the import CLI instead of the region CLI. The
first replay expected different wording for a correct protected-output refusal.
The corrected replay reused the same binaries and fixture without rebuilding.

[Directory migration receipt](../build/native-format-closeout/validation-packet/qupath-ngff05-provider/replay/validation.json)
[Native command reports](../build/native-format-closeout/validation-packet/qupath-ngff05-provider/replay/commands.json)

## Delivered locally - select a native reader per QuPath project entry

The prior migration CLI could not use the installed VTI reader. Native preview
and the CLI now accept explicit per-entry provider and options, with the
existing registry, project source guard and saved reader link. The review
distinguishes two same-sized VTI arrays. Import refuses an option switch after
preview, and reopen reads the exact selected UInt16 Z plane.

Twenty migration CLI checks and the native QuPath project suite passed after
one cached Python-off build. The existing VTI package and fixture supplied the
test. No plugin rebuild, new dependency or validation retry was needed.

[Prior reader refusal](../build/native-format-closeout/validation-packet/qupath-native-provider/baseline.log)
[CLI checks](../build/native-format-closeout/validation-packet/qupath-native-provider/pytest.log)
[Native project suite](../build/native-format-closeout/validation-packet/qupath-native-provider/native.log)

## Delivered locally - keep QuPath relocation reviews bound to source availability

The native baseline published a held relocation review after the original
image reappeared. Automatic relocation now records the original path's
absence and verifies it before saving. A restored original requires a new
review. An explicit replacement remains bound to the chosen path.

The native QuPath project suite and 19 CLI checks passed. The baseline build
also repaired the native suite's assertion against calibration fields removed
from the internal saved-data decoder. The corrective build reused that test
object and rebuilt the changed project core. No validation retries were needed.

[Reproduced stale publication](../build/native-format-closeout/validation-packet/qupath-relocation-review/baseline.log)
[Native project suite](../build/native-format-closeout/validation-packet/qupath-relocation-review/native.log)
[Passing CLI checks](../build/native-format-closeout/validation-packet/qupath-relocation-review/pytest.log)

## Delivered locally - preserve omitted calibration in direct QuPath saved-data import

The prior CLI erased lower-priority calibration in four direct `.qpdata`
fixtures. The shared native importer resolves the XY pair, Z spacing and time
series independently from saved data, then entry metadata, then acquisition.
Explicit unknown stays unknown. A partial XY pair still reports a loss, and
saved timestamps require their own time unit.

Nineteen targeted axis and project CLI checks passed after one cached native
build. The baseline reproduced four failed omission cases with the unknown
control passing. The documentation patch needed a context correction. The
older Windows archive retains its prior source identity.

[Reproduced saved-data losses](../build/native-format-closeout/validation-packet/qpdata-calibration-precedence/baseline.log)
[Passing native CLI checks](../build/native-format-closeout/validation-packet/qpdata-calibration-precedence/pytest.log)

## Delivered locally - preserve acquisition XY when QuPath entry metadata omits it

The staged importer saved null XY when a project entry supplied only Z and
time. The native importer now keeps acquisition XY when both fields are
omitted. A complete explicit uncalibrated pair still clears XY. A partial pair
reports a loss and leaves native spacing intact if the user accepts that loss.

Thirteen project and axis CLI checks passed after one cached Python-off build.
The fixture retained 0.25 × 0.5 µm acquisition XY and the exact overridden
Z/time archive. Explicit unknown and partial pixel/physical pairs passed. The
baseline failure was reproduced before changing C++. The documentation patch
needed a context correction. The staged handoff retains its source identity.

[Reproduced calibration loss](../build/native-format-closeout/validation-packet/qupath-builder-independent-calibration/baseline.log)
[Passing CLI checks](../build/native-format-closeout/validation-packet/qupath-builder-independent-calibration/pytest.log)

## Delivered locally - preserve four-bit grayscale TIFF samples

The staged Windows reader refused an independently verified four-bit grayscale
TIFF. The shared packed reader now exposes exact UInt8 codes 0–15. The view
applies MINISBLACK or MINISWHITE polarity over the full stored range. Raw
crops and saved-project replay preserve those codes. The four-bit profile
requires lossless compression and most-significant-bit fill order.

Sixty-five TIFF CLI checks, six compound regressions and the native region
suite passed in a single cached Python-off build. The first fixture check used
tifffile's optional four-bit decoder, which is absent from this environment.
Pillow verified the displayed strip and rotated tile independently, with raw
codes checked against the authored input arrays. The prior Windows handoff
retains its original source identity.

[Staged reader refusal](../build/native-format-closeout/validation-packet/tiff-packed-gray/baseline.log)
[TIFF CLI checks](../build/native-format-closeout/validation-packet/tiff-packed-gray/pytest.log)
[Compound reader checks](../build/native-format-closeout/validation-packet/tiff-packed-gray/compound.log)
[Native region suite](../build/native-format-closeout/validation-packet/tiff-packed-gray/native.log)

## Verified locally - matched Windows native handoff

The offline collector bound all 15 reviewed cached dependency archives to the
current compiled-source fingerprint. The handoff manifest ties that companion
to the native runtime, SDK, application source and optional provider archives.
Every digest matches its scoped install, source-configure or admission receipt.
All seven checksum entries passed on reread. The handoff uses links to the
existing archive bytes and required no rebuild, download or retries.

This closes local staging for the declared Windows x64 profile. Publication
and any additional provider or platform claims remain separate. Continue the
goal with the next demonstrated workflow blocker in the application queue.

[Local handoff receipt](../build/native-format-closeout/validation-packet/full-native-install/material-review/windows-native-handoff/handoff.json)
[Installation notes](../build/native-format-closeout/validation-packet/full-native-install/material-review/windows-native-handoff/README.md)
[Checksums](../build/native-format-closeout/validation-packet/full-native-install/material-review/windows-native-handoff/SHA256SUMS.txt)
[Dependency companion admission](../build/native-format-closeout/validation-packet/full-native-install/material-review/current-dependency-companion/dependency-sources.json)

## Verified locally - co-install optional providers with the current Windows archive

The standalone optional archive preserves all 214 admitted files from the
MetaImage, VTI, counts/points TSV and geometry TSV packages, including their own
source and notices. All 21 checks passed against the current native archive.
They covered decoded pixels and calibrated axes, explicit reader options, frame
matching and refusal, ROI publication and desktop reopen. The offline export
preserved the exact count `9007199254740993` after source disconnection.

Manifest-owned removal deleted 288 files and preserved all 45 research and
validation files. No rebuild, new fixture, download or retry was needed. Other
providers preserve their existing source-scoped handoffs. Publication remains
a separate step.

[Optional provider archive](../build/native-format-closeout/validation-packet/full-native-install/material-review/provider-handoff/qupathxx-windows-x64-optional-providers.zip)
[Co-install and removal receipt](../build/native-format-closeout/validation-packet/full-native-install/material-review/provider-handoff/installation-removal.json)
[Workflow reports](../build/native-format-closeout/validation-packet/full-native-install/material-review/provider-handoff/research%20outputs/validation.json)

## Verified locally - stage the current Windows native payload and source

The exact current Windows runtime, SDK and corresponding source passed the
existing stage and ZIP admission guards. The archives contain 74, 217 and
1,094 members respectively. The source stage reconfigured offline with Python
disabled and produced the same compiled-source digest as the installed SDK.
The runtime archive passed static PE dependency verification for all 11
binaries. Admission tests returned 26 passes and one host symlink skip.

The first audit needed the older host archive's lineage record from its
companion inventory. The next pass surfaced changes to the packaging guard's
license vocabulary, which were reviewed as first-party source. The runtime
inspector required its candidate filename, so a same-file alias supplied it.
No binary rebuild or dependency download was required. This is a local
handoff. Optional provider packages and publication remain separate steps.

[Archive admission and hashes](../build/native-format-closeout/validation-packet/full-native-install/material-review/local-release/admission.json)
[Exact material inventory](../build/native-format-closeout/validation-packet/full-native-install/material-review/windows-current-materials.json)
[Source configure receipt](../build/native-format-closeout/validation-packet/full-native-install/material-review/source-configure.json)
[Windows runtime manifest](../build/native-format-closeout/validation-packet/full-native-install/material-review/native-runtime.json)

## Verified locally - dependency sources and Windows component membership

Checked all 17 members of the retained dependency companion, including all 15
reviewed source archives. Compared 16,792 dependency files byte-for-byte
against active source trees and 17 ImGui text files after CRLF normalization.
The OpenSlide source carrier, SDK runtime and version inventory matched the
retained records. No downloads or builds were needed.

The unrestricted install included 62 files from the upstream HDF5 install.
Staging the seven named CMake components yielded the other 289 files, with
the same hashes as the verified full install and the SDK HDF5 library and
notice intact. The first membership report omitted `include/hdf5.h` from the
upstream group. The corrected list matches the exact file-set difference.
The next release step is to review 35 staged files and 176 compiled inputs
without current exact admission records, plus separately selected providers.

[Dependency correspondence](../build/native-format-closeout/validation-packet/full-native-install/material-review/dependency-correspondence.json)
[Source and installation inventory delta](../build/native-format-closeout/validation-packet/full-native-install/material-review/membership-delta.json)
[Verified component stage inventory](../build/native-format-closeout/validation-packet/full-native-install/material-review/component-stage.json)

## Verified locally - full Windows native CMake install

The full install now contains the spatial-counts CLI, native project example,
desktop, SDK and both plugin development kits. One cached Python-off build
added the two previously absent executables and their spatial fixture and
updated batch, desktop and OMERO. All eight installed executables matched the
cached binaries. The compiled source digest matched the checkout input list.

Four focused native tests passed. Six installed command groups verified
retained count projection, native save/replay, desktop reopen and an external
CMake consumer built against the installed SDK. The spatial and example routes
refused existing output while preserving both project hashes. The full install
matched the CMake file manifest. Manifest-owned removal deleted 351 files and
preserved all ten user files byte-for-byte. No build or validation retries were
needed. This verifies a local full install. Release staging still requires
reviewed binary, corresponding source and optional-provider membership.

[Install and removal receipt](../build/native-format-closeout/validation-packet/full-native-install/installation-removal.json)
[Installed command reports](../build/native-format-closeout/validation-packet/full-native-install/commands.json)
[Exact compiled inputs](../build/native-format-closeout/validation-packet/full-native-install/SOURCE-INPUTS.sha256)
[Cached build](../build/native-format-closeout/validation-packet/full-native-install/build.log)

## Verified locally - installed bilevel TIFF label workflow

Relinked `qupathxx_labels` from the Python-off cache and ran seven commands
from a fresh Windows install with a system-only PATH. Stored foreground code 1
selected exactly 145 pixels and produced a sum of 43102 on the calibrated test
image. The saved label record verified with matching input and raster digests.
Manifest-owned removal deleted 71 installation files and preserved all nine
user files byte-for-byte, including an in-prefix note.

The unrestricted CMake install requested two unrelated optional targets that
were absent from the cache. Installing the default component into a fresh
prefix scoped the check to the required runtime. The first validation command
incorrectly passed `--reader` to `--raw`; auto-selection by `.tif` corrected it
without rebuilding. The shell rejected cleanup of the first partial install;
that copy remains. Continue with the next uncovered release workflow.

[Install and removal receipt](../build/native-format-closeout/validation-packet/tiff-bilevel/installed-label-workflow-02/installation-removal.json)
[Installed command reports](../build/native-format-closeout/validation-packet/tiff-bilevel/installed-label-workflow-02/commands.json)
[Label consumer build](../build/native-format-closeout/validation-packet/tiff-bilevel/label-build.log)

## Delivered locally - open plain bilevel TIFF without changing mask values

The retained reader refused an independently authored one-bit mask.
Plain bilevel TIFF now decodes packed rows to exact UInt8 values 0 and 1.
The default display uses the photometric polarity. LibTIFF normalizes both
FillOrder values before the shared packed reader sees the bytes.

Sixty-three TIFF CLI checks, six compound regressions and the native region
suite passed. Fixtures covered odd-width strips, rotated reverse-fill tiles,
CCITT Groups 3 and 4, padded display and byte-identical project replay.
One cached build reused LibTIFF. CMake regenerated the cached HDF5 build
settings during the build. No validation retries were needed. Continue
with the next demonstrated workflow gap.

[Retained refusal](../build/native-format-closeout/validation-packet/tiff-bilevel/baseline.log)
[Exact bilevel export](../build/native-format-closeout/validation-packet/tiff-bilevel/after.json)
[TIFF CLI tests](../build/native-format-closeout/validation-packet/tiff-bilevel/pytest.log)
[Compound regressions](../build/native-format-closeout/validation-packet/tiff-bilevel/compound.log)
[Native region tests](../build/native-format-closeout/validation-packet/tiff-bilevel/native.log)

## Delivered locally - reject TIFF palettes with missing color maps

The retained reader accepted three malformed palette declarations because
LibTIFF normalized them to grayscale or RGB. The native reader checks the
original inline photometric tag at selected IFDs and rejects a lost palette
before raw export, display or project creation. The bounded check restores
the original decoder cursor and runs during open.

Fifty-nine TIFF CLI checks, six compound regressions and the native region
suite passed. The retained malformed fixtures all refused. Tests covered
both byte orders, BigTIFF, SubIFDs and exact reads of valid sibling series.
One cached build reused LibTIFF. The first patch context did not match and
was corrected before compilation. No validation retries were needed.
Continue with the next demonstrated workflow gap.

[Retained malformed exports](../build/native-format-closeout/validation-packet/tiff-required-colormap/baseline.log)
[Guarded refusals](../build/native-format-closeout/validation-packet/tiff-required-colormap/refusals.log)
[TIFF CLI tests](../build/native-format-closeout/validation-packet/tiff-required-colormap/pytest.log)
[Compound regressions](../build/native-format-closeout/validation-packet/tiff-required-colormap/compound.log)
[Native region tests](../build/native-format-closeout/validation-packet/tiff-required-colormap/native.log)

## Delivered locally - preserve packed 4-bit TIFF palette indices

The retained reader refused a 4-bit palette TIFF with odd-width rows.
Native block reads now unpack most-significant-bit indices into UInt8
values 0–15 and skip row padding. Default previews keep the ColorMap and
orientation. The existing raw and display entry points share the unpacking.

Fifty-five TIFF CLI checks, six compound regressions and the native region
suite passed. The fixtures exercised odd-width strips, cross-tile crops,
PackBits, Deflate with orientation, source index bytes and saved replay.
The first fixture used a tifffile writer that requires a byte-aligned
palette shape. An authored TIFF checked independently through tifffile
reproduced the gap. One cached build reused LibTIFF. Continue with the next
demonstrated workflow gap.

[Retained refusal](../build/native-format-closeout/validation-packet/tiff-packed-palette/baseline.log)
[Packed index export](../build/native-format-closeout/validation-packet/tiff-packed-palette/after.json)
[TIFF CLI tests](../build/native-format-closeout/validation-packet/tiff-packed-palette/pytest-final.log)
[Compound regressions](../build/native-format-closeout/validation-packet/tiff-packed-palette/compound.log)
[Native region tests](../build/native-format-closeout/validation-packet/tiff-packed-palette/native.log)

## Delivered locally - preserve indexed plain-TIFF samples and palette display

The retained reader refused a tifffile-authored raster with a ColorMap.
Plain lossless UInt8 and UInt16 palette TIFF now preserves its stored
indices and renders the selected level's palette by default. Explicit
contrast or channel changes render the indices.

Fifty TIFF CLI checks, six compound regressions and the native region suite
passed. They covered exact indices, colored previews, padding, a rotated
big-endian fixture, per-level palettes and byte-identical saved replay.
The first malformed-map test found that LibTIFF silently recovers a missing
map as grayscale before native photometric inspection. This inherited
recovery led to the inline tag check above. One cached build reused LibTIFF. Continue with the
next demonstrated workflow gap.

[Retained refusal](../build/native-format-closeout/validation-packet/tiff-palette/baseline.log)
[Indexed raw export](../build/native-format-closeout/validation-packet/tiff-palette/after.json)
[TIFF CLI tests](../build/native-format-closeout/validation-packet/tiff-palette/pytest-final.log)
[Compound regressions](../build/native-format-closeout/validation-packet/tiff-palette/compound.log)
[Native region tests](../build/native-format-closeout/validation-packet/tiff-palette/native.log)

## Delivered locally - normalize explicit plain-TIFF orientations

The retained reader refused a valid right-top UInt16 raster. Plain TIFF now
maps all eight orientations into top-left raw and display coordinates while
reading only intersecting stored blocks. Dimensions and pitch follow swapped
axes. SubIFDs use one coherent orientation. The OME reader retained its
top-left restriction at this checkpoint. The later coherent raster profile
records the separate OME-XML geometry boundary above.

Forty-six TIFF CLI checks, six compound regressions and the native region
suite passed. Independent fixtures verified exact pixels, padded crops,
separate strips, interleaved tiles, coherent pyramids and saved raw and
display replay. The fixture producer emitted existing NumPy deprecation
warnings. One cached build reused LibTIFF. No build retries were needed.
Continue with the next demonstrated workflow gap.

[Retained refusal](../build/native-format-closeout/validation-packet/tiff-orientation/baseline.log)
[Oriented UInt16 result](../build/native-format-closeout/validation-packet/tiff-orientation/after.json)
[TIFF CLI tests](../build/native-format-closeout/validation-packet/tiff-orientation/pytest-final.log)
[Compound regressions](../build/native-format-closeout/validation-packet/tiff-orientation/compound.log)
[Native region tests](../build/native-format-closeout/validation-packet/tiff-orientation/native.log)

## Delivered locally - retain agreed plane calibration across OME-TIFF companions

The retained CLI omitted both axes for a two-file corpus with complete,
matching plane tables. The reader now requires each distinct resolved
metadata document to supply the same regular step for an inferred axis.
Both embedded copies and a shared BinaryOnly master replay through either
entry file. Conflicting or incomplete tables leave the affected axis unset.

Six compound CLI cases, 35 TIFF CLI checks and the native region suite
passed. The retained example now reports 0.375 µm and 0.125 s with the same
exact raw array. The matching and shared-master projects replayed that
calibration and preserved their bytes. One cached build recompiled the native
reader and reused its dependencies. No validation retries were needed.
Continue with the next demonstrated workflow gap.

[Retained bundle receipt](../build/native-format-closeout/validation-packet/ome-compound-calibration/before.json)
[Calibrated bundle receipt](../build/native-format-closeout/validation-packet/ome-compound-calibration/after.json)
[Compound CLI cases](../build/native-format-closeout/validation-packet/ome-compound-calibration/compound.log)
[TIFF CLI tests](../build/native-format-closeout/validation-packet/ome-compound-calibration/tiff.log)
[Native region tests](../build/native-format-closeout/validation-packet/ome-compound-calibration/native.log)

## Delivered locally - retain regular OME-TIFF Z spacing from plane metadata

The retained reader omitted Z spacing for an independently written stack with
positions at 0, 375 and 750 nm. The native reader now reports 0.375 µm using
the same regular-plane check as time intervals. The fallback requires a
complete single-file table, zero origin and explicit physical units. Declared
Pixels spacing takes precedence. Ambiguous positions leave spacing unset.

All 35 TIFF CLI checks and the native region suite passed. The baseline and
new raw bundles contain the same exact pixel payload, and saved-project replay
retained the recovered calibration and project identity. The test producer
emitted its existing NumPy shape deprecation warnings. One cached native
build reused LibTIFF. Continue with the next demonstrated workflow gap.

[Retained bundle receipt](../build/native-format-closeout/validation-packet/ome-plane-spacing/before.json)
[Calibrated bundle receipt](../build/native-format-closeout/validation-packet/ome-plane-spacing/after.json)
[TIFF CLI tests](../build/native-format-closeout/validation-packet/ome-plane-spacing/pytest.log)
[Native region tests](../build/native-format-closeout/validation-packet/ome-plane-spacing/native.log)

## Delivered locally - open ENVI rasters with named companion suffixes

Spectral Python wrote and reopened same-stem RAW, BIL, BIP and BSQ pairs.
The retained ENVI 1.4.0 plugin rejected all four explicit selections.
Version 1.5.0 opens them through the existing saved companion option.
Each selection replayed exact raw bytes after editing a competing IMG file.
A missing selected raster refused, and replacing it required source review.

All 95 checks passed from the relocated Windows ZIP. The package inventory
verified 20 archive and source members. The pinned 1.4.0 UInt64 project still
replayed exact bytes and required review before using 1.5.0. The first
producer check compared a relative path to an absolute path and passed
after resolving both. One cached module build reused the host and SDK probe.
Continue with the next demonstrated workflow gap.

[Retained RAW selection refusal](../build/envi-reader/explicit-suffixes/raw-baseline.log)
[Relocated validation](../build/envi-reader/explicit-suffixes/relocated-validation/validation.json)
[Pinned-version refusal](../build/envi-reader/explicit-suffixes/pinned-refusal.log)
[Source and module identity](../build/envi-reader/package-1.5.0/SOURCE.json)

## Delivered locally - preserve 64-bit ENVI raster values

Spectral Python 0.24 wrote and reopened both documented Int64 and UInt64
profiles. ENVI 1.3.0 refused both. The 1.4.0 plugin reads both byte orders
through the existing native ABI, including the signed and unsigned limits.
Integer ignore values are compared at stored precision. Adjacent codes
above the exact binary64 range retain distinct validity.

All 79 checks passed from the relocated Windows ZIP, including byte-identical
saved-project raw replay for both types. Twenty package and archive members
passed the existing source and license inventory. A project pinned to 1.3.0
rendered its original RGB and required review before using 1.4.0. One cached
module build reused the native host and SDK probe. No validation retries
were needed. Continue with the next demonstrated workflow gap.

[Rejected Int64](../build/envi-reader/wide-integers/signed64-baseline.log)
[Rejected UInt64](../build/envi-reader/wide-integers/unsigned64-baseline.log)
[Relocated validation](../build/envi-reader/wide-integers/relocated-validation/validation.json)
[Pinned-version refusal](../build/envi-reader/wide-integers/pinned-refusal.log)
[Source and module identity](../build/envi-reader/package-1.4.0/SOURCE.json)

## Delivered locally - honor declared ENVI display bands

ENVI 1.3.0 reads the documented one-band grayscale and three-band RGB
selections through the existing display scaling and native ABI. A missing
field keeps the original first-band grayscale. Selected invalid samples
produce transparent pixels, and raw export keeps native band order and
values. Reopening the saved RGB project rendered byte-identical PAM.

All 65 checks passed from the relocated Windows ZIP. The existing staging
inventory verified 20 package and archive members, including corresponding
source and licenses. The pinned 1.2.0 project rendered the original
grayscale and required explicit review before using 1.3.0. One cached
module build reused the native host and SDK probe. Continue with the next
demonstrated workflow gap.

[Retained refusal](../build/envi-reader/default-bands/baseline.log)
[Relocated validation](../build/envi-reader/default-bands/relocated-validation/validation.json)
[Pinned-version refusal](../build/envi-reader/default-bands/old-refusal.log)
[Source and module identity](../build/envi-reader/package-1.3.0/SOURCE.json)

## Delivered locally - protect saved project paths during matrix indexing

CSV indexing published a QMX at an offline QPR locator and AnnData Zarr
indexing published at the source project's WAL path. File and store indexing
now use the native project and SQLite companion guards. The combined native
store route checks both proposed destinations.

Twenty-three CLI checks, three native import cases and the native association
suite passed. The CSV and AnnData Zarr outputs match the retained QMX bytes
and receipts. Both remained available as owned associations after their
standalone copies were moved. The first baseline needed absolute manifest
paths. The association fixture exceeded the Windows path limit under the
CTest working directory and passed from the shorter build directory with
the same executable. One cached build reused the staged providers. Continue
with the next demonstrated workflow gap.

[CSV collision receipt](../build/native-format-closeout/validation-packet/matrix-index-retained-assets/file-baseline.json)
[AnnData Zarr collision receipt](../build/native-format-closeout/validation-packet/matrix-index-retained-assets/store-baseline.json)
[Index and attachment checks](../build/native-format-closeout/validation-packet/matrix-index-retained-assets/validation/validation.json)
[Native import cases](../build/native-format-closeout/validation-packet/matrix-index-retained-assets/native-import.log)
[Native association suite](../build/native-format-closeout/validation-packet/matrix-index-retained-assets/native-short-path.log)

## Delivered locally - make default saved raw contour replay usable

The retained CLI refused a complete 35-sample stream under the default
limits. The native contour authority now admits the peak of file verification
and live work. Live work still includes the saved producer budget, retained
file metadata and framing checks. Verification, geometry and publication
retain their independent limits.

Forty-seven CLI checks passed. Default replay reopened and exported the same
GeoJSON bytes as the retained fresh run. Explicit caps refused a live budget
that omitted retained history and a total smaller than the file allowance.
The existing contour suite passed 15 checks. Its three installed-SDK reopen
checks used a module that predates the current project schema. Native reopen
verified the affected default replay. The first checker invocation needed a
sibling output directory to preserve relative image paths. One cached build
recompiled native contour admission. Continue with the next demonstrated gap.

[Default refusal baseline](../build/native-format-closeout/validation-packet/raw-contour-default-budget/baseline.log)
[Default replay and quota checks](../build/native-format-closeout/validation-packet/raw-contour-default-budget-final/validation.json)
[Reopened exact geometry](../build/native-format-closeout/validation-packet/raw-contour-default-budget/reopened-geometry.json)
[Existing contour regression results](../build/native-format-closeout/validation-packet/raw-contour-default-budget/pytest.log)

## Delivered locally - protect retained assets from raw classifier outputs

Raw training published a checkpoint at a QMX journal path and export wrote a
prediction stream at an offline QPR locator. Training, prediction, contour
publication and replay now use the shared native project export guard along
with live reader and multi-output checks.

Forty-five targeted CLI checks and 12 existing CLI regressions passed. The
backend-OFF test skipped on the enabled build. Training and prediction worked
with the archives offline. Model, training table and prediction identities
matched the retained baseline, and contour replay kept the geometry summary
and object IDs. The cached build reused the classifier kernels. The harness
needed an explicit 16 MiB replay file budget because the default combined
budget refused the fixture. The default replay admission is resolved above.

[Training checkpoint collision](../build/native-format-closeout/validation-packet/raw-classifier-retained-assets/training-baseline.jsonl)
[Prediction stream collision](../build/native-format-closeout/validation-packet/raw-classifier-retained-assets/prediction-baseline.jsonl)
[Path protection and replay](../build/native-format-closeout/validation-packet/raw-classifier-retained-assets-replay/validation.json)
[CLI regressions](../build/native-format-closeout/validation-packet/raw-classifier-retained-assets/pytest.log)

## Delivered locally - protect retained analysis files from saved-project region exports

The retained region CLI wrote raw TIFF and display crops at reserved matrix
companion paths and a raw bundle at an offline reduction locator. All three
routes now use the shared native project output guard before reading and at
publication. Valid exports kept the exact raw and display payloads, saved
series and project identity with the archives online or offline.

Thirty-three targeted CLI checks, four existing project TIFF CLI tests and
the native region suite passed. The fixture needed an explicit native TIFF
reader for its series option. The checker now uses the display and raw routes'
existing distinct error statuses. One cached build recompiled the shared
region exporter. Continue with the next demonstrated workflow gap.

[Reproduced collisions](../build/native-format-closeout/validation-packet/region-retained-assets/baseline.jsonl)
[Path protection and replay](../build/native-format-closeout/validation-packet/region-retained-assets-replay/validation.json)
[TIFF CLI regressions](../build/native-format-closeout/validation-packet/region-retained-assets/pytest-replay.log)
[Native region suite](../build/native-format-closeout/validation-packet/region-retained-assets/native.log)

## Delivered locally - protect retained analysis assets from pixel and stain exports

The retained pixel-stream and stain-estimate commands published unrelated
results at offline matrix and reduction locators and reserved QMX sidecar
paths. Both now use the shared native project output guard. Exports to
independent destinations still work with the analysis archives offline.

Eighteen targeted CLI checks, four existing CLI regressions and two native
stain suites passed. The pixel labels, validity and vote fractions exactly
matched the retained stream, and the stain input and model hashes matched
the retained estimate. Source project, model and archive bytes were preserved.
The harness initially compared whole-stream digests that include capture
provenance and read only the last line of the stain JSON. It now verifies each
stream's own digest, compares arrays exactly and parses the complete report.
One cached build reused the analysis kernels. Continue with the next
demonstrated workflow gap.

[Offline publication baseline](../build/native-format-closeout/validation-packet/rgb-retained-assets/offline-baseline.jsonl)
[Path protection and replay](../build/native-format-closeout/validation-packet/rgb-retained-assets-replay/validation.json)
[CLI regressions](../build/native-format-closeout/validation-packet/rgb-retained-assets/pytest.log)
[Native stain suites](../build/native-format-closeout/validation-packet/rgb-retained-assets/native.log)

## Delivered locally - protect retained analysis assets from TMA exports

The retained case, QPTMA and cohort exports published unrelated bytes at
offline QMX/QPR locators and a QMX journal path. Detection, QPTMA import and
export, and case and cohort export now use the existing native project output
guard. Valid exports produced identical bytes with the archives online or
offline.

Thirty-five targeted CLI checks, 14 reviewed-replay checks, 21 earlier output
checks and two native suites passed. The first validation copy changed its
relative image link. Placing the copy beside the original and replaying import
from the seed without a grid corrected the harness. One cached build reused the
detector. Continue with the next demonstrated workflow gap.

[Offline asset baseline](../build/native-format-closeout/validation-packet/tma-retained-assets/offline-baseline.jsonl)
[QMX companion baseline](../build/native-format-closeout/validation-packet/tma-retained-assets/companion-baseline.log)
[Retained-asset checks](../build/native-format-closeout/validation-packet/tma-retained-assets-complete/validation.json)
[Reviewed replay regressions](../build/native-format-closeout/validation-packet/tma-retained-assets/review-regression/validation.json)
[Existing workflow regressions](../build/native-format-closeout/validation-packet/tma-retained-assets/companion-regression/validation.json)
[Native suites](../build/native-format-closeout/validation-packet/tma-retained-assets/native.log)

## Delivered locally - bind reviewed TMA proposals to their source

The retained CLI applied a detected QPTMA to a different image of the same
size. Detection now optionally emits a review digest covering the project
bytes, selected image ID, observed reader frame and exact QPTMA bytes.
Reviewed import checks the digest and rechecks the inputs before publication.
Ordinary preview still emits identical QPTMA bytes.

Fourteen CLI checks passed, including embedded and linked replay, reopening the
saved grid and refusals for the wrong image, edited proposal, malformed digest,
changed project and changed linked image. All 21 prior TMA CLI checks and both
native suites passed. The first baseline used a smaller control image and hit
the ROI bounds check. Adding a same-size linked image reproduced the issue.
One cached build reused the detector. Continue with the next demonstrated gap.

[Reproduced wrong-image import](../build/native-format-closeout/validation-packet/tma-reviewed-proposal/wrong-image-baseline.log)
[Review and replay checks](../build/native-format-closeout/validation-packet/tma-reviewed-proposal/validation/validation.json)
[Existing workflow checks](../build/native-format-closeout/validation-packet/tma-reviewed-proposal/output-regression/validation.json)
[Native suites](../build/native-format-closeout/validation-packet/tma-reviewed-proposal/native.log)

## Delivered locally - protect retained analysis files from project exports

Cell-table export could overwrite either an owned QMX or its project's
retained QPR reduction archive. Disposable controls reproduced both losses.
Cell, expression and format exports now share the native project save guard
for those assets. It protects offline locators, file aliases and the QMX
SQLite companions. Managed asset publication retains its existing exact-reuse
checks.

Fifteen CLI checks, two rebuilt native suites, 25 matrix recovery regressions
and 16 saved-analysis regressions passed. The source project and both archives
kept their exact hashes. Normal cell-table replacement, GeoJSON export with an
offline matrix and byte-identical plot export passed. One cached build
recompiled the affected native units. No validation retries were needed.
Continue with the next demonstrated workflow gap.

[Reproduced replacements](../build/native-format-closeout/validation-packet/protected-retained-assets/baseline.json)
[Export checks](../build/native-format-closeout/validation-packet/protected-retained-assets/validation/validation.json)
[Native suites](../build/native-format-closeout/validation-packet/protected-retained-assets/native.log)
[Recovery regression](../build/native-format-closeout/validation-packet/protected-retained-assets/backup-regression/validation.json)
[Analysis regression](../build/native-format-closeout/validation-packet/protected-retained-assets/analysis-regression/validation.json)

## Delivered locally - create exact standalone backups of saved QMX matrices

`project-matrix-backup` validates either kind of saved association, streams
the full QMX and checks the copied SHA-256 before atomic new-only publication.
The native managed-copy workflow uses the same verified byte-copy function.
Existing output, SQLite companions, protected inputs and missing or changed
sources refuse. The exact backup restored an owned matrix while its original
was offline. The external backup reopened as a standalone QMX.

Twenty-five backup and restore checks, 30 CSV and position regressions and
the rebuilt native association suite passed. The cache recompiled the
matrix helpers, CLI and generated provenance. No validation retries were
required. Continue with the next demonstrated workflow gap.

[Retained command refusal](../build/native-format-closeout/validation-packet/project-matrix-backup/baseline.log)
[Backup and restore checks](../build/native-format-closeout/validation-packet/project-matrix-backup/validation/validation.json)
[Table regressions](../build/native-format-closeout/validation-packet/project-matrix-backup/table-regression/validation.json)
[Native association suite](../build/native-format-closeout/validation-packet/project-matrix-backup/native.log)

## Verified locally - current Windows headless handoff

The current format and batch executables both embed the verified checkout
source digest. The relocated native-only install verified the admitted ENVI
reader and four co-installed matrix packages. CSV, TSV, Arrow and Parquet
indexed and attached against the saved image, then exported bit-exact owned
values, positions and frame identities after the original files were removed
from their paths. Both manifest orders produced the same catalog identities.

Eighteen installed matrix and catalog commands, 21 TMA checks, and 16 saved
analysis checks passed. Removing the 159 verified installation files
preserved all 49 user and validation files byte-for-byte. One cached build
updated the batch frontend against the already current core. All plugin
packages and dependent DLLs were reused. This is local validation of the
Windows headless workflow. Continue with the next demonstrated gap.

[Handoff and removal receipt](../build/native-format-closeout/validation-packet/current-headless-handoff/validation.json)
[Exact compiled inputs](../build/native-format-closeout/validation-packet/current-headless-handoff/SOURCE-INPUTS.sha256)
[Build log](../build/native-format-closeout/validation-packet/current-headless-handoff/build.log)

## Delivered locally - protect SQLite companion paths in native Save As

The shared project save boundary now rejects a destination that occupies an
input SQLite companion path. A new database also requires an unoccupied
journal, WAL and shared-memory namespace. New-only saves recheck that
namespace immediately before publication. Synchronous and background
Save As share the guard. In-place updates passed their existing checks.

The retained format CLI reproduced both source and destination collisions.
Ten CLI checks passed after the fix, along with 24 matrix relink and ownership
regressions and both native project and QPTMA suites. The native project suite
also checked all three companion names through synchronous and background
save, a late companion, clean Save As and subsequent in-place save. The
existing cache rebuilt the affected core units and consumers in one invocation.
Continue with the next demonstrated workflow gap.

[Source collision baseline](../build/native-format-closeout/validation-packet/project-save-companions/source-baseline.log)
[Destination collision baseline](../build/native-format-closeout/validation-packet/project-save-companions/destination-baseline.log)
[CLI checks](../build/native-format-closeout/validation-packet/project-save-companions/cli/validation.json)
[Native suites](../build/native-format-closeout/validation-packet/project-save-companions/native.log)
[Matrix regressions](../build/native-format-closeout/validation-packet/project-save-companions/matrix-regression/validation.json)

## Delivered locally - protect SQLite paths during saved analysis publication

Cell-table export and new aggregation and differential-expression projects
reject a destination reserved for the input project's SQLite journal, WAL
or shared-memory file. All three retained routes published into reserved
paths on disposable copies. Native expression publication now uses the same
project output capture as saved result export. Cell-table export uses the
shared database guard and opens the input without journal recovery.

Sixteen CLI checks passed, including nine reserved-path refusals, exact
aggregation and differential replay, and byte-identical normal tables.
Three focused native suites and all 21 TMA checks passed. A retained
reduction plot exported byte-for-byte. The CLI checker initially expected
the TMA route's error status. Using the batch dispatcher's existing exit
status 2 resolved it without a rebuild. Continue with the next demonstrated
workflow gap.

[Cell-table baseline](../build/native-format-closeout/validation-packet/analysis-protected-project-assets/cell-baseline.log)
[Aggregation baseline](../build/native-format-closeout/validation-packet/analysis-protected-project-assets/agg-baseline.log)
[Differential baseline](../build/native-format-closeout/validation-packet/analysis-protected-project-assets/de-baseline.log)
[CLI checks](../build/native-format-closeout/validation-packet/analysis-protected-project-assets/verified/validation.json)
[Native checks](../build/native-format-closeout/validation-packet/analysis-protected-project-assets/native.log)

## Delivered locally - protect project SQLite companion paths from TMA commands

The native batch commands now reject output paths reserved for the input
project's SQLite journal, WAL or shared-memory file. TMA preview, QPTMA import
and export, and case and cohort CSV export share the same guard. The retained
batch executable wrote a proposal to a WAL path and a QPTMA export to a
journal path on disposable fixture copies.

Twenty-one CLI checks passed, including all 15 reserved-path refusals and
byte-exact normal exports and proposal/apply/reopen. The rebuilt QPTMA project
and TMA dearray native suites passed. The first baseline invocation selected
the inactive fixture's control image. Switching to the documented image ID
reproduced the failure. The cached build recompiled the small guard consumers
and reused the detector. Continue with the next demonstrated workflow gap.

[WAL baseline](../build/native-format-closeout/validation-packet/tma-protected-project-assets/detect-baseline-image2.log)
[Journal baseline](../build/native-format-closeout/validation-packet/tma-protected-project-assets/export-baseline-image2.log)
[CLI checks](../build/native-format-closeout/validation-packet/tma-protected-project-assets/validation/validation.json)
[Native suites](../build/native-format-closeout/validation-packet/tma-protected-project-assets/native.log)

## Delivered locally - import normalized TSV expression matrices

TSV Matrix 1.0.0 imports native ABI2 counts and continuous values with explicit
column selection, ordered UTF-8 identifiers and literal quotes. It shares the
bounded interpreter with CSV Matrix 1.0.1. Both feed the native position join
and owned QMX. The original normalized TSV refused by the ABI1 count provider
now imports and reopens with exact Float64 bits.

Each relocated ZIP passed three imports and three exact offline reopens.
TSV passed 15 refusals and CSV passed 17. Both 23-member packages passed
license and material admission. The three matrix providers co-installed
with zero catalog diagnostics. The first TSV compile warned about a
constant parser branch. Moving it to compile-time dispatch resolved the
warning in both cached plugin builds. The host and probe were reused.
Continue with the next demonstrated workflow gap.

[Legacy refusal](../build/native-format-closeout/validation-packet/typed-tsv-matrix/baseline.log)
[Original TSV reopen](../build/native-format-closeout/validation-packet/typed-tsv-matrix/legacy-shape-reopened.json)
[TSV relocated checks](../build/native-format-closeout/validation-packet/typed-tsv-matrix/tsv-relocated/validation.json)
[CSV relocated checks](../build/native-format-closeout/validation-packet/typed-tsv-matrix/csv-relocated/validation.json)
[Co-installed catalog](../build/native-format-closeout/validation-packet/typed-tsv-matrix/coinstalled.json)
[TSV source and binary identity](../build/native-format-closeout/validation-packet/typed-tsv-matrix/tsv-matrix-1.0.0/SOURCE.json)
[CSV source and binary identity](../build/native-format-closeout/validation-packet/typed-tsv-matrix/csv-matrix-1.0.1/SOURCE.json)

## Delivered locally - save an external project matrix as an owned QMX

`project-matrix-own` uses deferred native attachment and Save As to put an
exact QMX in a new project's managed directory. The input project and its
external locator stay untouched. The association ID and matrix digest are
preserved. Two new projects sharing a destination directory reused the same
exact asset, then reopened after both external locations were offline.

Twenty-four CLI checks passed across ownership and external relink, including
exact sparse replay, managed-file and companion collisions, and invalid
replacement refusal. The cached build recompiled the native attachment
helper, generated provenance and the format CLI. No validation retries were
needed. Continue with the next demonstrated workflow gap.

[Retained refusal](../build/native-format-closeout/validation-packet/project-matrix-own/baseline.log)
[Ownership and relink checks](../build/native-format-closeout/validation-packet/project-matrix-own/validation/validation.json)
[Cached build](../build/native-format-closeout/validation-packet/project-matrix-own/build.log)

## Delivered locally - relink a moved external project matrix

`project-matrix-relink` uses the native exact relink service and atomic
new-project publication. The input project retains its original link. The
output preserves the association, matrix digest and external ownership.
An unchanged named project folder can be relocated with its relative QMX
and replay the exact values. Destination checks protect both source paths,
the original offline locator and SQLite companions.

Fifteen CLI checks passed, including exact sparse counts after relocating
the new project tree, wrong bytes, destination collisions and owner-mode
refusal. All 15 owned restore regressions also passed with the new host.
The cached build recompiled the native attachment helper, generated
provenance and format CLI. No validation retries were needed.
Continue with the next demonstrated workflow gap.

[Retained refusal](../build/native-format-closeout/validation-packet/project-matrix-relink/baseline.log)
[Relink and relocation checks](../build/native-format-closeout/validation-packet/project-matrix-relink/validation/validation.json)
[Owned restore regressions](../build/native-format-closeout/validation-packet/project-matrix-relink/restore-regression/validation.json)
[Cached build](../build/native-format-closeout/validation-packet/project-matrix-relink/build.log)

## Delivered locally - restore an owned project matrix from exact backup bytes

`project-matrix-restore` uses the saved association's exact digest and
dimensions to restore a missing managed QMX. The project database stays
read-only. Concurrent attempts can reuse the winner's copy after verifying its
identity. Wrong backups, external links, conflicting destination files and
SQLite companions refuse.

Fifteen CLI checks passed, including fresh-folder recovery, exact offline
counts replay, unchanged project bytes and idempotent retry with the original
file timestamp. The rebuilt native association suite passed from a short
working directory. The cached build recompiled the shared project helper and
its dependent native units. The focused validation needed no retries.
Continue with the next demonstrated workflow gap.

[Retained refusal](../build/native-format-closeout/validation-packet/project-matrix-restore/baseline.log)
[Recovery checks](../build/native-format-closeout/validation-packet/project-matrix-restore/validation/validation.json)
[Native association suite](../build/native-format-closeout/validation-packet/project-matrix-restore/native.log)

## Delivered locally - export saved matrix positions for offline CSV interchange

`project-matrix-export-positions` emits the native three-column position TSV
from verified QMX coordinates in observation order. Both exports share source,
project and SQLite companion protection. The TSV needs stored level-zero image
pixels and the CLI reports the exact saved frame and channel, Z and time.
CSV and TSV now re-import together when the original inputs are offline.

Thirty end-to-end checks passed, including exact joined counts, continuous
values and positions on owned offline reopen, all 1,024 sparse positions,
signed zero and subnormal coordinate bits, and nonspatial and source-alias
refusals. A separate retained C1/Z1/T1 project preserved its frame hash and
nondefault plane in the export report. One cached build recompiled the
matrix and CLI sources and the generated provenance unit. No retries or plugin
rebuilds were needed. Continue with the next demonstrated workflow gap.

[Retained refusal](../build/native-format-closeout/validation-packet/project-matrix-positions/baseline.log)
[Round-trip checks](../build/native-format-closeout/validation-packet/project-matrix-positions/validation/validation.json)
[Nondefault-plane export](../build/native-format-closeout/validation-packet/project-matrix-positions/nonzero-plane.json)

## Delivered locally - export selected saved expression features to CSV

`project-matrix-export-csv` atomically writes a new dense value table from a
verified saved QMX. It preserves requested column order, source observation
order, exact UInt64 and binary64 values, and normalizes absent sparse entries
to measured zero. The CLI reports the source matrix digest and value domain.
The projection is limited to 63 selected columns, 100,000 observations and
64 MiB. QMX remains the format for retaining sparse entry presence and frame
and source provenance.

Seventeen checks passed, including exact count and continuous values after
CSV re-import and owned offline reopen, all 1,024 sparse projected rows, and
source and existing destination protection. A negative control initially
expected the wrong alias diagnostic. Correcting the test reused the binary.
The existing cache rebuilt only the expression matrix and generated
provenance units and the format CLI. No plugin rebuild was required.
Continue with the next demonstrated workflow gap.

[Retained refusal](../build/native-format-closeout/validation-packet/project-matrix-csv/baseline.log)
[Export and re-import checks](../build/native-format-closeout/validation-packet/project-matrix-csv/verified/validation.json)
[Cached build](../build/native-format-closeout/validation-packet/project-matrix-csv/build.log)

## Delivered locally - discover and inspect matrices saved in a project

`project-matrices` lists saved associations for one image, including their
resolved QMX paths, digests, ownership and dimensions. It can recover
association IDs even when the QMX is missing. `project-matrix-info` opens the
association using its exact saved digest and shares the standalone metadata
report, including the stored frame, domain and input receipt. Both commands
work with the original inputs and plugins offline.

Fifteen CLI checks passed on the empty and two-matrix projects, with exact
receipt comparisons and separate missing/altered QMX checks. Retained AnnData
Zarr store provenance also matched standalone metadata. The cached build
recompiled the CLI and generated provenance unit. The retained native spatial
suite passed. The native project suite hit a Windows path-length limit while
writing its fixture in the deep build directory and passed from a shorter
working directory without a rebuild. Continue with the next demonstrated gap.

[List baseline](../build/native-format-closeout/validation-packet/project-matrix-catalog/project-matrices-baseline.log)
[Inspection baseline](../build/native-format-closeout/validation-packet/project-matrix-catalog/project-matrix-info-baseline.log)
[CLI checks](../build/native-format-closeout/validation-packet/project-matrix-catalog/validation/validation.json)
[AnnData provenance](../build/native-format-closeout/validation-packet/project-matrix-catalog/store-inspected.json)
[Native project suite](../build/native-format-closeout/validation-packet/project-matrix-catalog/native-short-path.log)

## Delivered locally - import wide expression matrices from CSV

CSV Matrix 1.0.0 supplies exact ordered text IDs, UInt64 raw counts or finite
binary64 continuous values through matrix ABI2. The native host joins the
original position TSV and reopens owned QMX offline. The original quoted CSV
that the legacy TSV provider refused now preserves commas, doubled quotes,
leading zeros, signed zero and subnormal values. The relocated module lists
alongside the existing counts and point TSV providers with zero diagnostics.

Three imports, three exact offline reopens and 17 refusals passed against
both the build module and the relocated ZIP. The 22-member Windows x64 package
passes inventory admission. The initial build exposed a shadowed generic
parameter, renamed in the cached rebuild. The initial long-row control
replaced a field instead of adding one. Correcting the fixture passed without
a further module build. The native host and SDK were reused.
Continue with the next demonstrated workflow gap.

[Legacy TSV refusal](../build/native-format-closeout/validation-packet/csv-matrix/legacy.log)
[Native checks](../build/native-format-closeout/validation-packet/csv-matrix/verified/validation.json)
[Relocated package checks](../build/native-format-closeout/validation-packet/csv-matrix/relocated/validation.json)
[Combined catalog](../build/native-format-closeout/validation-packet/csv-matrix/coinstalled-catalog.json)
[Source and binary identity](../build/native-format-closeout/validation-packet/csv-matrix/package-1.0.0/SOURCE.json)

## Delivered locally - read Arrow dictionaries of floating-point values

Arrow Matrix 1.3.0 and Arrow Points 1.6.0 read dictionary-encoded Float32 and
Float64 values through the shared native resolver and delta decoder. Native
reopen retained point coordinates, exact Float32 widening, and Float64 bits
including signed zero and subnormals. The existing count and ID type checks
still apply, and selected nulls and nonfinite values refuse. Both relocated
Windows ZIPs opened and replayed their original refused PyArrow Feather file.

Matrix passed 12 imports, 12 exact reopens and 21 refusals. Points passed
23 imports, 23 exact reopens and 30 refusals. Both 42-member archives passed
admission. Each cached module built once, with existing codec, host and
probe binaries reused. There were no build or validation retries.
Continue with the next demonstrated application or format gap.

[Retained matrix refusal](../build/native-format-closeout/validation-packet/arrow-dictionary-floats/matrix-baseline.log)
[Retained point refusal](../build/native-format-closeout/validation-packet/arrow-dictionary-floats/points-baseline.log)
[Matrix checks](../build/native-format-closeout/validation-packet/arrow-dictionary-floats/matrix-validation/validation.json)
[Point checks](../build/native-format-closeout/validation-packet/arrow-dictionary-floats/points-validation/validation.json)
[Relocated package checks](../build/native-format-closeout/validation-packet/arrow-dictionary-floats/relocated.json)
[Matrix source and binary identity](../build/native-format-closeout/validation-packet/arrow-dictionary-floats/arrow-matrix-1.3.0/SOURCE.json)
[Point source and binary identity](../build/native-format-closeout/validation-packet/arrow-dictionary-floats/arrow-points-1.6.0/SOURCE.json)

## Delivered locally - read Arrow dictionaries of integer values

Arrow Matrix 1.2.0 and Arrow Points 1.5.0 share bounded dictionary index
resolution and exact signed/unsigned dictionary delta decoding. Matrix IDs
and counts and point IDs and integer coordinates reach the same validation
and native publication paths as plain columns. The original PyArrow Feather
UInt64 file refused by Matrix 1.1.0 now joins to positions and reopens from
owned QMX. The relocated point plugin preserves IDs through UINT64_MAX from
compressed deltas. Floating-point dictionaries remain outside both profiles.

Matrix passed 11 imports, 11 exact offline reopens and 18 refusals. Points
passed 21 imports, 21 exact reopens and 28 refusals. Both 42-member packages
passed admission and relocation checks. Each cached plugin built once, with
the existing host, probes and codec builds reused. The baseline exposed a
text-only error shared by both providers. The shared decoder now owns the
typed lookup and its diagnostic. Continue with the next demonstrated gap.

[Retained refusal](../build/native-format-closeout/validation-packet/arrow-dictionary-integers/baseline.log)
[Matrix checks](../build/native-format-closeout/validation-packet/arrow-dictionary-integers/matrix-validation/validation.json)
[Point checks](../build/native-format-closeout/validation-packet/arrow-dictionary-integers/points-validation/validation.json)
[Relocated package checks](../build/native-format-closeout/validation-packet/arrow-dictionary-integers/relocated.json)
[Matrix source and binary identity](../build/native-format-closeout/validation-packet/arrow-dictionary-integers/arrow-matrix-1.2.0/SOURCE.json)
[Point source and binary identity](../build/native-format-closeout/validation-packet/arrow-dictionary-integers/arrow-points-1.5.0/SOURCE.json)

## Delivered locally - join exact integer observation IDs in columnar matrices

Arrow and Parquet matrix plugins 1.1.0 join all eight signed and unsigned
integer ID widths to position TSV by exact decimal spelling. Arrow keeps
existing text and dictionary-text IDs, and Parquet reuses its typed point ID
decoder. An out-of-order join preserved IDs through UINT64_MAX, original UInt64
counts and exact saved-image coordinates in owned QMX. Both relocated ZIP
plugins imported the original PyArrow UInt64 tables and reopened the owned
result with no format plugin loaded.

Arrow passed 10 imports, 10 exact reopens and 16 refusals. Parquet passed
11 imports, 11 exact reopens and 19 refusals. The archives passed 42- and
48-member inventory checks respectively. Two test controls needed the actual
missing-position diagnostic and a duplicate ID present in the positions table.
The older SDK computed an obsolete reader-frame digest. Relinking only the
probe against the host cache's NativeSDK component restored exact agreement.
The host and decoder builds were reused. Continue with the next demonstrated
application or format gap.

[Retained Arrow refusal](../build/native-format-closeout/validation-packet/columnar-matrix-integer-ids/arrow-baseline.log)
[Retained Parquet refusal](../build/native-format-closeout/validation-packet/columnar-matrix-integer-ids/parquet-baseline.log)
[Arrow checks](../build/native-format-closeout/validation-packet/columnar-matrix-integer-ids/arrow-verified/validation.json)
[Parquet checks](../build/native-format-closeout/validation-packet/columnar-matrix-integer-ids/parquet-verified/validation.json)
[Relocated package checks](../build/native-format-closeout/validation-packet/columnar-matrix-integer-ids/relocated.json)
[Arrow source and binary identity](../build/native-format-closeout/validation-packet/columnar-matrix-integer-ids/arrow-matrix-1.1.0/SOURCE.json)
[Parquet source and binary identity](../build/native-format-closeout/validation-packet/columnar-matrix-integer-ids/parquet-matrix-1.1.0/SOURCE.json)

## Delivered locally - import exact integer IDs from Arrow IPC point tables

Arrow Points 1.4.0 preserves plain signed and unsigned 8-, 16-, 32- and 64-bit
IDs through the bounded IPC and Feather v2 paths. PyArrow fixtures exercised
all widths and uncompressed, LZ4 and Zstandard record batches. Reopening without
the source table preserved each exact decimal ID, declared type and point
coordinate. Null, duplicate and floating-point IDs refused. The relocated
package imported the original UInt64 fixture through UINT64_MAX.

Nineteen imports, 19 exact reopens and 26 refusals passed. Forty-two package
and ZIP members passed admission. The existing decoder, host and SDK probe
were reused. One cached plugin build sufficed. Continue with the next
demonstrated format or application gap.

[Retained refusal](../build/native-format-closeout/validation-packet/arrow-point-integer-ids/baseline.log)
[Native validation](../build/native-format-closeout/validation-packet/arrow-point-integer-ids/validation/validation.json)
[Relocated import](../build/native-format-closeout/validation-packet/arrow-point-integer-ids/relocated.json)
[Source and binary identity](../build/native-format-closeout/validation-packet/arrow-point-integer-ids/package-1.4.0/SOURCE.json)

## Delivered locally - import exact integer IDs from Parquet point tables

Parquet points 1.1.0 and GeoParquet 1.3.1 share a native physical-column ID
decoder. All eight signed/unsigned widths retained their exact decimal IDs
and declared types after offline reopen. Refusals covered null, duplicate
and floating-point IDs. The relocated points plugin imported and re-exported
the original UInt64 values above 2^53 and through UINT64_MAX.

Parquet points passed 11 imports, 11 exact reopens and 19 refusals. GeoParquet
passed 34 imports, 28 exact reopens and 37 refusals. The two ZIPs passed
48- and 50-member inventory checks. The point manifest initially inherited
the default provider version. Binding it to the same CMake version as the
descriptor required one cached plugin rebuild. The shared decoder and
host binaries were reused. Continue with the next demonstrated gap.

[Retained refusal](../build/native-format-closeout/validation-packet/parquet-point-integer-ids/baseline.log)
[Point checks](../build/native-format-closeout/validation-packet/parquet-point-integer-ids/points-validation/validation.json)
[GeoParquet checks](../build/native-format-closeout/validation-packet/parquet-point-integer-ids/geoparquet-validation/validation.json)
[Relocated points](../build/native-format-closeout/validation-packet/parquet-point-integer-ids/relocated.json)
[Point source and binary identity](../build/native-format-closeout/validation-packet/parquet-point-integer-ids/parquet-points-1.1.0/SOURCE.json)
[GeoParquet source and binary identity](../build/native-format-closeout/validation-packet/parquet-point-integer-ids/geoparquet-1.3.1/SOURCE.json)

## Delivered locally - preserve exact integer feature IDs in GeoParquet

GeoParquet 1.3.0 imports signed and unsigned 8-, 16-, 32- and 64-bit IDs
through both WKB and native GeoArrow. The bounded Parquet physical decoder
checks declared width before conversion to decimal text. Reopened projects
preserve the exact IDs, declared integer type and geometry with the source
offline. Null, duplicate and floating-point IDs refuse. The packaged plugin
imports the two retained UInt64 fixtures exactly, through UINT64_MAX.

Thirty-four imports, 28 exact reopens and 37 refusals passed. Fifty package
members passed the inventory and ZIP checks. A typed column callback fixed
the initial compile error. CMake now supplies one descriptor version and
contract. Validation required the retained projected fixture and the empty
image template rather than the previously annotated smoke file. The decoder
and host builds were reused. Continue with the next demonstrated catalogue gap.

[Retained WKB refusal](../build/native-format-closeout/validation-packet/geoparquet-integer-ids/WKB.log)
[Retained GeoArrow refusal](../build/native-format-closeout/validation-packet/geoparquet-integer-ids/geoarrow.log)
[Native checks](../build/native-format-closeout/validation-packet/geoparquet-integer-ids/empty-template-validation/validation.json)
[Relocated imports](../build/native-format-closeout/validation-packet/geoparquet-integer-ids/relocated.json)
[Source and binary identity](../build/native-format-closeout/validation-packet/geoparquet-integer-ids/package-1.3.0/SOURCE.json)

## Delivered locally - list projects with empty saved reader options

The retained catalogue failed on an existing ENVI-backed GeoParquet template
and a new PNM link created with explicit empty options. The CLI now emits the
canonical empty JSON object and preserves the saved binding. The mixed
built-in/PNM project lists with the reader installed and offline. The original
template also lists, and the existing APNG/FITS views retain their exact options.

Fifteen project/GeoJSON and sixteen QMX/frame checks passed. The format CLI
rebuilt once in the existing cache. GeoPandas produced exact UInt64 IDs for
both WKB and native points. Their baseline showed that both decoder routes
refuse integer ID columns. Use these retained files for the next profile.
The producer control needed its existing Shapely path and the refusal command
was corrected to avoid PowerShell's reserved Host variable.

[Retained template refusal](../build/native-format-closeout/validation-packet/project-empty-reader-options/retained-catalog.log)
[Project catalogue checks](../build/native-format-closeout/validation-packet/project-empty-reader-options/pytest.log)
[Reopened template](../build/native-format-closeout/validation-packet/project-empty-reader-options/template-final.json)
[Integer ID WKB baseline](../build/native-format-closeout/validation-packet/geoparquet-integer-ids/WKB.log)
[Integer ID native geometry baseline](../build/native-format-closeout/validation-packet/geoparquet-integer-ids/geoarrow.log)

## Delivered locally - retain regular OME-TIFF timing from plane metadata

The native TIFF reader recovers an interval from a complete, zero-origin
single-file OME Plane DeltaT table with no Pixels TimeIncrement. It checks
unit conversion, agreement across Z and C, strict increase and a one-ULP
rounding allowance on the full time grid. Explicit increments remain
authoritative. Incomplete, offset and irregular tables leave the interval
unspecified. A reopened project reports 0.1 seconds with the exact retained
72-sample raw array and unchanged source bytes.

Twenty-five CLI checks and the native region suite passed. The first build
accepted a repeated subnormal time inside its ULP allowance. The observed
case now has a strict increase check and regression. The final cached build
reused the same targets. Continue with the next demonstrated catalogue gap.

[Retained receipt](../build/native-format-closeout/validation-packet/ome-plane-timing/baseline.json)
[Reopened project](../build/native-format-closeout/validation-packet/ome-plane-timing/project-final.json)
[Repeated subnormal before correction](../build/native-format-closeout/validation-packet/ome-plane-timing/repeated-before.json)
[CLI checks](../build/native-format-closeout/validation-packet/ome-plane-timing/final.log)
[Native suite](../build/native-format-closeout/validation-packet/ome-plane-timing/native-final.log)

## Delivered locally - open VTI grids with shifted extent indices

VTI 1.8.0 opens signed whole and piece indices when the first stored point
is exactly at physical zero. VTK 9.4.2 verified the signed bounds and physical
positions. Both byte orders and all three writer layouts retained exact samples
and spacing. Reversed or excessive extents and a nonzero physical first point
refused. Multipiece assembly and a saved-project crop through the relocated
plugin matched the independent values.

Three hundred existing and changed reader checks passed. Thirty-one source,
notice, runtime and archive members passed the inventory. One cached plugin
build sufficed. The separate relocated project check reused the native host.
Continue with the next demonstrated format or application gap.

[Retained refusal](../build/vti-reader/shifted-extent/baseline.log)
[Reader checks](../build/vti-reader/shifted-extent/validation/validation.json)
[Relocated project](../build/vti-reader/shifted-extent/relocated-project.json)
[Source and binary identity](../build/vti-reader/shifted-extent/package-1.8.0/SOURCE.json)

## Delivered locally - carry saved project Z/time calibration into raw crop receipts

Raw project crop receipts and bundles report the document's physical Z and time
at the selected array indices. Unknown positions are null. The imported
nonmonotonic timestamp and explicit unknown axes were both verified against
the same TIFF pixels. The retained bundle still verifies, and the new bundles
preserve its raw array bytes and acquisition calibration. Offline extraction
passed and a contradictory Z coordinate was rejected before publication.

Sixteen CLI checks and the native region suite passed. The initial fabricated
receipt used an integer that shortened the ZIP member. Keeping the original
floating-point width let the test reach the verifier without a rebuild.
Continue with the next demonstrated format or application gap.

[Retained receipt](../build/native-format-closeout/validation-packet/raw-project-axes/baseline.json)
[Imported physical axes](../build/native-format-closeout/validation-packet/raw-project-axes/axes-0.json)
[Explicit unknown axes](../build/native-format-closeout/validation-packet/raw-project-axes/axes-2.json)
[CLI checks](../build/native-format-closeout/validation-packet/raw-project-axes/final.log)
[Native suite](../build/native-format-closeout/validation-packet/raw-project-axes/native.log)

## Delivered locally - preserve QuPath project-entry Z and time calibration

The retained CLI dropped all five tested project-entry calibrations when
native saved data was absent. The importer now shares the existing native
calibration decoder and preserves an omitted axis from the reader. Tests
verify exact stored bytes for partial, nonmonotonic and unknown positions,
saved-data precedence, and refusal of timestamps outside the source domain.
Projects reopened through the same native CLI. All 320 samples on the checked
raw Z/time plane matched the original TIFF exactly.

Nine focused CLI checks and the native QuPath project suite passed. The first
reopen probe kept a Python SQLite connection live on Windows. Closing that
connection fixed the probe without rebuilding. Continue with the next
demonstrated format or application gap.

[Retained CLI comparison](../build/native-format-closeout/validation-packet/qupath-builder-axes/baseline.log)
[Focused CLI checks](../build/native-format-closeout/validation-packet/qupath-builder-axes/final.log)
[Native suite](../build/native-format-closeout/validation-packet/qupath-builder-axes/native.log)
[Raw sample comparison](../build/native-format-closeout/validation-packet/qupath-builder-axes/raw.json)

## Delivered locally - open NIfTI files containing standard comment extensions

NIfTI 1.5.0 accepts bounded code-6 ASCII comments in NIfTI-1 and NIfTI-2
single files and pairs, with either byte order and the existing gzip routes.
The retained reader refused the NiBabel producer volume at its extended voxel
offset. The new reader preserves the samples, calibration and display and
refuses malformed records and unsupported codes. Saved NIfTI-1 single-file
and NIfTI-2 pair projects exported exact Z1 voxels against NiBabel.

The relocated package passed 46 positive and 84 negative fixture profiles
and 33 native validation commands. Twenty-five source, runtime and notice
members passed the staging inventory. The first test run exposed stale
hardcoded fixture counts. Updating them to cover all new cases required only
rebuilding the ABI test. Continue with the next demonstrated catalogue gap.

[Retained refusal](../build/nifti-reader/comment-extensions/baseline.log)
[Relocated validation](../build/nifti-reader/comment-extensions/relocated-validation/validation.json)
[Project exports](../build/nifti-reader/comment-extensions/projects.json)
[Source and binary identity](../build/nifti-reader/comment-extensions/package-1.5.0/SOURCE.json)

## Delivered locally - honor ENVI raster ignore values in the native plugin

ENVI 1.2.0 reads the scalar data ignore value and sends validity through the
existing reader ABI. The Spectral Python big-endian BIL fixture confirms that
only requested bands govern raw validity, invalid tuples are zero-filled and
valid values stay exact. The first-band preview is transparent at the sentinel.
Float32, clipped and malformed-value cases also pass. The project crop matches
the direct export byte for byte, and a 1.1.0 link still replays with its pinned
reader and requires explicit review before adopting 1.2.0.

Fifty-one checks passed from the relocated Windows package. Twenty source,
notice and package members passed the existing staging inventory. One cached
plugin build sufficed and the host and SDK probe were reused. Continue with the
next demonstrated format or application gap.

[Retained refusal](../build/envi-reader/ignore-value-baseline/refusal.log)
[Relocated validation](../build/envi-reader/ignore-value-baseline/relocated-validation/validation.json)
[Old-project version check](../build/envi-reader/ignore-value-baseline/retained-1.2-refusal.log)
[Source and binary identity](../build/envi-reader/package-1.2.0/SOURCE.json)

## Delivered locally - identify rendered project crops by their exact saved view

`--project-region ... --receipt` reports the published PAM digest and size,
project identity and revision, image, crop, observed reader source and saved
TIFF or OME-Zarr view. TIFF receipt and quiet export produced identical bytes.
An occupied target and a stale verified source refused without a success
receipt. OME-Zarr and APNG receipts matched the retained PAM bytes, and the
display view or plugin binding matched the saved project catalogue.

Sixteen CLI checks and the native region-export suite passed. The first build
used the wrong test target name after successfully linking the region CLI.
The follow-up built only the correct test target in the same cache.
Continue with the next demonstrated format or application gap.

[Retained CLI refusal](../build/native-format-closeout/validation-packet/project-display-receipt/baseline.log)
[TIFF CLI checks](../build/native-format-closeout/validation-packet/project-display-receipt/pytest.log)
[Native suite](../build/native-format-closeout/validation-packet/project-display-receipt/native.log)
[OME-Zarr receipt](../build/native-format-closeout/validation-packet/project-display-receipt/zarr.json)
[APNG receipt](../build/native-format-closeout/validation-packet/project-display-receipt/apng.json)

## Delivered locally - identify saved optional-reader views without loading pixels

The retained CLI listed same-size APNG frames 0 and 1 identically.
`project-images` now reports the normalized options, provider version and
exact binding from the native saved project link. Both frames and the selected
FITS HDU report the same saved facts with or without their manifests. The
APNG frame bindings differ. Each saved APNG view exported display and raw
bytes identical to direct reads of that frame. A missing provider refused
publication and the original projects and source stayed byte-exact.

Sixteen QMX/ROI and project-catalogue checks and sixteen TIFF CLI checks passed.
The cached format CLI rebuilt once and the region comparisons reused the
existing executable. No failed build or validation required a retry.
Continue with the next demonstrated format or application gap.

[Retained catalogue](../build/native-format-closeout/validation-packet/project-plugin-view/baseline.jsonl)
[QMX/ROI and catalogue checks](../build/native-format-closeout/validation-packet/project-plugin-view/matrix/validation.json)
[TIFF CLI checks](../build/native-format-closeout/validation-packet/project-plugin-view/tiff.log)
[APNG region comparisons](../build/native-format-closeout/validation-packet/project-plugin-view/regions.json)

## Delivered locally - save and inspect display channels and contrast from the CLI

`project-set-view` saves RGB channel mapping and contrast bounds for TIFF and
OME-Zarr through the native view setter. `project-images` reports saved bounds
even when the source is offline. TIFF verifies byte-exact rendering against
independently scaled samples, unchanged raw export and rejection of malformed
views without publication. OME-Zarr verifies the window on the saved plane,
byte-exact raw export for both channels and all 43 unchanged store members.

Thirty focused checks passed. Two provider cases initially skipped and passed
once their existing manifests were supplied. Only the cached format CLI was
rebuilt. Next, compare a named remaining reader profile or application workflow
against its existing native route before selecting another repair.

[Retained command refusal](../build/native-format-closeout/validation-packet/project-display-view/baseline.log)
[CLI checks](../build/native-format-closeout/validation-packet/project-display-view/pytest-final.log)
[Provider checks](../build/native-format-closeout/validation-packet/project-display-view/pytest-providers.log)
[OME-Zarr comparison](../build/native-format-closeout/validation-packet/project-display-view/zarr.json)

## Delivered locally - render a saved project region from the native CLI

`--project-region` renders the saved reader view as RGBA8 PAM. The TIFF
fixture verified swapped red/green channels, disabled blue, transparent
padding and exact samples on the saved Z/T. Occupied destinations, the
project WAL and stale source verification refused without publication.
OME-Zarr output matched independently encoded RGBA bytes from all saved
display channels, and an output inside its store refused.

Sixteen focused checks and the native region suite passed. The initial
OME-Zarr comparator requested two channels for a three-channel view. The
final comparison used the saved channel map. The region CLI and native
test reused the cache. Continue with the next demonstrated workflow gap.

[Retained command refusal](../build/native-format-closeout/validation-packet/project-display-region/baseline.log)
[TIFF checks](../build/native-format-closeout/validation-packet/project-display-region/pytest.log)
[OME-Zarr comparison](../build/native-format-closeout/validation-packet/project-display-region/zarr.json)
[Native suite](../build/native-format-closeout/validation-packet/project-display-region/native.log)

## Delivered locally - save a quantitative image view from the native CLI

`project-set-view` saves Z, time or both for TIFF and OME-Zarr through the
native view setter. TIFF exports read the exact chosen plane after reopen,
retained the other axis on a partial edit and preserved annotation bytes.
Invalid views and protected destinations refused. OME-Zarr default view
export matched the explicit-plane array byte for byte and preserved all 43
store members.

Twenty-seven focused cases passed. The initial run skipped two provider
checks until their manifests were supplied. Both passed in the follow-up.
Five OME-Zarr commands passed. Only the cached format CLI needed rebuilding.
Continue with the next demonstrated workflow gap.

[Retained command refusal](../build/native-format-closeout/validation-packet/project-raw-view/baseline.log)
[TIFF and project checks](../build/native-format-closeout/validation-packet/project-raw-view/pytest.log)
[OME-Zarr checks](../build/native-format-closeout/validation-packet/project-raw-view/zarr-workflow.json)

## Delivered locally - protect verified project inputs during RGB prediction export

The retained CLI exported replacement pixels and wrote a QPS stream at a
missing project WAL path. The RGB exporter now checks the verified image and
protects the project, SQLite sidecars, model and linked inputs before starting
the worker. It refused the missing WAL and other linked paths and the stale
slide. The adopted slide exported and verified 3136 valid pixels with the
changed class counts. Earlier output and the input project stayed exact.

Forty-three focused CLI checks passed. Four checks that require a source-matched
SDK and CLI were outside this mixed-build run. The batch CLI reused the native
cache and needed no retry. Continue with the next demonstrated workflow gap.

[Retained exports](../build/native-format-closeout/validation-packet/rgb-stream-verified-source/baseline.log)
[CLI checks](../build/native-format-closeout/validation-packet/rgb-stream-verified-source/pytest.log)

## Delivered locally - enforce saved image verification in native raw classifier workflows

The retained CLI trained on replacement TIFF samples. Project-backed training,
prediction, contour publication and the combined native workflow now check
the saved verification before starting. Thirteen focused checks passed.
Stale inputs refused without artifacts. After adoption, training kept 12
rows and the prediction verified its exact digest.

Four additional commands verified the complete combined workflow and exact
prediction and contour replay. The first manual contour replay exceeded the
default admission budget. The final invocation set explicit file and total
work limits. The cached build relinked the batch CLI once. Continue with the
next demonstrated workflow gap.

[Retained training](../build/native-format-closeout/validation-packet/raw-classifier-verified-source/baseline.log)
[CLI regression](../build/native-format-closeout/validation-packet/raw-classifier-verified-source/pytest.log)
[Combined workflow and replay](../build/native-format-closeout/validation-packet/raw-classifier-verified-source/final/workflow.json)

## Delivered locally - honor saved image verification when keeping derived recipes

The retained CLI saved dark/flat and unmixing recipes from replacement TIFF
samples. Both commands now check the verified project signal before creating
a recipe. Ten CLI checks confirmed refusal without output before adoption,
54 finite corrected samples after adoption, exact original and corrected
unmixing replay, and unchanged input bytes.

Both native recipe suites passed with their independent numeric checks.
The cached batch build and validation needed no retries. Continue with the
next demonstrated workflow gap.

[Retained recipes](../build/native-format-closeout/validation-packet/derived-verified-source/baseline.log)
[CLI checks](../build/native-format-closeout/validation-packet/derived-verified-source/workflow.json)
[Native suites](../build/native-format-closeout/validation-packet/derived-verified-source/native.log)

## Delivered locally - honor saved source verification in headless stain estimation

The retained CLI published a model from replacement slide pixels. The batch
CLI now verifies the selected image before fitting and checks the observed
source and linked inputs through model publication. A shifted replacement
refused without writing a model. After reviewed adoption, the estimate matched
the native SDK's exact input digest, model digest and stain vectors.

All 32 focused checks and both native stain suites passed. One opt-in desktop
check skipped. The cached native build and validation needed no retries.
Continue with the next demonstrated workflow gap.

[Retained model](../build/native-format-closeout/validation-packet/stain-verified-source/baseline.log)
[CLI and SDK regression](../build/native-format-closeout/validation-packet/stain-verified-source/pytest.log)
[Native suites](../build/native-format-closeout/validation-packet/stain-verified-source/native.log)

## Delivered locally - honor saved source verification for native TMA proposals

The retained CLI published a grid from a shifted replacement slide. Native
detection now checks saved source verification before reading the slide and
source currentness before publishing. A same-size replacement refused until
reviewed adoption. The adopted slide produced 11 candidates, all 12 grid
positions within the fixture tolerance, and the expected missing core.

The native TMA suite passed. All 12 CLI checks passed after switching the
QPTMA round-trip verifier to reopen the new project through the same CLI.
The retained Python SDK could read the original fixture but refused the
current schema. Only cached native targets were rebuilt. Continue with
the next demonstrated workflow gap.

[Retained proposal](../build/native-format-closeout/validation-packet/tma-verified-source/baseline.log)
[CLI checks](../build/native-format-closeout/validation-packet/tma-verified-source/pytest-final.log)
[Native suite](../build/native-format-closeout/validation-packet/tma-verified-source/native.log)

## Delivered locally - honor saved source verification in native fluorescence detection

The retained fluorescence CLI published a cell from replacement TIFF
samples with an older saved verification. New CLI detection shares the
source preflight with label analysis. Six CLI commands verified refusal
before adoption, exact replay after adoption, and 54 nucleus samples with
sum 5346 and mean 99. The input project and TIFF stayed byte-exact.

Both native CLI suites passed on the first validation. One cached build
rebuilt the batch and label frontends and shared source consumers. Project
density was inspected and uses saved geometry without reading pixels.
Continue with the next demonstrated workflow gap.

[Retained publication](../build/native-format-closeout/validation-packet/fluorescence-verified-source/baseline.log)
[CLI and exact measurements](../build/native-format-closeout/validation-packet/fluorescence-verified-source/workflow.json)
[Native checks](../build/native-format-closeout/validation-packet/fluorescence-verified-source/native.log)

## Delivered locally - enforce saved image verification in native project batches

The retained batch CLI published replacement TIFF measurements under an
older verification. The batch kernel now checks the saved observation
before checkpoint lookup or computation. Fresh and resumed runs refused
the changed image, while the second image saved or reused its checkpoint.
The historical checkpoint and input project stayed exact. Reviewed adoption
published 54 valid pixels with mean 99, up from 23 in the original checkpoint.

Five focused CLI checks passed. Native test assertions reached cleanup
under CTest, where Windows refused paths up to 283 characters. The same
native test binary passed from a shorter workspace path. The batch build
reused the native cache. Continue with the next demonstrated workflow gap.

[Baseline publication](../build/native-format-closeout/validation-packet/batch-verified-source/baseline.log)
[CLI checks](../build/native-format-closeout/validation-packet/batch-verified-source/pytest.log)
[CTest cleanup failure](../build/native-format-closeout/validation-packet/batch-verified-source/native.log)
[Native suite](../build/native-format-closeout/validation-packet/batch-verified-source/native-short-root.log)

## Delivered locally - honor saved source verification for new native label work

The retained CLI generated a crop digest and imported a mask against
replacement pixels. Both native frontends now check saved source verification
before new label work or input reporting. Ten CLI commands verified refusal
before adoption, exact saved raster replay after adoption and unchanged
input bytes. The CSV retained source label 7, 54 pixels, sum 5346 and mean
99.

Both native label suites passed. The first cached build completed its CLI
test target before the added regression was saved. Rebuilding that test
target verified the new case. Continue with the next demonstrated workflow
gap.

[Baseline publication](../build/native-format-closeout/validation-packet/label-verified-source/baseline.log)
[Standalone and batch checks](../build/native-format-closeout/validation-packet/label-verified-source/workflow.json)
[Exact CSV](../build/native-format-closeout/validation-packet/label-verified-source/labels.csv)
[Native suites](../build/native-format-closeout/validation-packet/label-verified-source/native.log)

## Delivered locally - honor saved source verification during native annotation plugin import

The retained CLI saved a plugin point against a same-size replacement TIFF.
Point and geometry imports now use the shared verified-source guard before
preview and the native project input guard at publication. The rebuilt CLI
refused the original replacement fixture. Both providers refused changed
sources without saving and kept exact coordinates and the literal source ID
`0001` after reviewed adoption.

All 14 focused checks and three native importer modes passed. The first
baseline fixture command had a PowerShell syntax error. A direct UTF-8 fixture
write reproduced the publication. Only the cached CLI and provenance object
needed rebuilding. Continue with the next demonstrated workflow gap.

[Retained publication](../build/native-format-closeout/validation-packet/annotation-verified-source/baseline.log)
[Rebuilt TIFF refusal](../build/native-format-closeout/validation-packet/annotation-verified-source/changed-tiff.log)
[CLI checks](../build/native-format-closeout/validation-packet/annotation-verified-source/pytest.log)
[Native annotation check](../build/native-format-closeout/validation-packet/annotation-verified-source/native-annotation.log)

## Delivered locally - preserve saved source verification during matrix indexing and attachment

The retained CLI created a QMX against a same-size replacement TIFF. File and
store project indexing and matrix attachment now share the native crop source
verifier. A QMX indexed directly against the replacement could not be
attached to the original verified project. After reviewed adoption, TSV and
AnnData Zarr indexing matched standalone QMX bytes and kept the old project
exact.

Sixteen focused checks, five AnnData CLI commands, three native importer
modes and the native region suite passed on the first validation. Both CLIs
and affected tests reused a single cached native build. Continue with the
next demonstrated workflow gap.

[Reproduced publication](../build/native-format-closeout/validation-packet/matrix-verified-source/baseline.log)
[CLI and crop regression](../build/native-format-closeout/validation-packet/matrix-verified-source/pytest.log)
[AnnData commands](../build/native-format-closeout/validation-packet/matrix-verified-source/store.json)
[Native matrix check](../build/native-format-closeout/validation-packet/matrix-verified-source/native-matrix.log)

## Delivered locally - enforce saved source verification for project crops

The retained CLI exported same-size replacement TIFF pixels with the old
verified project's hash. Project crops now check the saved observation
before reading pixels. Single-file replacements can match by exact SHA-256,
while compound and directory sources retain their full observation checks.

Fifteen focused checks and the rebuilt native export suite passed. A
byte-identical TIFF replacement passed, changed pixels refused until
reviewed adoption, and a byte-identical companion replacement refused.
The previously adopted native OME-Zarr project still exported its complete
168,960-pixel plane. One cached native build and one validation run.
Continue with the next demonstrated workflow gap.

[Reproduced replacement export](../build/native-format-closeout/validation-packet/raw-project-verified-source/baseline.log)
[TIFF and companion checks](../build/native-format-closeout/validation-packet/raw-project-verified-source/pytest.log)
[Native suite](../build/native-format-closeout/validation-packet/raw-project-verified-source/native.log)
[Native OME-Zarr check](../build/native-format-closeout/validation-packet/raw-project-verified-source/native-zarr.log)

## Delivered locally - bind raw crop bundles to the saved project image

Project crop reports and v3 ZIP receipts now include the exact project file
SHA-256, saved identity and generation, and image ID. The receipt digest
covers those fields. The CLI matched them against SQLite and an independent
hash. Moving the project and TIFF left offline extraction byte-exact. Invalid
fields and a changed image ID with a stale digest refused without output.

All 13 focused checks and the native export suite passed. Three retained
v1/v2 bundles extracted byte for byte. The first fixture retained the TIFF
handle, and ordinary ZIP reserialization used headers outside the native
profile. The final test releases the handle and mutates the native envelope
with corrected CRCs. The final cached build refreshed an explicit standard
header include. Continue with the next demonstrated workflow gap.

[Previous receipt](../build/native-format-closeout/validation-packet/project-raw-receipt/baseline.log)
[CLI checks](../build/native-format-closeout/validation-packet/project-raw-receipt/pytest-final.log)
[Native suite](../build/native-format-closeout/validation-packet/project-raw-receipt/native-final.log)
[Legacy archives](../build/native-format-closeout/validation-packet/project-raw-receipt/legacy.json)

## Delivered locally - export raw regions using a saved project image

`--raw-project` resolves the saved reader, TIFF series, and options and follows
saved Z/T by default. The shared native exporter writes atomic NPY or ZIP and
guards the project and linked inputs. All 13 TIFF/CLI checks and the rebuilt
native export suite passed. The CLI verified exact saved and explicit planes,
structured validity extraction, and protection of the project, a linked
hardlink and another image's missing original path.

Nine additional commands verified distinct same-size TIFF series, translated
OME-Zarr level-array export with exact calibration, and byte-exact UInt64
plugin output with the saved manifest. The retained CLI lacked the command.
The first grid verifier looked for the dataset at the report root. The final
comparison checks the full source grid and its nested dataset. The build
reused the native cache. Continue with the next demonstrated workflow gap.

[Retained refusal](../build/native-format-closeout/validation-packet/project-raw-region/baseline.log)
[TIFF and CLI tests](../build/native-format-closeout/validation-packet/project-raw-region/pytest.log)
[Native export suite](../build/native-format-closeout/validation-packet/project-raw-region/native.log)
[Reader and series checks](../build/native-format-closeout/validation-packet/project-raw-region/final/workflow.json)

## Delivered locally - discover saved project image IDs from the native CLI

`project-images` lists saved IDs, names, reader views and object counts for
export, relinking and matrix indexing. Sources can be offline. The native
project catalogue now checks directory-backed sources as directories.

All 13 focused checks and both rebuilt native project suites passed. Two
same-size TIFF series retain separate IDs when the TIFF moves. An installed
NGFF reader reported its directory available only with the admitted manifest
and present source. Listing preserved the project and all 23 source files.
The first plugin probe used a separate OpenSlide DLL path and reported the
loader conflict. The final probe colocated the test executable with the
admitted dependencies. Automatic tool policy rejected deletion of that
temporary runtime copy. One cached native build. Continue with the next
demonstrated workflow gap.

[Retained command refusal](../build/native-format-closeout/validation-packet/project-images-cli/baseline.log)
[CLI checks](../build/native-format-closeout/validation-packet/project-images-cli/pytest.log)
[Native project suites](../build/native-format-closeout/validation-packet/project-images-cli/native.log)
[Optional reader checks](../build/native-format-closeout/validation-packet/project-images-cli/reader/validation.json)

## Delivered locally - protect original links during native CLI source adoption

The retained CLI published a project at a moved image's missing original path.
Adoption now applies the shared source guard to the original reader, store and
recorded companions before replacing the link and again before publication.
It also protects the input project's SQLite sidecar paths.

All 12 hierarchy/CLI tests and both rebuilt native project/interchange suites
passed. CSV and nested GeoJSON stayed byte-exact after moving the image, and
adoption at a fresh path preserved the CSV. Five OME-Zarr commands verified
refusal at and inside the old root and successful adoption elsewhere with all
four store members unchanged. The cache rebuilt the affected native code once.
Continue with the next demonstrated workflow gap.

[Baseline publication](../build/native-format-closeout/validation-packet/offline-object-exchange/adopt-old-path-baseline.log)
[CLI checks](../build/native-format-closeout/validation-packet/offline-object-exchange/pytest.log)
[Directory checks](../build/native-format-closeout/validation-packet/offline-object-exchange/directory/validation.json)
[Native suites](../build/native-format-closeout/validation-packet/offline-object-exchange/native.log)

## Delivered locally - export saved project object measurements from the native CLI

`measurements-export` uses the shared native CSV writer and protected new-only
project export path. An independent CSV reader verified exact C1/Z1/T1, 400 px²,
50 µm², imported 12.5, lock/color and quoted multiline metadata. Selection,
duplicate IDs, a header-only image, an unknown ID, an existing CSV and aliases
of the input project and another linked image were checked. Protected bytes
stayed exact.

The retained CLI refused the command. All 11 hierarchy/CLI tests and the native
interchange suite passed on the first validation run. The cached build relinked
the format CLI and refreshed its provenance object. Continue with the next
demonstrated workflow gap.

[Retained refusal](../build/native-format-closeout/validation-packet/object-measurements-cli/baseline.log)
[CLI regression](../build/native-format-closeout/validation-packet/object-measurements-cli/pytest.log)
[Native interchange](../build/native-format-closeout/validation-packet/object-measurements-cli/native.log)

## Delivered locally - index spatial matrices on an explicit image plane

Standalone and project-based file and directory indexing accept an explicit
channel, Z and time through the native frame builder. The retained CLI rejected
the option. At C1/Z1/T1 the standalone and project QMX outputs match byte for
byte for both counts TSV and AnnData Zarr. The original default QMX also matches
the retained executable exactly. Attachment and ROI replay exported the exact
count 9007199254740993. An ROI on the wrong Z plane, an absent plane and duplicate
plane flags refused without publishing results.

All 15 end-to-end commands, three native importer checks and 26 object-plane
tests passed. Two opt-in tests skipped. The first legacy comparison passed an
explicit reader flag newer than the retained CLI. The final run uses that
binary's OME-TIFF auto selection. The native build reused the existing cache.
Continue with the next demonstrated gap.

[Retained option refusal](../build/native-format-closeout/validation-packet/matrix-nonzero-plane/baseline.log)
[Matrix and ROI replay receipt](../build/native-format-closeout/validation-packet/matrix-nonzero-plane/workflow/validation.json)
[Object-plane tests](../build/native-format-closeout/validation-packet/matrix-nonzero-plane/pytest.log)
[Native importer](../build/native-format-closeout/validation-packet/matrix-nonzero-plane/matrix.log)

## Delivered locally - select and persist a native TIFF series from the format CLI

`--series` now reaches the shared TIFF reader when creating or appending a
project image and indexing standalone file or directory matrices. Project-based
indexing uses the saved series. The retained CLI rejected the option. The new
CLI saves two same-size series from one TIFF as separate image views. Counts
TSV and AnnData Zarr each produced byte-identical standalone and project QMX
files for the selected series, and the other series refused attachment.

The reopened images produced exact UInt16 ROI means of 124.5 and 24.5 in one
raw batch. All 14 workflow checks, 12 TIFF CLI cases and five project CLI
regressions passed. The first plain `.tif` test let the existing automatic
dispatch choose OpenSlide. The final test and usage select `--reader tiff`
explicitly. The native cache relinked only the changed format CLI. Continue
with the next demonstrated gap.

[Retained option refusal](../build/native-format-closeout/validation-packet/tiff-series-format-cli/baseline.log)
[Matrix and batch receipt](../build/native-format-closeout/validation-packet/tiff-series-format-cli/end-to-end/validation.json)
[TIFF CLI checks](../build/native-format-closeout/validation-packet/tiff-series-format-cli/cli-final.log)
[Project CLI regression](../build/native-format-closeout/validation-packet/tiff-series-format-cli/project-regression.log)

## Delivered locally - review and import a QuPath project from the native CLI

`qupath-project-preview` and `qupath-project-import` reuse the native `.qpproj`
authority, source overrides, both saved-data routes and explicit loss acceptance.
An optional review token binds the compiled source, reported interpretation and
file observations across processes. The retained CLI lacked this route. The
retained SDK also allowed publishing a native project at the path of an expected
but missing exchange index. Shared native publication now protects that path.

The pinned two-image migration reopened with two objects and a parent link per
image. A byte-identical TIFF replacement and the return of a previously missing
image each invalidated the old token. Accepting the omission imported the other
image. Direct saved-data import and the missing-index refusal passed as well.
The focused regression group passed 69 checks with four skips, followed by
three targeted checks including the reappearing source. The native migration
suite passed. The build reused the native cache and rebuilt the CLI and affected
core inputs without rebuilding Python. Continue with the next demonstrated gap.

[Retained command refusal](../build/native-format-closeout/validation-packet/qupath-project-cli/baseline.log)
[Project and interchange regression](../build/native-format-closeout/validation-packet/qupath-project-cli/pytest.log)
[Final CLI checks](../build/native-format-closeout/validation-packet/qupath-project-cli/cli-final.log)
[Native migration suite](../build/native-format-closeout/validation-packet/qupath-project-cli/native.log)

## Verified locally - object-plane persistence across project schemas

The shared fixture helper now removes all post-26 tables during a synthetic
downgrade. The retained schema 29 SDK reaches the injected schema 17 save
fault, preserves the original project bytes, then migrates and retains the
exact plane after the fault is removed. The derived QuPath fixture imports
without losses and leaves the UNSET analysis default absent. The focused SDK
group passed 61 tests with four opt-in skips.

The native project suite passed. The current CLI wrote schema 30 and a second
process exported the exact stored C0/Z1/T1 plane. The first independent SQL
inspection retained its connection across the export. The final probe closes
its immutable read before reopening the project. The test contract defaults
to schema 30 and can explicitly pin a retained SDK version. No binding or
product rebuild was needed. Continue with the next demonstrated gap.

[Retained SDK checks](../build/native-format-closeout/validation-packet/project-schema-regression/sdk29.log)
[Active native project suite](../build/native-format-closeout/validation-packet/project-schema-regression/native.log)
[Active schema and plane receipt](../build/native-format-closeout/validation-packet/project-schema-regression/native-schema30.json)

## Verified locally - reconcile GeoJSON v1 and v2 interchange checks

The hierarchy checks now compare the current v2 class envelope and the exact
archived feature bytes. Both frozen v1 files still import and re-export byte
for byte. The clipboard conflict check follows the current class-definition
diagnostic and still verifies that the failed paste leaves the target intact.
The hierarchy, interchange, class, clipboard and CLI group passed 66 cases
with two opt-in skips. The first expected-wire edit reordered the outer keys;
the final verifier replaces only the versioned metadata. No build was needed.

The adjacent object-plane suite exposed stale expectations for the project
schema, historical fixture downgrades and a delivered image-setting migration.
The project-schema check above records their resolution.

[Interchange regression](../build/native-format-closeout/validation-packet/geojson-cli/schema-regression-final.log)
[Adjacent object-plane failures](../build/native-format-closeout/validation-packet/geojson-cli/schema-regression.log)

## Delivered locally - round-trip project annotations as GeoJSON from the native CLI

`geojson-import` and `geojson-export` expose the same native interchange
authority as desktop and SDK. Fresh processes imported six objects with four
parent links and retained the cell nucleus, classes, names and geometry.
Nested export → import → export repeated byte for byte. Untagged input and
losses required explicit flags. Publication refused existing outputs, hardlink
aliases and paths in a linked OME-Zarr store. Its 43 source files stayed exact.

The first verifier expected a historical v1 envelope and used a property
fixture whose fields are now supported. The final checks use the current v2
class envelope, exact archived feature bodies and one explicitly unknown
collection field. Both focused CLI tests and the rebuilt native interchange
suite passed. The regression follow-up above verifies the archived v1 files.

[Retained command refusal](../build/native-format-closeout/validation-packet/geojson-cli/baseline.log)
[CLI checks](../build/native-format-closeout/validation-packet/geojson-cli/cli-final.log)
[Native suite](../build/native-format-closeout/validation-packet/geojson-cli/native.log)
[Directory guard](../build/native-format-closeout/validation-packet/geojson-cli/directory-guard/receipt.json)

## Delivered locally - read default OME-TIFF plane mappings with extra TIFF pages

The retained reader refused an OME-TIFF with eight declared planes and an
unmapped ninth thumbnail. The native TIFF reader now bounds the inferred
mapping to the declared lattice. Explicit count overflow and incomplete
mapping still refuse. The CLI verified all 120 UInt16 samples across Z/C/T,
the three calibration axes and unmodified source bytes. The native raw TIFF
suite and all 11 CLI checks passed.

The build reused the Python-disabled cache and relinked the region executable
and native raw test. The native test ran from the shorter system temp path
to accommodate its checkpoint filenames. Continue with the next demonstrated
format or application gap.

[Retained refusal](../build/native-format-closeout/validation-packet/ome-tiff-extra-pages/baseline.log)
[Updated raw export](../build/native-format-closeout/validation-packet/ome-tiff-extra-pages/fixed.json)
[CLI tests](../build/native-format-closeout/validation-packet/ome-tiff-extra-pages/pytest.log)
[Native raw tests](../build/native-format-closeout/validation-packet/ome-tiff-extra-pages/native.log)

## Delivered locally - open detached NRRD rasters with spaced payload names

NRRD 1.14.0 accepts interior ASCII spaces and one `./` prefix in same-directory
single and LIST payload declarations. A producer-written gzip pair previously
refused. The new package exports its exact UInt16 plane and both planes of a
spaced LIST in header order. Fresh-process source review observes both payloads
and detects byte-identical replacement of one. The direct ABI and host suite
passed 168 fixtures, 179 refusals and 64 validation commands.

The first independent project probe assumed source receipts list companions
in raster order. The receipt actually sorts file paths. The corrected probe
checks membership separately from the independently verified plane order.
The build reused both locked codecs and the installed host. The stage and ZIP
each passed admission for 24 members, and the packaged sources configured
offline. Continue with the next demonstrated gap.

[Retained refusal](../build/nrrd-reader/spaced-companion/baseline.log)
[ABI and host validation](../build/nrrd-reader/spaced-companion/validation/validation.json)
[Producer and project checks](../build/nrrd-reader/spaced-companion/project-check-02/report.json)
[Package provenance](../build/nrrd-reader/spaced-companion/package-1.14.0/SOURCE.json)

## Delivered locally - pool TMA measurements with LF and CRLF headers

The retained batch CLI refused identical headers with different line endings.
The shared case/cohort parser now accepts LF and CRLF together. It preserves
every supplied row terminator and uses the first available header terminator
only when joining a complete EOF record to more rows. The retained fixture
now pools with exact input SHA-256 and weighted mean 3.5. All 21 focused CLI
checks passed, including reordered columns, embedded quoted newlines, exact
digests in both terminator orders and existing selector/revision refusals.

The native suite passed via CTest after moving its test workspace to the
canonical system temp path. The first run hit a 267-character checkpoint
path during cleanup. The follow-up resolved a trailing-separator mismatch
in the cleanup guard. The native cache rebuilt the affected units and test
without rebuilding the bindings or changing any release artifact. Continue
with the next demonstrated format or application gap.

[Retained refusal](../build/native-format-closeout/validation-packet/tma-mixed-endings/baseline.log)
[Exact pooled digest](../build/native-format-closeout/validation-packet/tma-mixed-endings/retained-fixture.json)
[CLI regression](../build/native-format-closeout/validation-packet/tma-mixed-endings/pytest.log)
[Native suite](../build/native-format-closeout/validation-packet/tma-mixed-endings/native-canonical.log)

## Delivered locally - import indexed PNG labels without converting palette colors

The retained CLI refused an indexed mask. The shared native label importer
now accepts `--mask-format png-indexed` and pins that profile and the existing
PNG decoder in the saved recipe. Authored 1/2/4/8-bit checks preserve exact
indices despite identical palette colors and a fully transparent positive ID.
Native publication, undo, original-image measurements, save and replay pass.
A separate-process `qupathxx_labels` run kept IDs 7 and 255, and `qupathxx_batch`
replayed its project and produced the same CSV bytes. Retained TIFF and
grayscale PNG result IDs, CSVs and PNG raster bytes still match exactly.

The focused build reused the Python-disabled integration cache. Batch needed
one extra relink to verify its actual executable, and the admitted PNG module
was reused directly. No source or artifact transfer was needed. Continue with
the next demonstrated format or application gap.

[Retained refusal](../build/native-format-closeout/validation-packet/indexed-png-labels/baseline.log)
[Native run](../build/native-format-closeout/validation-packet/indexed-png-labels/native.log)
[Standalone workflow](../build/native-format-closeout/validation-packet/indexed-png-labels/standalone.json)
[Batch replay](../build/native-format-closeout/validation-packet/indexed-png-labels/batch-replay.json)
[Retained recipe compatibility](../build/native-format-closeout/validation-packet/indexed-png-labels/legacy-replay.log)

## Delivered locally - open ENVI rasters with an explicit companion layout

ENVI 1.1.0 adds `bin`, `dat` and `appended` layouts alongside the default
`.img` layout. Source enumeration and decoding use one same-directory
resolver. The retained module refused the `.dat` copy of a writer fixture.
The relocated Windows package passed 18 raw checks, three previews, 15
refusals and five project checks. The explicit selection read exact band
values even with a different same-stem `.img` beside it. A project saved with
the appended layout detected a byte-identical replacement of its selected
companion on fresh-process review.

The verifier moved the option before `--raw`, reused the fixture's pinned
region host and kept project review's primary-only coverage distinct from
a full content match. Only the ENVI module rebuilt. The stage and ZIP each
passed admission for 20 exact members, and the staged sources configure
offline. Continue with the next demonstrated open-format or application gap.

[Retained refusal](../build/envi-reader/companion-baseline/refusal.log)
[Relocated validation](../build/envi-reader/companion-layout-packaged/validation.json)
[Source and binary identity](../build/envi-reader/package-1.1.0/SOURCE.json)

## Verified locally - replay the frozen Windows reader handoff from another directory

The portable runner takes the pinned runtime ZIP and its admission inventories,
the retained MetaImage project probe and producer slices, and the VTI producer
fixture. It runs the existing native checks from a new install directory and
checks manifest-owned removal. The validation host needs Python 3.12 with NumPy
and PowerShell 7. The runner uses the ZIP's admitted DLLs for the setup probe.

The relocated run rejected an incorrect ZIP hash before extraction, passed all
15 workflow checks, verified 286 archive members and preserved 36 user and
validation files. Admission tests passed 18 checks with one skip. The first run
reached removal with Windows PowerShell, which could not resolve `Get-FileHash`.
The runner now uses PowerShell 7 and the full replay passes. It records the host,
archive, compiled sources, fixture and probe identities. No product rebuild was
needed.

The existing hosted workflow targets an older candidate, and local Hyper-V
guest enumeration is denied. Run this gate on a fresh Windows host when one
can receive the exact validation inputs. A clean-machine source rebuild and
release remain separate checks.

[Portable runner](../tools/ci/check_windows_reader_handoff.py)
[Relocated replay receipt](../build/native-format-closeout/validation-packet/reader-workflow-handoff/relocated-runner.json)

## Delivered locally - review a moved directory image from the CLI

Both source review commands accept `--directory ROOT` and use the existing
explicit-root C++ review. The review digest binds the destination root and
directory observation. The retained CLI refused the moved NGFF 0.5 store
because it compared the candidate fingerprint against the saved root.

Ten relocated plugin checks passed. The CLI refused wrong metadata, an
incorrect root, missing change acceptance and a review made stale by a
byte-identical shard replacement. The adopted project reopened without the
original store, and the relocated raw read matched all 96 producer UInt64
samples. Built-in OME-Zarr preview, adoption and reopen also passed, along
with ten file-source review regressions. The first baseline run loaded
OpenSlide from a different directory. Co-locating the test host with the
admitted runtime resolved it. Only the cached format CLI needed relinking.

[Retained mismatch](../build/native-format-closeout/validation-packet/directory-cli-review/baseline.log)
[Directory plugin checks](../build/native-format-closeout/validation-packet/directory-cli-review/validation.json)
[Built-in OME-Zarr adoption](../build/native-format-closeout/validation-packet/directory-cli-review/native-ome-zarr.json)
[File review regression](../build/native-format-closeout/validation-packet/review-file-regression/validation.json)

## Delivered locally - review and adopt a changed source from the native CLI

`project-review-source` reports comparison, coverage, verified primary
content and the observed candidate file versions. `project-adopt-source`
recomputes the review digest from the current project bytes and source.
The existing native review policy still requires explicit acceptance of
changed observations. A moved MetaImage source with a byte-identical slice
replacement refused until the caller obtained a fresh preview.

Ten installed review, adoption and batch checks passed, along with the
matching-source preview and save and 21 reader workflow regressions.
Adoption preserved the original project and historical checkpoint rows.
Recomputation retained the same pixel digest under a new input identity.
Compound coverage hashes the primary file and checks companion file
versions. The retained CLI lacked the preview command. One cached CLI
build was needed.

[Retained refusal](../build/native-format-closeout/validation-packet/cli-source-review-baseline.log)
[Review and adoption checks](../build/native-format-closeout/validation-packet/cli-source-adoption/validation.json)
[Matching source](../build/native-format-closeout/validation-packet/cli-source-adoption/matching-source.json)
[Reader workflow regression](../build/native-format-closeout/validation-packet/source-review-create-regression/validation.json)

## Verified locally - quantify and resume a CLI-assembled mixed-reader project

The installed native CLI created and annotated a two-image project using VTI
and numbered zlib MetaImage slices. One raw batch measured a six-pixel ROI on
Z1 of each image. The selected VTI Auxiliary array gave 1054–1099 with mean
1076.5. MetaImage gave 6–11 with mean 8.5. Both retained UInt16 data, exact
calibration and their original saved reader links. The native CLI exported
both checkpoint CSVs byte for byte using the returned paths.

Reversing manifest order reused both checkpoints. Replacing a MetaImage slice
with a byte-identical copy failed only that image under continue-on-error.
VTI still reused its checkpoint, and no checkpoint or input bytes changed.
All nine end-to-end checks passed using existing binaries. The first
inspection passed an extended Windows path to Python's SQLite URI builder.
The corrected oracle uses the ordinary drive path for its independent read
and passes the original extended path to the native export command.

[Mixed-reader batch and resume receipt](../build/native-format-closeout/validation-packet/appended-raw-run/validation.json)

## Delivered locally - append a linked image to a project from the native CLI

`project-add-image` appends one linked image to a new project file through
the same image-open and atomic save path as `project-create`. A read-only
VTI project retained its selected array, image ID, active image and stored
ROI. The new project linked a MetaImage series with spaced slice names at ID 2
and copied the existing owned QMX byte for byte to its new directory.

The relocated workflow passed 14 checks, including reopening both images
and exporting the exact UInt64 ROI sum with the template matrix unavailable.
Existing outputs, the original project, its owned matrix and the new
image's companion refused as destinations. A missing companion published
no project. The earlier single-image and ROI workflow passed 21 regressions.
The affected CLI relinked once in the existing native cache.

[Retained refusal](../build/native-format-closeout/validation-packet/project-append-baseline.log)
[Installed append checks](../build/native-format-closeout/validation-packet/project-append/validation.json)
[Single-image regression](../build/native-format-closeout/validation-packet/project-append-create-regression/validation.json)

## Delivered locally - bind import file guards to the opened image

The format-import guard now compares the primary and companion file versions
on its new handles with those captured by the reader. It rejects a replacement
between reader open and guard creation. The retained guard accepted that
handoff on PPM. The annotation frame check already gave an independent
refusal. The shared guard also refuses a byte-identical replacement of a
MetaImage slice. A fresh reader accepts the current files.

Six native cases and 21 relocated CLI and desktop workflow checks passed.
Matrix and annotation results, stale directory handling, and project creation
and offline ROI export stayed exact. The first witness used a private PPM
factory without its declaration. Using the normal reader registry and checking
the guard directly isolated the gap. The final build reused the native cache
and kept earlier packages frozen.

[Retained guard witness](../build/native-format-closeout/validation-packet/stale-target-witness.log)
[Native cases](../build/native-format-closeout/validation-packet/stale-target-validation)
[Installed workflow](../build/native-format-closeout/validation-packet/stale-target-installed/validation.json)

## Delivered locally - create a linked project from a native CLI reader selection

The format CLI's `project-create` command saves a linked single-image
project through the shared reader, source guards and atomic new-only project
writer. It reports the saved path and image ID. A VTI project created with
an explicit array accepted the corresponding QMX and refused the other array
on the same canvas. The CLI-created MetaImage project protected its slice
companions and passed indexing, annotation, desktop reopen and offline ROI
export with the exact UInt64 sum. A built-in PPM project also opened in the
desktop after CLI creation.

The installed workflow passed 21 checks and the existing matrix-frame suite
passed 12. Existing targets and input aliases refused without changing bytes.
The retained CLI lacked the command. The cached build relinked the affected
format CLI once. Earlier Windows archives retain their exact binaries.

[Retained refusal](../build/native-format-closeout/validation-packet/project-create-baseline.log)
[Installed workflow](../build/native-format-closeout/validation-packet/native-project-create/validation.json)
[Matrix frame regression](../build/native-format-closeout/validation-packet/project-create-matrix-regression/validation.json)
[Built-in reopen](../build/native-format-closeout/validation-packet/native-project-create/builtin-ppm.json)

## Delivered locally - open an explicit plugin view from desktop startup

The desktop accepts `--reader-options` for an image opened with an explicit
native plugin. The installed app opened either named array in the VTI
fixture and uploaded tiles. Explicit options replaced a manifest default
that named a missing array. Saved projects reopened with their stored
options even with that default configured. The native reader rejected
ambiguous and unavailable arrays and malformed options.

The application-entry suite and 11 relocated startup checks passed,
including MetaImage 1.7.0 filenames with spaces and the legacy PPM route.
Project reopen refuses per-open options. The new entry point uses the
existing native reader and project attachment authorities with no public
GUI options ABI change. One build reused the native cache. The retained
app refused the new syntax. Earlier Windows archives retain their exact
app identities.

[Retained refusal](../build/native-format-closeout/validation-packet/desktop-reader-options-baseline.log)
[Installed startup checks](../build/native-format-closeout/validation-packet/desktop-reader-options/validation.json)
[Application-entry suite](../build/native-format-closeout/validation-packet/configure-root-02/desktop-reader-options-ctest.log)

## Delivered locally - open MetaImage companion filenames with spaces

MetaImage 1.7.0 accepts interior ASCII spaces in same-directory detached
payload names, LIST members and numbered patterns with an explicit step.
Six raw and zlib examples reopened in SimpleITK 2.5.3. The retained 1.6.0
reader refused the numbered example. Device aliases with spaces, alternate
streams and malformed ranges refuse in the native reader.

The installed module, relocated ZIP and offline rebuild each passed 125 raw
reads, seven displays, 59 refusals and 36 project checks. Removing the 20
admitted package files preserved the saved project, header and both slices.
The project reopened with its matching plugin. The cached plugin and SDK
probe built once. Earlier handoffs keep their exact reader identities.

[Retained refusal](../build/metaimage-reader/spaced-baseline.log)
[Relocated ZIP checks](../build/metaimage-reader/spaced-zip-validation/validation.json)
[Offline build checks](../build/metaimage-reader/spaced-offline-validation/validation.json)
[Removal](../build/metaimage-reader/spaced-removal.json)

## Delivered locally - open numbered MetaImage slices

MetaImage 1.6.0 expands one plain or zero-padded decimal placeholder into
up to 63 guarded same-directory XY slices. It reads raw and zlib data in
numeric Z order, including signed starts and positive strides, and verifies
that the inclusive range selects exactly the declared depth. The retained
1.5.0 reader refused the same header that SimpleITK opened.

The installed plugin, relocated ZIP and offline rebuild each passed 115
raw reads, seven displays, 55 refusals and 24 project checks. Removing
the 20 admitted package files preserved the project and its three inputs.
The project reopened with the matching module. A saved 1.5.0 project still
replays with its own module and requires explicit adoption for 1.6.0.
The cached plugin and SDK probe built once. The historical replay check
needed the probe beside its runtime DLLs. Frozen Windows handoff receipts
retain the 1.5.0 identity.

[Retained refusal](../build/metaimage-reader/numbered-baseline.log)
[Relocated ZIP checks](../build/metaimage-reader/numbered-zip-validation/validation.json)
[Offline build checks](../build/metaimage-reader/numbered-offline-validation/validation.json)
[Removal](../build/metaimage-reader/numbered-removal.json)

## Verified locally - Windows reader handoff runtime dependencies

The bounded PE verifier checks the reader ZIP's four native executables and
five provider DLLs as entry points, resolving bundled dependencies beside
the importing binary. It verified all 286 archive files, 12 PE images and
177 edges. The remaining dependencies resolve to Windows, API sets or the
declared MSVC runtime. No Python dependency or unresolved DLL was found.

All 61 verifier tests pass, including missing providers, unresolved or
Python-linked provider imports and the earlier wheel and native layouts.
The initial test exposed a changed legacy error string. The final checker
preserves that wording. No archive or native rebuild was needed. The local
account cannot enumerate Hyper-V guests, so the clean-host run remains open.

[Verified Windows dependency manifest](../build/native-format-closeout/validation-packet/reader-workflow-handoff/windows-runtime-manifest.json)
[Verifier usage](windows-runtime-manifests.md)

## Delivered locally - Windows reader host source companion

The runtime ZIP's 1,057 compiled input hashes match the relocated host source.
The source archive passed admission for all 1,077 members, including the
build recipe and notices. Offline CMake configuration generated all four
native targets from the extracted sources and retained dependency roots.
The configured identity matches the runtime ZIP and Python is disabled.
The retained dependency source ZIP passed admission with all 15 dependency
archives. The compared source files match the build roots byte for byte,
with 17 ImGui files matching after newline normalization. The staged
OpenSlide runtime matches the SDK and its retained source carrier.

The first source stage included an example README absent from the checkout.
The first configure also hit MSBuild path length during compiler detection.
The final run uses the actual example files and a short configure directory.
No engine rebuild or runtime archive change was needed. This verifies source
membership and offline configuration with prepared libraries. A clean-machine
rebuild and installation remain distinct checks.

[Host source ZIP](../build/native-format-closeout/validation-packet/reader-workflow-handoff/host-source-companion-02/qupathxx-windows-x64-reader-workflow-host-source.zip)
[Retained dependency source ZIP](../build/feature-expansion/l01-admission/dependency-sources-final/qupathxx-dependency-sources.zip)
[Dependency audit](../build/native-format-closeout/validation-packet/reader-workflow-handoff/host-source-companion-02/dependency-sources-verified.json)
[Offline configuration](../build/native-format-closeout/validation-packet/reader-workflow-handoff/host-source-companion-02/configure.json)

## Delivered locally - Windows reader workflow archive

The desktop, region, format import and batch executables now share the current
native core. The local Windows x64 ZIP binds those binaries to the exact
compiled-source manifest and includes the reviewed MetaImage, VTI, counts
and geometry providers with their sources and notices. The existing material
guard passed all 286 stage and ZIP members.

The extracted archive passed 15 checks in a new per-user directory outside the
checkout, including desktop reopening of the saved ROI project and exact
offline export. Manifest-owned removal preserved all 36 user and validation
files, including notes inside the install directory. The build relinked three
CLI targets in the existing Python-disabled cache. The initial removal shell
lacked Get-FileHash and failed before touching files. Retrying in the installed
PowerShell reused the completed workflow and removed all owned members.
This handoff is local. See the host source companion above. Fresh-machine
verification and public release remain separate gates.

[Local Windows ZIP](../build/native-format-closeout/validation-packet/reader-workflow-handoff/qupathxx-windows-x64-reader-workflow-local.zip)
[Archive receipt](../build/native-format-closeout/validation-packet/reader-workflow-handoff/archive-receipt.json)
[Installed workflow checks](../build/native-format-closeout/validation-packet/reader-workflow-handoff/workflow.json)
[Install and removal receipt](../build/native-format-closeout/validation-packet/reader-workflow-handoff/installation-removal.json)

## Verified locally - reopen the newer reader packs in the Windows desktop

The relocated app and admitted MetaImage 1.5.0 and VTI 1.7.0 ZIPs passed nine
checks. Both saved projects uploaded tiles, including a VTI project reopened
with its saved named array and the normal manifest. The MetaImage LIST
project opened in both manifest orders. Missing MetaImage and a byte-identical
replacement slice failed with their distinct reader diagnostics. Legacy image
and empty workspace smoke runs passed. Original project and input bytes
were preserved.

The retained app reported a generic zero-tile error. The native entry point
now enables existing tile settlement for smoke runs. The cached app and
application-entry suite built once, and the test passed. The first test of the
rebuilt app tried to reopen after renaming the same file back. NTFS changed
its version stamp, so it correctly still needed review. The final check tests
both manifest orders before changing the source. The app has its own hash
and local evidence. A fresh Windows machine and a combined release archive
are outside this smoke result.

[Retained diagnostic](../build/native-format-closeout/validation-packet/desktop-reader-reopen-01/missing-metaimage-provider.json)
[Installed desktop checks](../build/native-format-closeout/validation-packet/desktop-reader-reopen-03/validation.json)
[Application-entry test](../build/native-format-closeout/validation-packet/configure-root-02/desktop-tile-settlement-ctest.log)

## Verified locally - co-installed Windows reader views through QMX replay

Four relocated packs and the current cached region, format and batch CLIs
passed 14 checks. MetaImage 1.5.0 and VTI 1.7.0 preserved exact Z1 samples
and calibration in one reader catalogue. Two VTI arrays on the same canvas
produced distinct QMX frame identities. Reversing reader order preserved
MetaImage attachment, and a byte-identical replacement slice refused indexing
without publishing a new QMX. Native aggregation of the geometry TSV ROI
exported the exact UInt64 sum 9007199254740993 for the one included spot.
Verification and CSV export passed with the original image and matrix inputs
unavailable and no plugin manifests supplied.

The earlier combined Windows stage still matches its own receipt. The newer
CLI executables have separate hashes, recorded alongside the exact packs and
inputs. Reused the cached binaries and existing producer fixtures without a
build. An initial 12-check run staged only the selected counts module. The final
run stages every manifest member and additionally inspects both VTI frames.
This closes the tested installed CLI route. Track any later desktop or package
claim against the earlier combined receipt and its actual source delta.

[Installed CLI and plugin receipt](../build/native-format-closeout/validation-packet/reader-matrix-replay-02/validation.json)
[Replay checker](../tools/ci/check_installed_reader_matrix_replay.py)

## Delivered locally - read ordered MetaImage slice lists

MetaImage 1.5.0 reads XYZ LIST headers with up to 63 guarded same-directory
XY slices in header order. Each slice uses the existing exact raw or zlib
decoder. Saved projects retain the identity of every companion and refuse
byte-identical replacements. The retained 1.4.0 reader refused the same
header that SimpleITK opened.

The installed module, relocated ZIP and offline rebuild each passed 110 raw
reads, seven displays, 41 refusals and 18 project checks. Removing the 20
admitted members preserved the project, header and both slice files byte
for byte. The project reopened with the matching plugin. The cached build
reused zlib and saved the earlier probe. Packaging was repeated after the
guide update. The packer requires a fresh output directory, so the initial
staging tree and ZIP were retained under separate names.

The later 1.6.0 packet adds numbered slices. Other geometries remain separate
profiles.

[Retained refusal](../build/metaimage-reader/slice-list-baseline.log)
[Relocated ZIP checks](../build/metaimage-reader/slice-list-zip-validation/validation.json)
[Offline build checks](../build/metaimage-reader/slice-list-offline-validation/validation.json)
[Removal](../build/metaimage-reader/slice-list-removal.json)

## Delivered locally - select named VTK point arrays

VTI 1.7.0 selects one named point array per piece using `point_array`.
It accepts inactive named arrays and uses the sole active array by default.
Distinct active scalar and vector names require a choice. All appended
offsets, including unselected arrays, bound each selected payload.

The installed module, relocated ZIP and offline build each passed 191 raw
reads, 71 refusals, three display checks and 20 project checks. The matrix
of producer formats covered all four writer layouts and the existing codecs.
Removing all 31 package members preserved the source and saved project,
which reopened with its original selection. The retained 1.6.0 module refused
the same producer file. The native build reused the codec sources and probe
and required no retry. MSVC repeated warnings in unmodified XZ sources.
Alternative dataset assemblies and cell arrays remain separate profiles.

[Retained refusal](../build/vti-reader/array-selection-baseline.log)
[Relocated ZIP checks](../build/vti-reader/array-selection-zip-validation/validation.json)
[Offline build checks](../build/vti-reader/array-selection-offline-validation/validation.json)
[Removal](../build/vti-reader/array-selection-removal.json)

## Delivered locally - open multi-component VTK ImageData

VTI 1.6.0 reads a single multi-component active scalar or three-component
vector array per piece. It preserves source component labels and exact
tuples across the selected binary layouts and codecs. Raw reads can select
contiguous components. The default preview shows the first in grayscale.
Overlapping pieces must agree for the entire tuple.

The installed module, relocated ZIP and offline build each passed 141 raw
reads, 65 refusals, three display checks and 18 project checks. Removing the
29 package members preserved the vector source and project, which reopened
exactly with the matching module. The retained 1.5.0 module rejected both
producer layouts. The first candidate additionally refused VTK's cached
norm-range metadata. The retry validates that structure and uses raw values
for analysis and display. Build reuse and the earlier SDK probe were
preserved. MSVC repeated its warnings in the unmodified XZ sources.

[Retained scalar-array refusal](../build/vti-reader/component-scalars-baseline.log)
[Retained vector-array refusal](../build/vti-reader/component-zlib-baseline.log)
[Relocated ZIP checks](../build/vti-reader/components-zip-validation/validation.json)
[Offline build checks](../build/vti-reader/components-offline-validation/validation.json)
[Removal](../build/vti-reader/components-removal.json)

## Delivered locally - preserve 64-bit MetaImage samples

MetaImage 1.4.0 reads SimpleITK 2.5.3 `MET_LONG_LONG` and `MET_ULONG_LONG`
rasters through native Int64 and UInt64. The retained 1.3.0 plugin refused
both tags. Exact values at the integer limits and above 2^53 survive scalar
and vector reads, either byte order, zlib and detached project replay.

The installed module, relocated ZIP and offline rebuild each passed 105 raw
reads, seven display checks, 32 refusals and 12 project checks. Removing the
20 package files preserved a saved UInt64 project, header and payload byte
for byte. Reopening with the matching module reproduced both selected
components. Both builds reused the pinned zlib and the retained SDK probe
was preserved before recompiling it. No retry was needed. Nonidentity geometry
and other MetaImage profiles remain in the format queue.

[Retained signed refusal](../build/metaimage-reader/wide-baseline-i64.log)
[Retained unsigned refusal](../build/metaimage-reader/wide-baseline-u64.log)
[Relocated ZIP checks](../build/metaimage-reader/wide-zip-validation/validation.json)
[Offline rebuild checks](../build/metaimage-reader/wide-offline-validation/validation.json)
[Removal](../build/metaimage-reader/wide-removal.json)

## Delivered locally - read multi-component MetaImage rasters

MetaImage 1.3.0 preserves ordered components in binary MHA/MHD, including
single-stream zlib and opposite-endian samples. Raw reads support contiguous
component ranges. The default preview uses component 1 in grayscale. The
retained 1.2.0 module refused four-component SimpleITK 2.5.3 output.

The installed module, relocated ZIP and offline rebuild each passed 54 raw
reads, five display checks, 32 refusals and nine project checks. A detached
vector project reopened exactly and refused a changed companion. Removal
preserved the saved project and its header and payload byte for byte.
The first test attempt used the wrong CLI option name for the first component.
The corrected harness used `--channel`. Native builds reused the cache and
the pinned zlib source. The older SDK probe executable was preserved.
Nonidentity geometry and other MetaImage profiles remain in the format queue.

[Retained failure](../build/metaimage-reader/vector-baseline.log)
[Relocated ZIP checks](../build/metaimage-reader/vector-zip-validation/validation.json)
[Offline rebuild checks](../build/metaimage-reader/vector-offline-validation/validation.json)
[Removal](../build/metaimage-reader/vector-removal.json)

## Delivered locally - open VTK LZMA-compressed ImageData

VTI 1.5.0 reads the CRC32 XZ blocks written by VTK 9.4.2 in inline,
appended and serial multi-piece ImageData. The retained 1.4.0 module
refused this output. The bounded decoder checks the full stream, checksum
and exact scalar byte count. The package pins a reproducible source subset
of XZ Embedded and includes its 0BSD notices.

The installed module, relocated ZIP and offline rebuild each passed 108 raw
reads, 57 refusals, two display checks and 16 project checks. The relocated
and rebuilt modules each preserved both planes of a separate VTK fixture
containing three range-coded LZMA2 blocks. Removal preserved the source and
saved project byte for byte. The first stage exposed the missing SPDX 0BSD
entry in the offline admission guard. The original notice matched that
license and the next stage admitted all 29 members. MSVC emitted warnings
from the unmodified upstream C source. Both builds and functional checks
passed. VTK's documented zlib, LZ4 and LZMA writer modes now have local receipts.

[Retained failure](../build/vti-reader/lzma-baseline.log)
[Relocated ZIP checks](../build/vti-reader/lzma-zip-validation/validation.json)
[Offline rebuild checks](../build/vti-reader/lzma-offline-validation/validation.json)
[Range-coded planes](../build/vti-reader/lzma-large/validation.json)
[Removal](../build/vti-reader/lzma-removal.json)

## Delivered locally - open VTK LZ4-compressed ImageData

VTI 1.4.0 reads VTK 9.4.2 LZ4 ImageData in inline base64, appended base64,
raw-appended and streamed binary layouts. Existing geometry, block and
workspace bounds apply. The retained 1.3.0 plugin refused the same producer
input. The reader preserves exact stored samples and rejects damaged or
incorrectly sized blocks.

The installed module, relocated ZIP and offline source rebuild each passed
82 raw reads, 51 refusals, two display checks and 12 project checks, including
uncompressed and zlib regressions. Removing the 25 admitted package files
preserved the project and source byte for byte. The project reopened after
restoring the matching module. The build reused the existing cache and exact
admitted LZ4 source and notice. No build retry was needed. Other compressors
remain separate profiles in the format queue.

[Retained failure](../build/vti-reader/lz4-baseline.log)
[Relocated ZIP checks](../build/vti-reader/lz4-zip-validation/validation.json)
[Offline rebuild checks](../build/vti-reader/lz4-offline-validation/validation.json)
[Removal](../build/vti-reader/lz4-removal.json)

## Delivered locally - use the stored QMX frame in CLI ROI workflows

Offline `matrix-info` reports the saved frame ID, source identity, units,
plane, dimensions and available calibration. A source-mode regional spec may
omit the frame ID to use an exact saved level-zero QMX frame. Plugin-backed
images require reader-bound evidence for this route. Registered workflows
and legacy plugin frames use explicit IDs.

The relocated Windows CLIs passed 12 checks and the native aggregation suite
passed. Implicit and explicit runs produced the retained record and result
hashes, and the offline CSV matched byte for byte. Conflicting planes,
implicit registration and attempts to infer a legacy plugin frame refused.
The build reused the native cache and fixtures. No build retry was needed.
Use the remaining application and format queues to select the next blocker.

[Retained metadata](../build/format-catalogs/matrix-frame-cli/baseline-info.json)
[Retained missing-frame failure](../build/format-catalogs/matrix-frame-cli/baseline-aggregation.log)
[Relocated CLI checks](../build/format-catalogs/matrix-frame-cli/relocated-validation/validation.json)
[Native regression suite](../build/format-catalogs/matrix-frame-cli/ctest.log)

## Delivered locally - bind project matrices to the reader view and use indexed ROI positions

The retained CLI attached a project-indexed APNG frame 0 matrix to frame 1
with the same dimensions. Project-based file and store indexing now records
the same reader-bound frame as standalone indexing. Matching attachments
pass and alternate views refuse. A new plugin attachment from a legacy QMX
requires reindexing. Existing attachments and QMX metadata remain readable.

The first batch run showed that ROI aggregation required detection objects
even when an attached QMX contained exact positions. The native spatial
request now reads embedded coordinates in bounded pages when the frame
matches and no backed detection objects exist for the source. The relocated
CLIs passed 46 checks, including exact UInt64 ROI sums for a new attachment,
an existing legacy association and a spot on the second coordinate page.
Offline verification and export passed, as did both native regression suites.
The incremental rebuild followed the reproduced batch failure. The previous
native binaries and both failure logs remain available. Further application
gaps stay in the existing queue.

[Retained wrong-view attachment](../build/format-catalogs/project-matrix-reader-identity/baseline.log)
[Retained ROI failure](../build/format-catalogs/project-matrix-reader-identity/relocated-validation/reader-bound-aggregate.log)
[Relocated checks](../build/format-catalogs/project-matrix-reader-identity/indexed-roi-validation/validation.json)
[Native regression suites](../build/format-catalogs/project-matrix-reader-identity/ctest.log)

## Delivered locally - index standalone matrices against a selected reader view

Both standalone matrix commands accept an explicit image reader and its
invocation options through the shared native resolver. New explicit plugin
frames hash the effective implementation and normalized options. A QMX indexes
both selected FITS HDUs, and the AnnData store indexes against and attaches to
the saved FITS view. An APNG QMX attaches to its saved view and rejects another
view with the same canvas. Built-in automatic and saved-project output matches
the retained CLI byte for byte.

The relocated Windows host passed 23 checks, including provider-free QMX
metadata, malformed options and new-only attachment. The cached native build
also compiled already-modified native inputs. The first oracle incorrectly
assumed an HDU or position rejection and was corrected to compare actual
frame identities and an unavailable HDU. The relocated host and pre-change
executable are preserved separately. Next check the remaining application
queue for a reproducible consumer failure.

[Retained failure](../build/format-catalogs/matrix-reader-selection/baseline-explicit.log)
[Native build](../build/format-catalogs/matrix-reader-selection/build.log)
[Relocated host checks](../build/format-catalogs/matrix-reader-selection/relocated-validation/validation.json)

## Delivered locally - preserve whole-pixel OME mask transforms

The retained 1.2.0 module refused a schema-valid non-square mask rotated
90 degrees. Version 1.3.0 maps covered unit cells exactly for all eight
grid-preserving XY orientations with integral translations. Scaling, shear,
degenerate transforms, fractional translation and crops beyond the target
canvas refuse atomically.

Twelve imports, twelve offline reopens, 35 refusal controls and five vector
regressions passed against the cached module, relocated ZIP and offline source
rebuild. The pinned OME schema accepted all eight authored orientation fixtures.
Removing the 96 package members preserved both checked projects byte for byte.
The host and codec objects were reused. Rebuilding the SDK probe replaced its
cached executable. The earlier package retains its probe source and receipts,
and the new executable is preserved by version. Next reconcile open application
and integration blockers before selecting another format extension.

[Schema verification](../build/gui-aggregation/catalogue-closure/ome-roi/mask-grid-schema.json)
[Relocated ZIP checks](../build/gui-aggregation/catalogue-closure/ome-roi/mask-grid-zip-validation/validation.json)
[Offline source rebuild checks](../build/gui-aggregation/catalogue-closure/ome-roi/mask-grid-offline-validation/validation.json)
[Package admission](../build/gui-aggregation/catalogue-closure/ome-roi/package-mask-1.3.0-admission.json)
[Removal](../build/gui-aggregation/catalogue-closure/ome-roi/mask-grid-removal.json)

## Delivered locally - import compressed OME XML bit masks

OME ROI 1.2.0 reads zlib and bzip2 BinData in the existing ABI5 mask workflow.
Both decode to the exact packed crop and reopen with identical coverage, holes,
disconnected parts, plane and lock. Truncated, trailing, concatenated and
wrong-size streams refuse before publication.

Four imports, four offline reopens, 29 refusal controls and five vector
regressions passed against the cached module, relocated 96-member ZIP and
offline source rebuild. Removing those members preserved and reopened both
compressed-mask projects. The first compile exposed Windows min/max macros
from the bzip2 header. A module-local NOMINMAX definition resolved it. The
retained host, SDK and Expat cache were reused. The next annotation candidate
needs representative evidence for an exact whole-pixel transform beyond
translation.

[Relocated ZIP checks](../build/gui-aggregation/catalogue-closure/ome-roi/mask-compressed-zip-validation/validation.json)
[Offline source rebuild checks](../build/gui-aggregation/catalogue-closure/ome-roi/mask-compressed-offline-validation/validation.json)
[Package admission](../build/gui-aggregation/catalogue-closure/ome-roi/package-mask-1.2.0-admission.json)
[Removal](../build/gui-aggregation/catalogue-closure/ome-roi/mask-compressed-removal.json)

## Delivered locally - import OME XML bit-mask annotations

OME ROI 1.1.0 imports uncompressed BinData masks against an explicit
Image/Pixels canvas through the existing ABI5 mask contour and project service.
It validates base64, bit extents and exact canvas/plane alignment. Integer
translations preserve whole pixels, disconnected parts, holes and edit locks.

Two imports, two offline reopens, 15 refusal controls and five vector
regressions passed. The relocated 90-member ZIP and offline rebuild each passed
the suite. Removal preserved the exact project and its saved mask reopened.
The retained ABI5 CLI and Expat cache were reused. The old ABI4 test overlay
could not link current matrix/store symbols, so the reopen probe uses the
retained SDK directly. The fixture switched to the existing quantitative volume
after the single-plane template correctly refused Z/T=1. Compressed BinData
and arbitrary mask transforms remain separate profiles.

[Relocated package checks](../build/gui-aggregation/catalogue-closure/ome-roi/mask-zip-validation/validation.json)
[Offline source rebuild checks](../build/gui-aggregation/catalogue-closure/ome-roi/mask-offline-validation/validation.json)
[Package admission](../build/gui-aggregation/catalogue-closure/ome-roi/package-mask-1.1.0-admission.json)
[Removal](../build/gui-aggregation/catalogue-closure/ome-roi/mask-removal.json)

## Delivered locally - register projected GeoPackage annotations

Version 1.1.0 accepts a projected EPSG layer with an explicit source-to-pixel
affine. It matches the contents and geometry-column SRS references, EPSG row,
projected WKT root and binary feature SRS. Existing WKB and affine authorities
retain exact source envelopes and mapped geometry. Saved receipts keep the
local SRS key, EPSG identity and coefficients.

Three imports, three offline reopens and 13 refusals passed. The translated,
reflected and remapped local SRS key cases preserved feature IDs, line vertices
and polygon holes. Seventeen legacy GeoPackage and WKB checks passed.
The 32-member ZIP and the offline rebuild each passed the registration suite.
Removal preserved the saved project and its annotations reopened without the
provider. The cached build reused SQLite. The two retries aligned the
descriptor version with the manifest and the oracle with the native image
bounds message. Geographic reprojection remains outside the tested profile.

[Baseline](../build/geopackage-annotations/projected-baseline.log)
[Relocated package checks](../build/geopackage-annotations/projected-zip-validation/validation.json)
[Offline source rebuild checks](../build/geopackage-annotations/projected-offline-validation/validation.json)
[Legacy checks](../build/geopackage-annotations/projected-legacy-regression/validation.json)
[ZIP member admission](../build/geopackage-annotations/package-1.1.0-admission.json)
[Removal](../build/geopackage-annotations/projected-removal.json)

## Delivered locally - open bytes-only and big-endian Zarr 3 shard indexes

The optional 1.17.0 module reads fixed-size UInt64 indexes in either byte
order at the start or end of a shard. CRC32C is verified when declared.
The same safeguards reject half-fill sentinels, out-of-bounds and overlapping
ranges. Outer-shard checksums still protect bytes-only indexes when declared.

51 index cases, 46 legacy commands and 19 Blosc regression cases passed.
The 143-member ZIP and offline rebuild each passed the 38 new producer index
cases. Removal preserved the host's shared DLLs and the user's export.
The initial oracle located sharding only as a sole outer codec. Selecting the
named stage now covers outer transposes and CRC32C. The build reused the module
cache and pinned sources. Next candidate is explicit affine registration for
GeoPackage annotations, using the existing GeoParquet registration contract.

[Baseline](../build/gui-aggregation/catalogue-closure/ngff05/v117-index-baseline.log)
[Delivery evidence](benchmarks/ngff05/README.md#bytes-only-and-big-endian-shard-indexes-in-1170)

## Delivered locally - read Zarr Blosc zlib and Snappy blocks

The optional 1.16.0 module now reads all six compressor names listed by the
Zarr 3 Blosc specification within the tested regular and indexed-shard profiles.
It uses the retained SDK zlib and admitted Snappy 1.2.2 sources. Zarr-Python
produced the zlib stores. The Snappy fixture wraps independently encoded
PyArrow Snappy blocks because the pinned Python Blosc lacks that encoder.

97 zlib producer commands, 19 compressed-frame cases and 76 standalone
codec regression commands passed. The 142-member ZIP and offline rebuild each
passed the frame suite. Removal preserved the host's DLLs and the user's array.
The Snappy configure retry honored the parent options through CMP0077.
The trailing-byte oracle now follows the producer's actual split table.

[Baseline](../build/gui-aggregation/catalogue-closure/ngff05/v116-zlib-baseline.log)
[Delivery evidence](benchmarks/ngff05/README.md#blosc-zlib-and-snappy-in-1160)

## Delivered locally - read the Zarr Blosc/Zstandard profile

The optional 1.15.0 module reads the pinned Zarr-Python producer's default
Blosc compressor in regular chunks and indexed shards. Its inner splits use
the same bounded native Zstandard decoder as ordinary chunks. The producer
checks cover wide UInt64 samples, big-endian bytes, transposes and checksums.
97 producer commands, 13 actual compressed-frame cases and 76 standalone
codec regression commands passed. The 138-member ZIP and offline rebuild
passed the 13 frame checks. Removal preserved host DLLs and the user's array.

The module build used the admitted C-Blosc and Zstandard archives in the
existing cache. Staging borrowed the already admitted notice once after the
initial duplicate registration. Packaged sources regenerated the same
decoder source. Version 1.16.0 extends this baseline to zlib and Snappy.

[Baseline](../build/gui-aggregation/catalogue-closure/ngff05/blosc-zstd-baseline.log)
[Delivery evidence](benchmarks/ngff05/README.md#blosc-zstandard-in-1150)

## Delivered locally - read Blosc-compressed OME-Zarr 0.5 images

The optional 1.14.0 reader decodes LZ4, LZ4HC and BloscLZ in regular chunks
and indexed shards with exact raw samples and bounded single-threaded work.
75 Blosc producer commands, ten actual compressed-frame checks and 76 gzip
and Zstandard regression commands passed. The relocated ZIP and offline
rebuild each passed the ten frame checks. Material admission verified 140
members. Removal preserved the user's export and the host's existing DLLs.

Corrected the fixture codec spelling, used one source for manifest and ABI
version values, and repaired the static archive inventory's composite terms.
The gzip and Zstandard regression invocations now use their producer metadata.
The removal check now records the preexisting OpenSlide DLL before installation.
Existing reader binaries were retained. Blosc's other compressor families,
including the producer's Blosc/Zstandard default, remain separate profiles.

[Retained reader baseline](../build/gui-aggregation/catalogue-closure/ngff05/blosc-baseline.log)
[Delivery evidence](benchmarks/ngff05/README.md#blosc-chunks-in-1140)

## Delivered locally - use installed reader packs in batch and label workflows

Batch and label CLIs share a repeatable reader-manifest bootstrap. One raw batch
saved ENVI and FITS measurements with the same per-image input and pixel digests,
record IDs and stable CSV values as the prior single-reader baseline. Reversing
manifest order reused both checkpoints byte for byte. Both label entry points
returned the exact prior raw-input identities. A third pack supplied PNG mask
decoding on both images. Both label records replayed exactly and both saved
source links retained their original bytes.

Only the affected CLI targets built in the existing Python-free cache.
The initial oracle compared across independent execution timing and source
observation times. The corrected oracle checks stable result fields against
the earlier host and hashes the exact persisted checkpoints across resume.

[Baseline](../build/reader-catalogs/batch-label/baseline.json)
[Build](../build/reader-catalogs/batch-label/build.log)
[Workflow checks](../build/reader-catalogs/batch-label/validation-02/validation.json)

## Delivered locally - configure installed image reader packs together

Native C++, region inspection, desktop startup and format import accept up to
32 absolute image-reader manifests in one catalog. Modules resolve relative to
their own packages. Conflicting provider IDs refuse before loading and a missing
or conflicting manifest leaves unrelated providers available. The one-manifest
catalogs match the prior host exactly for both installed providers.

Eight check groups passed. One project with relocated ENVI and FITS readers
reopened in a fresh process with reversed pack order. Native checks preserved
raw samples, validity and exact saved link bytes. The desktop opened both images
and the CLI imported a TSV point on each while preserving both source links.
Three existing native suites passed. The region build added the declaring reader
ID header after the first failure. Probe retries corrected the live-observation
comparison for per-open generations and capture times while keeping the saved
link byte assertion. The build reused the native cache and installed modules.

[Baseline](../build/reader-catalogs/baseline.json)
[Installed workflow checks](../build/reader-catalogs/validation-03/validation.json)
[Native reader suites](../build/reader-catalogs/ctest.log)
[Final project suite](../build/reader-catalogs/ctest-observation.log)

## Delivered locally - configure installed format packs together

The native CLI and C++ catalog can configure up to 32 explicit format
manifests before first use. Each package resolves its own module paths.
Cross-manifest ID collisions refuse before DLL loading. Twelve checks cover
matrix and annotation imports, input order, isolated failures and exact
single-manifest receipts and QMX. One combined project reopens both results
with original inputs and the external QMX absent. The legacy ABI1 direct
checks, six native host cases and two catalog/header tests pass. One cached
host build reused the installed modules. The build had no failed retries.
[Baseline](../build/format-catalogs/baseline.json)
[Combined catalog checks](../build/format-catalogs/validation-01/validation.json)
[Native build](../build/format-catalogs/build.log)

## Delivered locally - register projected GeoParquet annotations to a saved image

GeoParquet 1.2.0 imports WKB and separated GeoArrow with a matching projected
EPSG declaration and explicitly supplied affine. The relocated ZIP passed
18 imports, 12 exact reopen checks and 31 refusals. Tests cover translated,
reflected and sheared coordinates, source-space bbox, exact saved coefficients,
invalid CRS/affines and the native image-bounds refusal. The affine calculation
is shared with native registration. Its two suites passed using the rebuilt
registration object over the verified SDK. The GeoPackage regression passed.
The build reused the pinned Parquet SDK without failed retries. Both delivery
forms admit 50 members. Packaged sources configured against that SDK. Automatic
reprojection, geographic coordinates and Z/M remain separate profiles.
[Baseline](../build/geoparquet/registration/baseline.json)
[Packaged checks](../build/geoparquet/registration/packaged-validation/validation.json)
[Native registration suites](../build/geoparquet/registration/core-overlay/cache/Testing/Temporary/LastTest.log)
[GeoPackage regression](../build/geoparquet/registration/geopackage-regression/validation.json)
[Package provenance](../build/geoparquet/registration/package-1.2.0/SOURCE.json)

## Delivered locally - import native GeoArrow shapes from GeoParquet

GeoParquet 1.1.0 now decodes all six native separated Float64 XY encodings.
The relocated ZIP passed 16 imports, ten exact reopen checks and 21 refusals.
Six checks reopen paired native/WKB projects and compare exact shapes, source
identity and collection paths. Tests cover malformed rings, null geometry,
incompatible nesting and absent CRS. The prior outside-image case is already
covered by the native annotation append test. The build reused the verified
Parquet SDK with no failed retries. The matrix and point regressions passed
and both package forms admit 49 members. Packaged sources configured against
the verified SDK. Registered coordinates and Z/M remain outside this profile.
[Producer baseline](../build/geoparquet/geoarrow/baseline.json)
[Packaged checks](../build/geoparquet/geoarrow/packaged-validation/validation.json)
[Matrix regression](../build/geoparquet/geoarrow/matrix-regression/validation.json)
[Point regression](../build/geoparquet/geoarrow/points-regression/validation.json)
[Package provenance](../build/geoparquet/geoarrow/package-1.1.0/SOURCE.json)

## Delivered locally - import GeoParquet image-pixel annotations

The native ABI3 provider imports a selected XY WKB column with explicit null
CRS. The relocated ZIP passed four imports, four exact offline reopens and
17 refusals. Coverage includes the GeoPandas writer, polygon holes, multiple
geometry columns, a big-endian collection, class mapping, and null versus
absent or declared CRS. The rebuilt GeoPackage provider passed its full
regression using the shared WKB bounds authority. Producer setup added
dateutil and the harness matched the native options diagnostic after their
initial failures. The pinned decoder SDK was reused. The 48-member delivery
configured against it from the packaged sources. Geographic registration and
GeoArrow encodings remain separate profiles.
[Baseline](../build/geoparquet/baseline.json)
[Packaged checks](../build/geoparquet/packaged-validation/validation.json)
[GeoPackage regression](../build/geoparquet/geopackage-regression/validation.json)
[Package provenance](../build/geoparquet/package-1.0.0/SOURCE.json)

## Delivered locally - import Parquet point annotations

The optional native ABI3 provider imports explicit level-zero point columns.
The relocated package passed three imports, three exact offline reopens and
16 refusals across Snappy, Zstandard and uncompressed files. Checks cover
dictionary pages, nullable labels, native class mapping, wide coordinates and
page checksums. The shared Parquet matrix and Arrow point regression suites
passed all existing cases. The build reused the verified static decoder SDK
without failed retries. Both package forms admit 48 members. Packaged sources
configured against that SDK. GeoParquet geometry and CRS handling remain open.
[Baseline](../build/parquet-points/baseline.json)
[Packaged checks](../build/parquet-points/packaged-validation/validation.json)
[Parquet matrix regression](../build/parquet-points/matrix-regression/validation.json)
[Arrow point regression](../build/parquet-points/arrow-regression/validation.json)
[Package provenance](../build/parquet-points/package-1.0.0/SOURCE.json)

## Delivered locally - index Parquet expression matrices from a saved project

The optional Parquet ABI2 provider shares the Arrow matrix option and transport
code. The relocated ZIP passed three imports, three exact owned reopens and 15
refusals. Checks cover default Snappy counts, Zstandard continuous values, plain
buffers, ordered features and row groups, wide counts, the position join and
page checksums. The rebuilt Arrow provider passed two imports, two owned reopens
and 12 refusals. The pinned static decoder built from verified sources. The build
retry supplied .env and normalized archive paths. Staging retained both
identical uriparser notices with one lineage ID. Both package forms admit
47 members. Packaged plugin sources configured against the verified local
decoder SDK. A cold rebuild of that SDK from the package was not repeated.
[Baseline](../build/parquet-matrix/baseline.json)
[Arrow regression](../build/parquet-matrix/arrow-regression/validation.json)
[Packaged checks](../build/parquet-matrix/packaged-validation/validation.json)
[Package provenance](../build/parquet-matrix/package-1.0.0-final/SOURCE.json)

## Delivered locally - index Arrow expression matrices from a saved project

The optional ABI2 provider shares the native Arrow file decoder with the
point provider. The relocated package passed two imports and exact owned
project reopens for UInt64 counts and mixed Float32/Float64 continuous values.
Checks verify ordered axes, dictionary IDs, shuffled position joins, wide counts,
signed zero, subnormals and twelve refusals. The rebuilt point provider passed
its ten imports, ten reopens and 23 refusals. The compile retry validated the
actual matrix-axis request layout. The harness retry matched the native
position-set diagnostic. Both matrix delivery forms admit 41 members, and
offline configuration regenerated the patched sources byte for byte.
Parquet and additional table layouts remain open.
[Baseline](../build/arrow-matrix/baseline.json)
[Packaged checks](../build/arrow-matrix/packaged-validation/validation.json)
[Point regression](../build/arrow-matrix/point-regression/validation.json)
[Package provenance](../build/arrow-matrix/package-1.0.0/SOURCE.json)

## Delivered locally - import Zstandard-compressed Arrow point files

Arrow points 1.3.0 imports PyArrow Zstandard IPC files with plain text, UTF-8
dictionaries and ordered deltas. The relocated ZIP passed ten imports, ten
exact offline reopens and 23 refusals, including damaged frames and oversized
decompression. The plugin build reused the admitted Zstandard 1.5.7 source and
BSD notice. No host or SDK rebuild or failed retry was needed. Both package
forms admit 38 members. Offline configuration regenerated the modified
decoder sources byte for byte. Continue with the next demonstrated workflow.
[Prior failure](../build/arrow-annotations/delta-packaged-validation/validation.json)
[Packaged checks](../build/arrow-annotations/zstd-packaged-validation/validation.json)
[Package provenance](../build/arrow-annotations/package-1.3.0/SOURCE.json)

## Delivered locally - import Arrow files with dictionary deltas

Arrow points 1.2.0 appends ordered UTF-8 and large UTF-8 dictionary batches.
The relocated ZIP passed seven imports, seven exact offline reopens and 22
refusals. Cases include an uncompressed delta, two successive LZ4 deltas,
an unused null entry and refusal when the combined dictionary exceeds
100,000 entries. The provider built in the existing cache without host or
SDK rebuilds or failed retries. Both delivery forms admit 36 members.
Offline configuration regenerated the patched decoder sources byte for byte.
Continue with the next demonstrated eligible workflow.
[Prior failure](../build/arrow-annotations/dictionary-packaged-validation/validation.json)
[Packaged checks](../build/arrow-annotations/delta-packaged-validation/validation.json)
[Package provenance](../build/arrow-annotations/package-1.2.0/SOURCE.json)

## Delivered locally - import Feather categorical IDs and labels

Arrow points 1.1.0 resolves UTF-8 dictionary IDs, names and classes in both
uncompressed and default PyArrow LZ4 files. The relocated ZIP passed five imports,
five exact offline reopens and 22 refusals, including the plain-file regressions.
The packaged sources configure offline and regenerate the two patched nanoarrow
sources byte for byte. Both delivery forms admit 35 members. The compile retry
fixed an unmatched generated accessor delimiter. The test retry closed the
PyArrow delta fixture before the Windows native read lease. Delta dictionaries,
Parquet and broader table profiles remain open. Continue with the next
demonstrated eligible workflow.
[Writer baseline](../build/arrow-annotations/dictionary-baseline.json)
[Packaged checks](../build/arrow-annotations/dictionary-packaged-validation/validation.json)
[Package provenance](../build/arrow-annotations/package-1.1.0/SOURCE.json)

## Delivered locally - import Feather v2 image-pixel points

The optional native ABI3 provider imports uncompressed Arrow IPC V5 and PyArrow
Feather v2 with default LZ4. The relocated ZIP passed three imports, three
exact fresh-process project reopens and 18 refusals. The source IDs, coordinates,
names, class mapping and frame survive reopen with the originals moved aside.
Both delivery forms admit 34 members and the packaged sources configure offline.
Build retries corrected Windows allocator declarations/alignment, added the
pinned footer support, and used the verified FlatCC union accessor. The test
retry seeded actual native classes and matched the existing ID diagnostic.
Parquet, dictionaries, other codecs and broader table workflows remain open.
Continue with the next demonstrated eligible workflow.
[Writer baseline](../build/arrow-annotations/baseline.json)
[Packaged checks](../build/arrow-annotations/packaged-validation/validation.json)
[Package provenance](../build/arrow-annotations/package-1.0.0/SOURCE.json)

## Delivered locally - import GeoPackage image-pixel annotations

The optional native provider imports one selected XY feature layer with
undefined Cartesian SRS and explicit image-pixel interpretation. The relocated
package passed two imports, two exact reopens, 12 refusals and the rebuilt
shared WKB control. Cases cover the GDAL writer, big-endian geometry,
quoted layer names, feature IDs and polygon holes. The build retry renamed
the SQL identifier helper to avoid C++ `std::quoted` lookup; the harness retry
closed each SQLite fixture before requesting the native read lease. Both
delivery forms admit 29 members and the staged sources configure offline.
Continue with the next demonstrated eligible workflow.
[Writer baseline](../build/geopackage-annotations/baseline.json)
[Validation](../build/geopackage-annotations/packaged-validation/validation.json)
[Package provenance](../build/geopackage-annotations/package-1.0.0/SOURCE.json)

## Delivered locally - bind the SVG canvas to the target image

SVG 1.1.0 negotiates annotation ABI6. The native host compares the declared
canvas to the saved level-zero dimensions before reading object pages. The
relocated package passed two imports, two exact reopens with saved canvas
dimensions, and 17 refusals. The old 8-by-6 overlay now refuses against the
4-by-3 target even though its shapes fit. The old host refuses the new ABI,
and the ABI3 preview, mappings and receipt reproduce on both hosts. The C
header test passed. The cached module build reused Expat. Both plugin delivery
forms admit 86 members. Continue with the next demonstrated eligible workflow.
[Mismatch baseline](../build/svg-annotations/canvas-check/baseline.json)
[Validation](../build/svg-annotations/canvas-check/packaged-validation/validation.json)
[Package provenance](../build/svg-annotations/canvas-check/package-1.1.0/SOURCE.json)

## Delivered locally - import SVG image-pixel annotations

The optional native ABI3 provider preserves the viewport, child transforms,
exact supported types, SVG source IDs, selected paint and even-odd holes.
The relocated 1.0.1 package passed two complete imports, two exact fresh-process
project reopens and 15 refusals. The duplicate-ID harness retry used the
native diagnostic. A follow-up control reproduced ignored stylesheet
processing instructions in the preliminary package. Version 1.0.1 rejects
them, retains the XML declaration and reuses the Expat objects. The writer
reproduced the fixture hash and both final delivery forms admit 86 members.
Continue with the next demonstrated eligible workflow.
[Writer baseline](../build/svg-annotations/baseline.json)
[Validation](../build/svg-annotations/packaged-validation-1.0.1/validation.json)
[Package provenance](../build/svg-annotations/package-1.0.1/SOURCE.json)

## Delivered locally - index a matrix store against a saved project image

The new directory route uses the saved ENVI reader and the same read-only
project target checks as single-file indexing. Twelve checks cover the
10,240-by-10,240 sparse AnnData store, source and output protection, exact
store receipts and fresh-process owned reads with the store, positions and
original QMX moved aside. The legacy image-path store command produced
byte-identical QMX. The single-file project regression passed all 13 checks.
One cached CLI build reused admitted providers. Continue with the next
demonstrated eligible workflow.
[Baseline](../build/ann-project-store-index/baseline.json)
[Directory checks](../build/ann-project-store-index/validation/results.json)
[File regression](../build/ann-project-store-index/file-regression/results.json)

## Delivered locally - index a matrix against a saved project image

The native project-index operation and CLI use the saved image reader and the
shared file-matrix writer. The ENVI project previously failed through
automatic image selection. Thirteen checks now cover read-only sparse MuData
indexing, protected inputs and a changed ENVI companion. Attachment succeeds
after the H5MU and positions are moved aside. A fresh SDK process reopens the
owned copy with the original QMX absent and verifies the exact sparse values
and saved image frame. The ordinary image-path route produces byte-identical
QMX. Its baseline harness first expected a TIFF diagnostic, then recorded the
actual OpenSlide refusal. The plugin packages are reused.
Continue with the next demonstrated eligible workflow.
[Baseline](../build/mudata/project-index/baseline.json)
[Project checks](../build/mudata/project-index/validation/results.json)
[Image-path check](../build/mudata/project-index/image-route.json)

## Delivered locally - preserve sparse MuData publication at scale

MuData 1.1.0 uses matrix ABI5 and the shared sparse writer. The retained
writer fixture has 144 million logical cells and three stored UInt64 entries.
The local ABI5 host indexes only those entries, including explicit zero and
both wide counts, into a 3.21 MB QMX. The relocated package passed six imports,
six exact offline SDK reads including the empty sparse axis case, and 12
refusals. The C header contract passes. Four earlier file/directory ABI outputs
match byte for byte and the wrong source routes refuse. The compatibility
probe retry isolated the directory-provider guard from output protection.
Both plugin delivery forms admit 86 members and configure offline. The host
and SDK are new local build identities.
Continue with the next demonstrated eligible workflow.
[Writer baseline](../build/mudata/sparse-scale/baseline.json)
[Validation](../build/mudata/sparse-scale/packaged-validation/validation.json)
[Compatibility](../build/mudata/sparse-scale/compatibility/results.json)
[Package provenance](../build/mudata/sparse-scale/package-1.1.0/SOURCE.json)

## Delivered locally - index a selected MuData modality with ABI2

The optional native ABI2 provider imports one named modality and matrix from
H5MU through the existing position join and QMX writer. The relocated package
passed four imports, four exact SDK reopens with source files moved aside, and
12 refusals. The cases cover CSR UInt64 counts, CSC continuous values on a
different observation axis, dense Float64 and a compressed big-endian layer.
The rebuilt Loom module passed both focused CLI import smokes.
Build retries added the public ABI include and the HDF5 Windows system library.
The validation retry matched the external-link diagnostic.
Both delivery forms admit 86 members and the packaged sources configure offline.
ABI2 publication uses bounded dense pages, including for sparse inputs.
Continue with the next demonstrated eligible workflow.
[Writer baseline](../build/mudata/baseline.json)
[Validation](../build/mudata/packaged-validation/validation.json)
[Package provenance](../build/mudata/package-1.0.0/SOURCE.json)

## Delivered locally - open ENVI research rasters

The optional native reader opens uncalibrated ENVI Standard .hdr/.img pairs.
The relocated package passed 12 exact raw checks, three preview comparisons,
13 refusals and three fresh-process project checks, including a replaced
companion. The CLI refuses NPY crops with invalid pixels, so the SDK probe
checks the clipped validity mask and the CLI checks its refusal. The project
probe retry corrected its region argument order. The shared packed reader
passed the existing GIF suite. Both ENVI delivery forms admit 18 members.
Sources configure offline, and all six Spectral Python fixture hashes reproduce.
Continue with the next demonstrated eligible workflow.
[Writer baseline](../build/envi-reader/baseline.json)
[Validation](../build/envi-reader/packaged-validation/validation.json)
[Package provenance](../build/envi-reader/package-1.0.0/SOURCE.json)

## Delivered locally - import continuous NumPy archives

The native ABI2 provider imports finite Float32 and Float64 arrays with a
named interpretation through the existing position join and QMX writer.
The relocated package passed three imports, three exact offline SDK reads
with the archive, positions and image moved aside, and eleven refusals.
The controls retain signed zero, binary32 and binary64 subnormals, exact
axis IDs, coordinates and provider identity. The rebuilt counts provider
passed its three imports, three offline reads and ten refusals. Both delivery
forms admit 28 members, and the packaged sources configure offline.
The build retry used the matrix-specific ABI constant. The validation retry
matched the strict option parser's member-limit diagnostic. The staging
retry gave continuous materials their own inventory IDs.
Continue with the next demonstrated eligible workflow.
[Writer baseline](../build/npz-continuous/baseline.json)
[Validation](../build/npz-continuous/packaged-validation/validation.json)
[Package provenance](../build/npz-continuous/package-1.0.0-verified/SOURCE.json)

## Delivered locally - import NumPy counts and explicit axis IDs

The native NPZ provider imports the retained big-endian Fortran-order
UInt64 matrix through the existing position join and QMX writer. The
relocated package passed three imports, three exact offline SDK reads and
ten refusals. The offline checks recovered reordered feature queries,
counts through UInt64 max, exact UTF-8 IDs and positions after moving the
fixture inputs aside. Both delivery forms admit 24 members. Sources
configured offline against the matching format kit. The baseline retry
used a native-readable PPM to isolate the missing provider from image intake.
Continue with the next demonstrated eligible workflow.
[Writer baseline](../build/npz-matrix/baseline.json)
[Validation](../build/npz-matrix/packaged-validation/validation.json)
[Package provenance](../build/npz-matrix/package-1.0.0/SOURCE.json)
[NumPy archive format](https://numpy.org/doc/stable/reference/generated/numpy.savez.html)

## Delivered locally - open an explicitly selected animated WebP frame

The native 1.0.0 animation provider opens the retained Pillow file and
the selected frame reopens under a different catalogue default. The
relocated package passed seven exact raw and seven display comparisons,
11 refusals and two project checks. Authored controls cover partial-frame
linear sRGB blending, background fill/disposal and ALPH/VP8 reconstruction.
Both WebP providers read their profiles from one catalogue. The 90-member
stage and ZIP reuse admitted libwebp sources and notices, and the staged
sources configure offline. The package validation retry used a fresh output
path after the first command collided with the installed validation path.
Continue with the next demonstrated eligible workflow.
[Still-reader refusal](../build/webp-animation/baseline.json)
[Validation](../build/webp-animation/packaged-validation/validation.json)
[Package provenance](../build/webp-animation/package-1.0.0/SOURCE.json)
[Reader coexistence](../build/webp-animation/coexistence/result.json)
[WebP animation format](https://developers.google.com/speed/webp/docs/riff_container#animation)

## Delivered locally - open an explicitly selected APNG frame

The native 1.0.0 APNG plugin opens the retained Pillow animation and saves
the frame index in the project reader identity. The relocated package passed
eight exact raw and eight display comparisons, 11 refusals and two project
checks. Authored controls cover split chunks, a separate fallback image,
SOURCE and linear sRGB OVER, and both frame disposal modes. The 88-member
stage and ZIP reuse the admitted libpng and zlib source and notice records.
The build retry reused codec objects and removed a shadowing warning. The
staging retry deduplicated parent records shared by source and notice paths.
The staged sources configured offline. Continue with the next demonstrated gap.
[Static-reader refusal](../build/apng-intake/baseline.json)
[Validation](../build/apng-intake/packaged-validation/validation.json)
[Package provenance](../build/apng-intake/package-1.0.0-final/SOURCE.json)
[PNG animation specification](https://www.w3.org/TR/png-3/#apng-frame-based-animation)

## Delivered locally - open lossless tiled FITS volumes

FITS 1.9.0 opens the retained 27-tile Astropy Float32 XYZ volume. The
relocated package passed 65 exact ABI fixture profiles, 108 refusal cases
and 53 native checks. The evidence includes every Z plane, partial tiles
on all axes, three CLI Z crops, GZIP_1 and RICE_1 volume controls, a singleton
Z and fresh-process project replay. The cached build reused zlib without
a retry. The stage and ZIP admit 26 members. Continue with the next
demonstrated eligible workflow.
[Writer baseline](../build/fits-reader/tile-volume-baseline/baseline.json)
[Validation](../build/fits-reader/tile-volume-baseline/packaged-validation/validation.json)
[Package provenance](../build/fits-reader/tile-volume-baseline/package-1.9.0/SOURCE.json)

## Delivered locally - preserve lossless floating FITS tiles

FITS 1.8.0 opens the retained Astropy GZIP_2 Float32 image with exact
signed zero and subnormal samples. The relocated package passed 61 exact
ABI profiles, 101 refusals and 48 native checks. Float64 controls verify
both GZIP codecs. The selected-HDU replay and raw crop preserve exact
samples. The build reused zlib without a retry. Both delivery forms admit
26 members with the same pinned source and notice.
[Writer baseline](../build/fits-reader/tile-gzip2-baseline/baseline.json)
[Validation](../build/fits-reader/tile-gzip2-baseline/packaged-validation/validation.json)
[Package provenance](../build/fits-reader/tile-gzip2-baseline/package-1.8.0/SOURCE.json)

## Delivered locally - open the default Astropy FITS tile compression

FITS 1.7.0 opens the retained default RICE_1 Int16 output, using the
existing tile grid, heap descriptors and scalar raster. The relocated
package passed 56 exact ABI profiles, 94 refusal cases and 45 native checks,
including partial tiles and fresh-process HDU replay. The stage and ZIP
admit 26 exact members. The build reused zlib. The final validation added
a nondefault seven-sample block control and rebuilt only the ABI probe.
[Default writer baseline](../build/fits-reader/tile-rice-baseline/baseline.json)
[Validation](../build/fits-reader/tile-rice-baseline/packaged-validation/validation.json)
[Package provenance](../build/fits-reader/tile-rice-baseline/package-1.7.0/SOURCE.json)

## Delivered locally - open GZIP_1 tile-compressed FITS images

FITS 1.6.0 opens the retained Astropy Int16 extension with nine tiles and
partial right and bottom edges. The relocated plugin passed 50 exact ABI
profiles, 87 refusal cases and 42 native checks, including a crop across
tile boundaries and fresh-process reopen under a different HDU default.
The stage and ZIP admit 25 exact members. The module reused cached zlib.
Harness corrections accounted for upstream gzip timestamps, specified tile
sizes in the wide-descriptor oracle, and updated the fixed fixture counts.
The native checks retained all earlier assertions.
[Writer baseline](../build/fits-reader/tile-gzip-baseline/baseline.json)
[Validation](../build/fits-reader/tile-gzip-baseline/packaged-validation/validation.json)
[Package provenance](../build/fits-reader/tile-gzip-baseline/package-1.6.0/SOURCE.json)
[FITS tiled image convention](https://fits.gsfc.nasa.gov/registry/tilecompression.html)

## Delivered locally - retain associated OpenEXR alpha

OpenEXR 1.1.0 opens the retained ZIP RGBA still and preserves all four
source channels, including emission at zero alpha. The preview unassociates
positive-alpha values and shows transparent black at zero alpha. The
relocated stage passed ten exact raw exports, ten RGBA comparisons, 17
refusals and four fresh-process project checks. The build reused zlib.
The harness retries isolated the upstream writer's dictionary mutation and
incorrect handling of strided channel input. Contiguous channel snapshots
matched the authored values in both the upstream reader and the plugin.
The stage and ZIP admit 20 members. Continue with the next demonstrated gap.
[RGBA baseline](../build/openexr-reader/alpha-baseline/baseline.json)
[Validation](../build/openexr-reader/packaged-validation-1.1.0/validation.json)
[Package provenance](../build/openexr-reader/package-1.1.0/SOURCE.json)
[OpenEXR alpha convention](https://openexr.com/en/latest/TechnicalIntroduction.html#premultiplied-vs-un-premultiplied-color-channels)

## Delivered locally - open linear OpenEXR scanline data

The native 1.0.0 plugin opens the retained OpenEXR 3.5.4 writer file with
exact signed Float32 RGB, including a crop across the ZIP block boundary.
The relocated stage passed seven exact raw exports, seven RGBA comparisons,
15 refusals and two fresh-process project checks. Controls cover NONE, ZIPS,
ZIP, verbatim fallback, decreasing scanline order and lossless HALF widening.
The stage and ZIP admit 20 members with the pinned zlib source and notice.
The independent plugin and SDK probe built without a retry. `uv` replaced
the unavailable `pip` module for the isolated development oracle. A second
validation pass added an explicitly compressed ZIPS control without rebuilding.
Continue with the next demonstrated eligible gap.
[Writer baseline](../build/openexr-reader/baseline.json)
[Validation](../build/openexr-reader/packaged-validation/validation.json)
[Package provenance](../build/openexr-reader/package-1.0.0/SOURCE.json)
[OpenEXR format reference](https://openexr.com/en/latest/TechnicalIntroduction.html)

## Delivered locally - read linear RGBE exposure sweeps

The retained CLI fell through to TIFF for the imagecodecs 2025.8.2 RGBE
exposure sweep. The optional native 1.0.0 plugin preserves exact linear
Float32 bin centers for canonical top-left, flat and scanline-RLE files.
The relocated Windows package passed five exact raw exports, five display
comparisons, 18 refusals and two fresh-process project checks. The writer
fixture reproduced byte-for-byte. The independent module and probe built
without a retry. The test retry fixed CLI argument order for the offset
preview. The stage and ZIP admit 17 members with no downstream decoder.
Continue with the next demonstrated eligible gap.
[Writer baseline](../build/rgbe-reader/baseline.json)
[Validation](../build/rgbe-reader/packaged-validation/validation.json)
[Package provenance](../build/rgbe-reader/package-1.0.0/SOURCE.json)
[Radiance format reference](https://radsite.lbl.gov/radiance/refer/Notes/picture_format.html)

## Delivered locally - open native SDR AVIF stills

The retained CLI had no AVIF provider for a Pillow 4:2:0 sRGB still.
The optional 1.0.0 module opens it with `allow_yuv` and independently
preserves exact identity-coded samples. The relocated package passed seven
exact raw exports, seven RGBA comparisons, eight refusals and four native
project checks. The stage and ZIP admit 23 members with the original notices.
Both selected decoder-source archives reproduced byte-for-byte. The cached
build reused libaom objects after publishing its header path to libavif.
The test retry matched the truncated payload's decode-stage diagnostic.
Continue with the next demonstrated eligible gap.
[Writer baseline](../build/avif-reader/baseline.json)
[Validation](../build/avif-reader/packaged-validation/validation.json)
[Package provenance](../build/avif-reader/package-1.0.0/SOURCE.json)

## Delivered locally - decode default lossy JPEG XL stills with opt-in

The retained JPEG XL 1.2.0 provider refuses default lossy cjxl output.
Version 1.3.0 admits the structured SDR sRGB XYB profile when `allow_xyb`
is true. Native metadata identifies reconstructed sRGB. The relocated
package passed 37 raw exports, 37 RGBA comparisons, 21 refusals and ten
native project checks, including decoded-sRGB reopen with an empty catalog
default. The separately compiled GIF and VTI controls retained exact
samples and their option contracts. The plugin build reused codec objects.
The test retry corrected the expected invalid-option diagnostic wording.
The stage and ZIP admit 28 members with the pinned sources and notices.
Continue with the next demonstrated gap.
[Default lossy baseline](../build/jpegxl-reader/xyb-baseline/baseline.json)
[Validation](../build/jpegxl-reader/packaged-validation-1.3.0/validation.json)
[Shared adapter regression](../build/jpegxl-reader/options-regression/results.json)
[Package provenance](../build/jpegxl-reader/package-1.3.0/SOURCE.json)

## Delivered locally - normalize JPEG XL codestream orientation

Retained JPEG XL 1.1.0 refuses a 90-degree codestream produced by cjxl 0.13.0.
The 1.2.0 plugin uses the pinned decoder's coordinate normalization for the
common raw and display raster. All eight orientations and a 16-bit RGBA
reflection match the explicit transforms and upstream decoder. The relocated
package passed 33 exact raw exports, 33 RGBA comparisons, 18 refusals, eight
native project checks and two direct fragment checks. A fresh process verified
the rotated project dimensions and the same raw and display crop. The plugin
reused the codec objects. The official development archive uses Deflate64,
so 7-Zip replaced the failed Windows tar extraction. The stage and ZIP admit
28 members with the original codec sources and notices. Continue with the next
demonstrated gap.
[Oriented writer refusal](../build/jpegxl-reader/orientation-baseline/baseline.json)
[Validation](../build/jpegxl-reader/packaged-validation-1.2.0/validation.json)
[Package provenance](../build/jpegxl-reader/package-1.2.0/SOURCE.json)

## Delivered locally - open fragmented JPEG XL containers

The retained JPEG XL 1.0.0 reader refuses a three-part container. The
1.1.0 plugin assembles ordered and reordered `jxlp` inputs before decoding
and checks indices, final marker, exact extents and the work budget. The
relocated stage passed 23 exact raw exports, 23 RGBA comparisons, 18
refusals, six native project checks and two direct libjxl container checks.
The upstream 0.13.0 reference reproduced every sample of the ordered and
reordered fixtures. The independent build needed no retry and reused the
pinned codec objects. The stage and ZIP admit 28 members with exact source
archives and notices. Continue with the next demonstrated gap.
[Fragment baseline](../build/jpegxl-reader/fragment-baseline/baseline.json)
[Validation](../build/jpegxl-reader/packaged-validation-1.1.0/validation.json)
[Package provenance](../build/jpegxl-reader/package-1.1.0/SOURCE.json)

## Delivered locally - open integer JPEG XL stills

Retained PNG correctly refuses the Pillow two-frame APNG. The native CLI
had no JPEG XL provider for a lossless RGB fixture. JPEG XL 1.0.0 opens
that codestream and the selected integer stills through the pinned
libjxl 0.13.0 decoder. The relocated package passed 19 raw exports, 19
RGBA comparisons, seven refusals and two native project checks. The upstream
archive's unused benchmark symlinks are excluded from Windows extraction.
The external writer uses linear transfer on UInt16 inputs and emits a box
for its 16-bit profile. The native reader preserves their raw codes and uses
an explicit display transfer and bounded container parser. The stage and
ZIP admit 27 members with exact source archives and notices. Continue with
the next demonstrated gap.
[JPEG XL baseline](../build/jpegxl-reader/baseline.json)
[APNG refusal](../build/apng-intake/baseline.json)
[Validation](../build/jpegxl-reader/packaged-validation/validation.json)
[Package provenance](../build/jpegxl-reader/package-1.0.0/SOURCE.json)

## Delivered locally - read static indexed GIF

The retained native CLI cannot open a Pillow-authored full-canvas GIF.
GIF 1.0.0 reads the original fixture with exact indices and RGBA, including
the transparent palette entry. The relocated module passed 12 raw exports,
12 RGBA comparisons, 18 refusals and two native project checks. Authored
local palettes, interlace, LZW width growth and deferred clear agree with
Pillow. A fresh process reopened the saved source with exact indices and
display. The build needed no retry. The stage and ZIP admit 17 members
with corresponding sources and notices. Continue with the next demonstrated gap.
[Writer baseline](../build/gif-reader/baseline.json)
[Validation](../build/gif-reader/packaged-validation/validation.json)
[Package provenance](../build/gif-reader/package-1.0.0/SOURCE.json)

## Delivered locally - assemble streamed VTK ImageData pieces

The retained VTI 1.2.0 refuses four VTK 9.4.2 two-slab files with a shared
Z plane. VTI 1.3.0 reproduced all five planes of each file and preserved exact
samples on a shared X column. It rejects conflicting overlap, incomplete
coverage and invalid appended offsets. The relocated module passed 56 raw
exports, 47 refusals, two display checks and eight native project checks,
including fresh-process multi-piece replay with retained calibration.
The producer script reproduced the five inputs byte-for-byte. The independent
build needed no retry. Stage and ZIP admit 23 members with existing source
archives and notices. Continue with the next demonstrated gap.
[Streamed writer baseline](../build/vti-reader/piece-workflow/baseline.json)
[Validation](../build/vti-reader/packaged-validation-1.3.0/validation.json)
[Package provenance](../build/vti-reader/package-1.3.0/SOURCE.json)

## Delivered locally - read VTK raw-appended scalar data

The retained VTI 1.1.0 rejects raw-appended VTK 9.4.2 output with and
without zlib. VTI 1.2.0 separates the bounded binary body from XML and
reuses the endian, extent and inflater paths. The relocated package
passed 34 exact raw exports, 38 refusals, two display checks and six
fresh-process project checks. The parser separates the exact writer footer
before decoding so it cannot supply missing sample bytes. Embedded XML-like
bytes, LF/CRLF framing and the original refused files were verified.
Only the independent plugin rebuilt, without a retry. Both staged forms
admit 23 members with the same pinned sources and notices.
Continue with the next demonstrated gap.
[Raw-appended writer refusal](../build/vti-reader/raw-appended-baseline/baseline.json)
[Validation](../build/vti-reader/packaged-validation-1.2.0/validation.json)
[Package provenance](../build/vti-reader/package-1.2.0/SOURCE.json)

## Delivered locally - read the default compressed VTK ImageData output

The staged VTI 1.0.0 plugin refuses default VTK 9.4.2 output. VTI 1.1.0 opens
that file and validates independently framed zlib blocks in inline and
appended base64. The relocated package passed 26 exact raw exports, 28
refusals, two display checks and four fresh-process project checks. An
independently built ICS regression matches the retained three-channel gzip
export byte-for-byte and refuses a bad CRC. The VTI build needed no retry.
The partial-block fixture now uses 16 bytes because VTK rounds block sizes
to multiples of eight. The ICS check uses all three reference channels and
the actual malformed-member diagnostic. The stage and ZIP admit 23 members
with both corresponding source archives. Continue with the next demonstrated gap.
[Default writer refusal](../build/vti-reader/default-compressed/baseline.json)
[Validation](../build/vti-reader/packaged-validation-1.1.0/validation.json)
[ICS regression](../build/vti-reader/ics-regression/results.json)
[Package provenance](../build/vti-reader/package-1.1.0/SOURCE.json)

## Delivered locally - read scalar VTK ImageData

The retained CLI had no VTI route. The optional 1.0.0 plugin reads the exact
VTK 9.4.2 volume in ASCII, inline base64 and appended base64 with both
byte orders. The relocated package passed 19 raw exports, 18 refusals,
two display checks and two native project checks. Reopen retains the explicit
millimetre setting against an empty catalog default. The shared packed-reader
change passed 29 TGA profiles and 88 native CLI checks. A shadowed variable
warning was corrected before staging. Both package forms admit 20 members,
including the pinned tinyxml2 archive and notice. The next demonstrated gap
is the default VTK writer's zlib-compressed appended output, refused by the
staged 1.0.0 module.
[Baseline](../build/vti-reader/baseline.json)
[Validation](../build/vti-reader/packaged-validation/validation.json)
[Package provenance](../build/vti-reader/package-1.0.0/SOURCE.json)
[Default writer refusal](../build/vti-reader/default-compressed/baseline.json)

## Delivered locally - open compressed scalar MetaImage

MetaImage 1.1.0 refuses uncompressed-compatible geometry written with zlib
by SimpleITK 2.5.3 in both MHA and MHD. The 1.2.0 native plugin reads those
files and checks the zlib checksum, exact stream and raster extents. The
relocated package passed 29 exact raw exports, four display checks, 28
refusals and six SDK checks. Fresh processes reopened both detached layouts
and refused byte-identical payload replacements. The fixture generator
reproduced all six compressed producer files byte-for-byte. The independent
build needed no retry. The stage and ZIP admit 20 members with the admitted
zlib 1.3.2 archive and notice. Continue with the next demonstrated gap.
[Compressed producer baseline](../build/metaimage-reader/simpleitk-compressed/baseline.json)
[Validation](../build/metaimage-reader/packaged-validation-1.2.0/validation.json)
[Package provenance](../build/metaimage-reader/package-1.2.0/SOURCE.json)

## Delivered locally - open identity-geometry files written by SimpleITK

Retained MetaImage 1.0.0 refuses four uncompressed identity-geometry UInt16
XY/XYZ MHA/MHD files emitted by SimpleITK 2.5.3 because of its default
orientation tags. MetaImage 1.1.0 admits the matching tags only with explicit
identity geometry. The producer script regenerates identical input bytes.
The relocated package passed all 23 raw exports, four display checks,
17 refusals and three fresh-process SDK checks. Only the provider rebuilt,
with no retry. The stage and ZIP admit 18 members. Continue with the next
demonstrated gap.
[Producer baseline](../build/metaimage-reader/simpleitk-baseline/baseline.json)
[Validation](../build/metaimage-reader/packaged-validation-1.1.0/validation.json)
[Package provenance](../build/metaimage-reader/package-1.1.0/SOURCE.json)

## Delivered locally - read scalar MetaImage MHA and MHD

The retained host refused an attached scalar UInt16 volume without a MetaImage
provider. The 1.0.0 optional ABI1 plugin passed 19 exact raw exports, four RGBA
checks, 15 refusals and three fresh-process SDK checks for uncompressed XY/XYZ
with identity geometry. It reopened a detached project with exact samples and
calibration and refused a byte-identical replacement of its payload. The stage
and ZIP admit 17 members. The relocated stage passed the same suite.
The plugin built on the first attempt. The probe
needed public SDK includes and the harness needed the Windows uppercase
environment key. Continue with the next demonstrated catalogue gap.
[Baseline](../build/metaimage-reader/baseline.json)
[Validation](../build/metaimage-reader/validation-02/validation.json)
[Package provenance](../build/metaimage-reader/package-1.0.0/SOURCE.json)

## Delivered locally - read packed MRC2014 mode 101 samples

The retained 1.4.0 plugin refuses the even-column mode 101 stack fixture.
[Baseline](../build/mrc-reader/packed4-candidate/baseline.json)

The 1.5.0 plugin exposes exact UInt8 codes from packed mode 101 images,
volumes and selected stacks. Both header byte orders, axis permutation,
gzip and bzip2, display, source identity and fresh-process project reopen
passed. The packaged module reads the original fixture's final series
exactly. The extension passed 94 valid profiles, 54 refusals and 31 native
checks. The build needed no retry. Both package forms admit 27 members
with retained codec sources and notices. Continue with a demonstrated gap.
[Validation](../build/mrc-reader/packed4-candidate/validation/validation.json)
[Package admission](../build/mrc-reader/packed4-candidate/package-1.5.0-admission.json)

## Delivered locally - select members of MRC2014 scalar stacks

The retained 1.3.0 plugin refuses both image and volume stacks generated by
the CCP-EM development oracle.
[Baseline](../build/mrc-reader/stacks-candidate/baseline.json)

The 1.4.0 plugin exposes each XY section or P1 volume as its own selectable
series. Exact raw and display samples, calibration and bounds passed for
plain, gzip and bzip2 inputs. A fresh process reopened the saved nonzero
series with its captured window despite different catalog defaults. The
oracle baseline uses the explicit image-stack setter. The product build
needed no retry. All 90 profiles, 50 malformed files and 27 native checks
passed. The stage and ZIP admit 27 members with the retained codec notices.
Continue with the next demonstrated catalogue gap.
[Validation](../build/mrc-reader/stacks-candidate/validation/validation.json)
[Package admission](../build/mrc-reader/stacks-candidate/package-1.4.0-admission.json)

## Delivered locally - open NRRD0003 and NRRD0004 scalar images

The retained 1.12.0 plugin refuses explicit scalar XYZT version 3 and
calibrated XYZ version 4 files at the magic.
[Baseline](../build/nrrd-reader/legacy-version-candidate/baseline.json)

The 1.13.0 plugin reads both baseline files with exact samples and calibration.
Version 3 single-file and version 4 LIST exports agree at Z1/T2. The version 4
SDK project reopens in a fresh process with exact data, calibration and source
identities. The corpus totals 166 valid profiles, 175 refusals and 64 native
checks. The build needed no retry. Both package forms admit 24 members with
the retained codec archives and notices. Continue with a demonstrated gap.
[Validation](../build/nrrd-reader/legacy-version-candidate/validation/validation.json)
[Package admission](../build/nrrd-reader/legacy-version-candidate/package-1.13.0-admission.json)

## Delivered locally - read axis-aligned NRRD space directions

NRRD 1.11.0 refuses a scalar volume emitted by pynrrd with a positive
diagonal, calibrated basis and zero origin.
[Baseline](../build/nrrd-reader/geometry-candidate/baseline.json)

The 1.12.0 plugin maps this basis to native calibration and preserves raw
sample order. Exact replay of the original producer fixture, the detached
XYZT SDK project, and selected CLI export passed. The retained and new corpus
totals 162 valid profiles, 168 refusals and 63 native checks. The independent
plugin build needed no retry. Both package forms admit 24 members with the
same codec sources and notices. Continue with the next demonstrated gap.
[Validation](../build/nrrd-reader/geometry-candidate/validation/validation.json)
[Package admission](../build/nrrd-reader/geometry-candidate/package-1.12.0-admission.json)

## Delivered locally - read scalar NRRD time series

The retained NRRD 1.10.0 reader refuses a calibrated XYZT volume.
[Baseline](../build/nrrd-reader/time-series-candidate/baseline.json)

The 1.11.0 plugin accepts explicit final time axes in XYT and XYZT. Exact Z/T
samples, default display, time calibration, detached identities, fresh-process
project reopen and NPY export pass. The retained corpus and added cases total
159 valid profiles, 159 refusals and 63 native checks. The initial validation
used the wrong overflow enum. The corrected expectation passed on the same
module. The Windows package and ZIP both admit 24 exact members with the
retained codec sources and notices. Continue with a demonstrated catalogue gap.
[Validation](../build/nrrd-reader/time-series-candidate/validation-corrected/validation.json)
[Package admission](../build/nrrd-reader/time-series-candidate/package-1.11.0-admission.json)

## Delivered locally - preserve conflicting sidecars on project reopen

The retained SDK reopens a legacy project far enough for SQLite to delete
its linked PPM at the journal path.
[Baseline](../build/project-reopen-journal/baseline.log)

The writable recovery fallback now checks the native database and rollback
journal signatures. The relocated SDK probe preserves the linked image and
a foreign database with a hot journal, accepts a non-hot zero journal
read-only, and recovers a genuine interrupted project with the previous
generation and exact pixels. SQLite integrity and both native suites pass.
The Python-free build needed no retry. Continue with a demonstrated gap.
[Handoff](../build/project-reopen-journal/results.json)

## Delivered locally - protect linked inputs during project saves

The baseline ordinary save wrote inside the live store and replaced the
missing OME-Zarr store root. The SDK probe also showed SQLite deleting a
linked PPM at the rollback journal path.
[Store baseline](../build/project-save-source-protection/baseline.log)
[Journal baseline](../build/project-save-source-protection/baseline-sidecar.log)

Both save routes now check database, journal, WAL and SHM names through
shared native input protection. The first fixed build staged Save As safely
but created a project SQLite refused to reopen, so the final guard now also
covers the published sidecar names. Online and offline store collision tests,
safe Save As with exact image bytes, owned matrix and recovery suites pass.
Continue with the next demonstrated catalogue gap.
[Handoff](../build/project-save-source-protection/results.json)

## Delivered locally - index AnnData Zarr with a read-only project template

The retained SDK consumer writes a managed QMX into the template's linked
OME-Zarr store and accepts an explicit indexed output inside that store.
[Baseline](../build/anndata-project-template/baseline.json)

The native index service checks both destinations and the example reuses
read-only attachment. The isolated Windows SDK consumer preserves both
input stores and the project, refuses three colliding outputs, and reopens
the exact owned QMX offline. An occupied managed file preserves the completed
standalone index, which the relocated CLI attaches offline. Both native
suites passed. The existing managed directory needed a directory containment
check, and the verifier needed to close its SQLite handle and stage the CLI
separately. Continue with the next demonstrated catalogue gap.
[Installed consumer and CLI handoff](../build/anndata-project-template/results.json)

## Delivered locally - import annotations and matrices against directory-backed images

The relocated CLI refuses both point imports onto linked OME-Zarr project
images and matrix indexing against a local OME-Zarr image.
[Baseline](../build/format-directory-targets/baseline.json)

The shared guard verifies the reader's observed directory and protects it
through publication. The relocated CLI persists exact points, ordinary TSV
and AnnData Zarr counts, and attaches a byte-identical QMX in the same frame.
All three in-store destinations refuse and inputs match their original hashes.
Four native cases pass, including unchanged document state after stale preview
and no QMX after a final-publication change. The cached build needed a missing
header include. The CLI verifier now decodes the implicit default plane.
Continue with the next demonstrated integration gap.
[CLI handoff](../build/format-directory-targets/cli-results.json)
[Native stale-directory evidence](../build/format-directory-targets/native-directory-target.log)

## Delivered locally - publish backed spatial import from a read-only project template

The unchanged product refuses an external project when the template's
managed matrix directory is a linked OME-Zarr store.
[Baseline](../build/backed-import-readonly/baseline.log)

Native Save As now materializes the exact QMX beside the destination project.
New ownership, linked-to-owned conversion and repair of a missing owned
matrix all pass. The protected destination refuses, the input projects and
store stay unchanged, and registered counts replay exactly. Both native
suites and the relocated matrix attachment CLI passed, including duplicate
refusal. The Python-free product build needed no retry. Continue with the
next demonstrated catalogue gap.
[Handoff](../build/backed-import-readonly/results.json)

## Delivered locally - attach a completed QMX from a read-only project template

The retained CLI refuses a valid retry into an external destination because
it first tries to copy the QMX into the template's linked `.qpx-expression`
store. The selected image still resolves to its original frame.
[Baseline](../build/matrix-attach-readonly/baseline.json)

The retry keeps the association pending in the temporary native project.
Guarded Save As copies the QMX beside the new project and serializes the
existing canonical association. Online counts and offline continuous
retries reopen byte-identical owned QMX files. The protected destination
refuses and template hashes match. Matrix and backed import suites passed,
including the new native offline regression. The product build needed
no retry. Continue with an independently demonstrated integration gap.
[Handoff](../build/matrix-attach-readonly/results.json)

## Delivered locally - protect linked stores during owned matrix retention

The retained native SDK writes an owned QMX into a linked OME-Zarr store
when that store is the saved project's `.qpx-expression` directory.
[Baseline](../build/owned-matrix-input-guard/baseline.json)

Attachment, ownership conversion and relink use the shared guard before
writing and before publication. Four native refusal cases preserve project
revisions and create no managed copy. The backed import suite passes, and
both relocated CLI retries reopen exact counts and continuous QMX files.
The first native fixture reused a source moved earlier, so the test now
opens its current owned copy. The CLI retry uses the original template
location because moving the relative image changes the stored frame.
The product build needed no retry. Continue with a demonstrated workflow gap.
[Handoff](../build/owned-matrix-input-guard/results.json)

## Delivered locally - protect linked stores during managed Save As copies

The retained CLI saves a project and copies its owned QMX when the new
project's `.qpx-expression` directory is itself a linked OME-Zarr store.
[Baseline](../build/expression-save-input-guard/managed-matrix-followup/results.json)

Native Save As now checks actual new QMX and QPR copy paths before writing
and at publication. Reduction preflights the planned copies before fitting.
The relocated CLI rejects both online and offline collisions before staging
its archive. All three copied external assets and the fitted record match.
Four native suites pass. The matrix suite hit a pre-existing deep path in
its batch fixture, then passed from a short working directory with the same
binary. The cached product build needed no retry. Continue with the next
demonstrated native integration blocker.
[Handoff](../build/managed-project-copy-guard/results.json)

## Delivered locally - protect linked stores during reduction archive publication

The retained CLI refuses a new project inside a linked OME-Zarr store after
writing a fitted archive there. It also succeeds when the project destination is
outside but its managed reduction directory is itself a linked store.
[Baseline](../build/expression-save-input-guard/baseline.json)

The shared destination guard now checks the new project and exact fitted
archive path before writing and again at publication. The relocated CLI
refuses all four online/offline cases. Both external fitted archives match
the retained bytes and the new project replays the exact record. Three
native suites passed, including removal of a staged archive on final guard
refusal. The cached build needed no retry.
[Handoff](../build/expression-save-input-guard/results.json)

The next blocker is the shared Save As copy of an owned QMX when the
destination's `.qpx-expression` folder is itself a linked store. The
current CLI reproduces that separate write on a copied fixture.
[Follow-up baseline](../build/expression-save-input-guard/managed-matrix-followup/results.json)

## Delivered locally - protect linked stores during expression statistics export

The retained native CLI writes sums, membership, gene and replicate CSV
inside an unselected linked OME-Zarr store and at its offline location.
[Baseline](../build/expression-statistics-export-guard/baseline.json)

The three native expression workflows now share saved-project destination
checks before temporary output and at publication. The relocated CLI
refused ten protected destinations and produced all four external CSVs
byte for byte. The project and store hashes match. Three native suites
passed, including cleanup after a publication refusal. The reduction CLI
also refused its linked destination and reproduced its external CSV.
The first fixture edit used the shell encoding and overlapped the build.
The corrected UTF-8 edit and focused relink passed. Continue with the next
demonstrated native integration blocker.
[Handoff](../build/expression-statistics-export-guard/results.json)

## Delivered locally - protect linked stores during expression reduction export

The retained native CLI writes embedding CSV, loadings CSV and scatter SVG
into an unselected image's linked OME-Zarr store. External controls export
normally. The baseline used a copy of the store and a saved native result.
[Baseline](../build/expression-reduction-export-guard/baseline.json)

The native CLI uses the shared guard before opening its result or writing a
temporary output, and again at publication. The relocated CLI refuses all
six online/offline store destinations and the missing SQLite WAL path.
All three external exports match retained output bytes, and input hashes
match. The two focused native suites pass, including temporary-file cleanup
when the final guard refuses. The cached build needed no retry.
Next, check sibling saved expression export routes for the same failure.
[Handoff](../build/expression-reduction-export-guard/results.json)

## Delivered locally - open JPEG 2000 with multiple tile parts

OpenJPEG 2.5.4 produces a tiled JP2 in which every tile has three parts
split by quality layer. Its complete decode matches the source. The
retained 1.12.0 reader refuses the first multipart tile.
[Baseline](../build/jpeg2000-reader/tile-parts-candidate/baseline/results.json)

Version 1.13.0 validates explicit counts and consecutive indices per tile.
The decoder preserves packet iteration across interleaved parts and requires
all packets at each final boundary. The original samples match OpenJPEG
exactly and the calibrated application crop reopens with identical bytes.
The relocated and offline modules each passed 70 profiles and 281 CLI
checks. A separate multipart SOP/EPH case matches its independent reference.
All 112 members passed admission and the cached build needed no retry.
Continue with the next demonstrated native integration blocker.
[Handoff](../build/jpeg2000-reader/tile-parts-candidate/results.json) ·
[Multipart SOP/EPH](../build/jpeg2000-reader/tile-parts-candidate/multipart-sop/results.json)

## Delivered locally - open JPEG 2000 with packet-length tables

OpenJPEG 2.5.4 reads an authored tiled RPCL JP2 with PLT markers exactly.
The retained 1.11.0 provider refuses the tile header at its first table.
[Baseline](../build/jpeg2000-reader/packet-table-candidate/baseline/results.json)

Version 1.12.0 checks sequential PLT lengths against the complete tile
body. Actual packet boundaries come from the strict decoder. The original
producer case and a split-table control export exact samples. A calibrated
three-layer JP2 with SOP, EPH and PLT reopens exactly. The relocated and
offline modules each passed 68 profiles and 273 CLI checks. All 112
members passed admission. The cached build needed no retry. Continue
with a demonstrated format or integration blocker.
[Handoff](../build/jpeg2000-reader/packet-table-candidate/results.json)

## Delivered locally - open JPEG 2000 with SOP and EPH packet markers

OpenJPEG 2.5.4 writes a tiled three-layer RPCL JP2 with SOP and EPH and
decodes its original pixels exactly. Version 1.10.0 refuses the COD flags.
The first producer probe exceeded OpenJPEG's output buffer with small
precincts and both markers. Default precincts produce the baseline.
[Baseline](../build/jpeg2000-reader/packet-markers-candidate/baseline/results.json)

Version 1.11.0 validates marker flags and tile extents, and reuses JasPer's
SOP sequence and EPH header checks. The original fixture exports exact
reference samples. A calibrated three-layer RPCL project reopens exactly.
The relocated and offline modules each passed 66 profiles and 262 CLI
checks. A separate fixture with custom precincts and both markers
produces identical bytes in both modules. All 112 members passed admission.
The cached build required no retry. Continue with a demonstrated format
or integration blocker.
[Handoff](../build/jpeg2000-reader/packet-markers-candidate/results.json) ·
[Combined marker and precinct check](../build/jpeg2000-reader/packet-markers-candidate/combined-marker-precinct/results.json)

## Delivered locally - open JPEG 2000 with explicit precincts

OpenJPEG 2.5.4 writes and exactly reads a tiled, three-layer RPCL JP2 with
rectangular precincts that change by resolution. The retained 1.9.0
plugin refuses its COD precinct declaration.
[Baseline](../build/jpeg2000-reader/precinct-candidate/baseline/results.json)

Version 1.10.0 checks explicit precinct dimensions and counts the implied
packets before decode. The original fixture exports exact reference bytes.
A calibrated irreversible crop spanning precincts and odd tile boundaries
reopens exactly. The relocated and offline modules each passed 63 profiles
and 248 CLI checks, including excessive and malformed grids. The 112-member
archive passed admission. The cached build required no retry. Continue
with a demonstrated format or integration blocker.
[Handoff](../build/jpeg2000-reader/precinct-candidate/results.json)

## Delivered locally - open alternate JPEG 2000 packet orders

OpenJPEG 2.5.4 writes a tiled three-layer RPCL JP2 and recovers its
source samples exactly. The retained 1.8.0 plugin refuses its COD order.
[Baseline](../build/jpeg2000-reader/progression-candidate/baseline/results.json)

Version 1.9.0 reads all five Part 1 packet orders with default precincts.
The original RPCL case now matches the full OpenJPEG reference exactly.
An irreversible RPCL crop reopens with exact samples and calibration.
The relocated and offline modules each passed 58 profiles and 230 CLI
checks, including missing packets under every order. All 112 package
members passed admission. The cached build needed no retry. Continue with
the next demonstrated format or integration blocker.
[Handoff](../build/jpeg2000-reader/progression-candidate/results.json)

## Delivered locally - decode all JPEG 2000 quality layers

An OpenJPEG three-layer tiled JP2 returns the original pixels after all
layers. Its first layer alone differs. The retained 1.7.0 plugin refuses
the COD layer count.
[Baseline](../build/jpeg2000-reader/quality-layers-candidate/baseline/results.json)

Version 1.8.0 decodes 1 to 16 LRCP layers. The first build accepted an
extra declared layer with missing packets. The private decoder now checks
its packet iterator at each tile boundary and refuses both malformed probes.
A Python fixture name collision was corrected without rebuilding.
[Probe](../build/jpeg2000-reader/quality-layers-candidate/baseline/unpatched-probe.json)

The relocated and offline suites each passed 54 profiles and 217 CLI checks.
The original case exports exact all-layer samples, distinct from the first
layer alone. A calibrated three-layer crop reopens exactly. All 112 package
members passed admission with the pinned JasPer source and private build
change. Continue with a demonstrated format or integration blocker.
[Handoff](../build/jpeg2000-reader/quality-layers-candidate/results.json)

## Delivered locally - open irreversible grayscale JPEG 2000

OpenJPEG 2.5.4 produces an 8-bit tiled grayscale JP2 with one quality layer,
irreversible wavelet and explicit scalar quantization. It reads the resulting
image, while the 1.6.0 provider refuses its COD transform.
[Baseline](../build/jpeg2000-reader/lossy-candidate/baseline/results.json)

Version 1.7.0 verifies the quantization structure and labels reconstructed
UInt8 values in reader metadata. Four fixtures differ from OpenJPEG by at
most one code. The calibrated crop reopens exactly through the native SDK.
The relocated and offline modules each passed 50 profiles and 205 CLI
checks. All 112 package members passed admission. The cached build needed
no retry. Continue with the next producer-backed format or integration gap.
[Handoff](../build/jpeg2000-reader/lossy-candidate/results.json)

## Delivered locally - preserve signed grayscale JPEG 2000 samples

OpenJPEG 2.5.4 round-trips signed 16-bit tiled JP2 into identical signed PGX.
The retained 1.5.0 provider refuses its SIZ declaration. The first PGX input
probe inferred a narrower precision, so the independent encoder uses signed
RAW with explicit depth and the decoder uses PGX to verify sample bytes.
[Baseline](../build/jpeg2000-reader/signed-candidate/baseline/results.json)

Version 1.6.0 keeps exact signed 8- to 16-bit grayscale codes and maps the
declared signed range to display. The retained baseline now exports exact
Int16 bytes. The relocated and offline builds each passed 46 producer
profiles and 187 CLI checks. Direct ABI checks covered signed padding and
calibrated project reopen preserved cross-tile samples. The archive and
relocated copy passed all 112 material checks.

The retained generator needed its original CMake executable for regeneration.
OpenJPEG RAW rejects non-byte-aligned depths, so the 9- and 12-bit probes
use centered FFmpeg coefficients and independent OpenJPEG PGX readback.
Continue with the next producer-backed format or integration gap.
[Handoff](../build/jpeg2000-reader/signed-candidate/results.json)

## Delivered locally - open bzip2-wrapped MRC volumes

The independent mrcfile 1.5.4 oracle opens the compressed mode 12 control
with identical samples and calibration. The retained MRC 1.2.0 CLI opens
the plain volume and refuses its bzip2 copy.
[Baseline](../build/mrc-reader/bzip2-candidate/baseline/results.json)

Version 1.3.0 uses the admitted libbzip2 small decoder, sizes storage from
the decoded header and verifies one complete stream before publication.
The relocated and offline modules each passed 85 profiles, 47 refusals
and 22 native checks. The original rejected case exports byte-identical
NPY output and a re-encode refuses saved-source currentness. The 27-member
archive includes both codec sources and notices and passed admission.
The baseline writer's read-only header assignment was replaced by exact
wrapping. The first compile exposed Windows min/max macros. Applying the
NRRD target definitions allowed the cached rebuild to pass.
Continue with the next producer-backed format or integration gap.
[Handoff](../build/mrc-reader/bzip2-candidate/results.json)

## Delivered locally - open gzip-wrapped MRC volumes

CCP-EM mrcfile 1.5.4 reads an authored gzip member with the same stored bytes
and calibration as its plain mode 12 source. The retained 1.1.0 CLI opens the
plain control and refuses the compressed file before reading its header.
[Baseline](../build/mrc-reader/gzip-candidate/baseline/results.json)

Version 1.2.0 reuses the bounded gzip decoder and preserves calibration,
sample bits and the original compressed-file identity. The relocated package
and offline module each passed 79 profiles, 38 refusals and 18 native checks.
The formerly rejected file exports the exact NPY bytes of the plain control.
The 25-member archive passed material admission with pinned zlib source and
notice. The cached build needed no retry. Evaluate another producer-backed
format gap before extending this profile.
[Handoff](../build/mrc-reader/gzip-candidate/results.json)

## Delivered locally - open binary16 MRC volumes

CCP-EM mrcfile reads an authored mode 12 volume exactly. The retained
provider rejects it while the equivalent Float32 control opens. The 1.1.0
reader expands binary16 at the raw boundary and keeps its two-byte storage
layout, endian handling and axis mapping.

The relocated package and offline module each passed 71 positive cases,
29 refusals and 14 native checks, including all 65,536 binary16 encodings,
saved volume reopen and a later Z-plane export. The formerly refused file
matches the Float32 control's NPY bytes. The 22-member archive and source
package passed admission. The cached build needed no retry.
[Handoff](../build/mrc-reader/half-candidate/results.json)

## Delivered locally - retain JP2 capture calibration

OpenJPEG decodes the same JP2 samples with an authored capture-resolution
box. The 1.4.0 reader refuses the wrapper. The 1.5.0 provider exposes
declared X/Y capture spacing and keeps default display resolution out of
measurement metadata. The formerly refused file exports exact pixels.

The relocated and offline modules each passed 38 producer profiles and
162 CLI checks. A saved UInt16 project preserves 0.25 by 0.5 µm spacing and
exact cross-tile samples, including cached spacing without the provider.
The shared packed reader's TGA regression passed 88 checks. All 112 package
members passed admission. The cached build and delivery checks needed no retry.
[Handoff](../build/jpeg2000-reader/resolution-candidate/results.json)

## Verified locally - retain numeric cell measurements without contours

The numeric-only regression fixture wrote to an older archive offset and
invalidated the contour flag. It now locates the captured contour parameters
and changes only the vertex limit before recalculating the archive checksum.
Both the frozen binding and staged native CLI preserve numeric rows with
`geometry_unavailable` status. Strict export preserves an existing table.

The cell-table regression group passed 54 tests with two skips. The manual
CLI probe initially held the SQLite connection open. Closing it allowed
the regular file guard to acquire its read handle. This packet reused the
existing runtime and changed no product code.
[Handoff](../build/cell-numeric-archive/results.json)

## Delivered locally - protect cell-measurement export destinations

The retained CLI wrote a four-row cell table inside an attached NGFF label
store, at an offline store location and over a missing ICS1 companion path.
The cell-table CLI now uses the shared project-input guard before capture
and at publication, with historical rows available while sources are offline.

The native cell-table suite and 35 focused tests passed. Five protected
destinations refused, two external exports matched baseline bytes and all
input hashes matched. The cached build needed no retry. A wider run found a
numeric-only archive fixture failure in the frozen binding. The isolated test
reproduced it without the changed CLI.
[Handoff](../build/cell-table-input-guard/results.json)

## Delivered locally - protect recorded companion paths during TMA export

The retained CLI wrote a TMA proposal, grid and both summary formats to the
offline `.ids.gz` path of a saved ICS1 link. The shared project guard now
protects saved plugin and reviewed companions without opening their readers.
The file guard recognizes matching names and case aliases while offline.

Two native suites and 105 relocated tests passed. Five TMA commands each
refused the exact missing path and a case alias. Four external exports
matched baseline bytes, external import passed and input hashes matched.
The follow-up build restricted filesystem canonicalization to plausible
aliases after the first regression run showed extra cost. The staged tests
used native reopen evidence for the current schema.
[Handoff](../build/tma-dearray-companions/results.json)

## Delivered locally - dearray linked directory images

The retained native CLI rejects a saved OME-Zarr TMA image as a non-file,
while an embedded copy of the same raster produces a proposal. The CLI now
uses the saved reader kind and the reader's directory check at publication.

The native dearray suite, ten relocated tests and five CLI comparisons
passed. The linked proposal, pixel and recipe hashes match the embedded
control. Applying and reopening through the current CLI preserves all
twelve cores. Changed chunks and destinations in the store refuse without
publishing. The frozen binding could not author an OME-Zarr link, so the
fixture was created with the existing native SDK. The product build passed
once, and native reopen covered the current project schema.
[Handoff](../build/tma-dearray-directory/results.json)

## Delivered locally - protect linked stores during TMA grid publication

The retained batch CLI wrote a dearray proposal and exported QPTMA grid
inside an unselected image's NGFF label store. QPTMA import already refused
that destination through native project saving. The TMA commands now share
the same saved-input destination guard before creating temporary output.

The relocated CLI passed six protected-destination refusals and three
external operations. Proposals and exported grids matched baseline bytes.
Three native suites and 105 staged checks passed. The first Python test run
tried to open newer project snapshots with the frozen binding. The native
suite and native CLI reopen cover the current schema, and the two matching
frozen interface controls pass separately.
[Handoff](../build/tma-qptma-store-guard/results.json)

## Delivered locally - join TMA context onto standard CSV records

The installed native context join rejects a valid final record at EOF and
headers with a leading UTF-8 signature. Its case-summary sibling accepts both.
The context join now shares the scanner and signature handling. It keeps the
signature at the output start and preserves source fields and line endings.

The native TMA suite verified the accepted variants, stale revision and
malformed record refusals. All 81 tests with the relocated CLI passed,
including existing case-summary digests and TMA batch CSV export. The cached
build and validation needed no retry. The installed binding served as the
baseline; this packet has native source and CLI evidence.
[Handoff](../build/tma-context-csv/results.json)

## Delivered locally - keep TMA summary exports outside linked stores

The retained CLI wrote both case and cohort CSVs inside an NGFF label store
attached to an unselected image. Both exports now use the same saved project
source traversal as density, so online and offline stores are protected
without configuring or opening their decoders.

Two native suites and 81 TMA checks passed against the relocated CLI. Six
comparisons verified four refusals and two byte-identical external exports.
All 45 protected files were preserved. The initial imported grid exceeded the
small fixture's bounds, and the generated fixture had moved its label store
offline. A one-core grid and restoring the isolated store made the baseline
reproducible. The native cleanup failure cleared in a shorter working path
using the same build.
[Handoff](../build/gui-aggregation/gap-closure/tma-protected-stores/results.json)

## Delivered locally - preserve explicitly declared JP2 transparency

OpenJPEG reads authored tiled gray-alpha and sRGB-alpha JP2 fixtures with
their original samples. The 1.3.0 optional module refuses both. The 1.4.0
provider validates explicit channel definitions and preserves straight
whole-image alpha, including original gray or RGB codes under zero opacity.

The relocated package and offline rebuild each passed 34 exact profiles and
149 CLI checks. They cover gray-alpha and RGBA at 8 and 16 bits, reversible
component transformation, unchanged raw validity and malformed definitions.
An RGBA16 project reopened with an exact cross-tile alpha crop. The 112-member
package and archive passed admission. Pillow suppressed the requested RGBA
transform in the first fixture. Authoring the reversible component planes
and checking them with OpenJPEG resolved it without another module build.
[Handoff](../build/jpeg2000-reader/alpha-candidate/results.json)

## Delivered locally - preserve sub-16-bit JPEG 2000 samples

The 1.2.0 provider refuses a 12-bit tiled gray J2K image that OpenJPEG
decodes exactly. The 1.3.0 provider accepts uniform unsigned precision
from 8 through 16 bits in its existing gray and sRGB profiles. It preserves
unshifted sample codes and scales each display channel by its declared maximum.

The relocated package and offline rebuild each passed 29 exact profiles and
127 CLI checks. Producer coverage includes 8, 9, 10, 12, 14 and 16 bits.
A saved RGB12 project reopened with exact cross-tile channel values. The
112-member package and archive passed admission. FFmpeg conversion to gray12
and Pillow's 9-bit gray path altered comparison values. The fixture checks
now use the producer's original codes and the exact left-aligned reference
route. SDK discovery was corrected using its recorded cache entry.
Neither test correction required rebuilding the module.
[Handoff](../build/jpeg2000-reader/precision-candidate/results.json)

## Delivered locally - open lossless sRGB JPEG 2000 images

OpenJPEG reads an authored reversible tiled sRGB JP2 exactly. The 1.1.0
module refuses its three components. The 1.2.0 provider adds explicitly
enumerated JP2 sRGB at UInt8 and UInt16 with packed raw RGB and RGBA8 display.
It uses the admitted JasPer codestream entry directly and takes the color
declaration from the verified JP2 wrapper.

The relocated DLL and offline source rebuild each passed 17 exact profiles
and 81 CLI checks. Native projects preserve the RGB16 cross-tile crop,
offline channel metadata and the review required to adopt the new module
for an old gray project. All 112 archive members passed admission.
Using the cache's recorded CMake fixed a module-version mismatch. The first
RGB decode encountered warnings while creating a synthetic JasPer ICC profile.
Direct codestream decoding removed that step and kept strict decoder warnings.
Staging needed the pinned tarball path. Select the next demonstrated gap.
[Handoff](../build/jpeg2000-reader/rgb-candidate/results.json)

## Delivered locally - discover gzip-named ICS1 companions

Pinned libics reads the authored gzip-declared ICS1 pair with an `.ids.gz`
companion. The Windows 1.4.0 host exits 2 with the alternate name and 0
with the same bytes under `.ids`. The 1.5.0 module discovers the alternate
only when the canonical name is absent and the header declares gzip.

The relocated package passed 83 positive cases, 109 refusals and 30 checks.
Raw NPY bytes match the canonical control exactly. Reopen preserves the
alternate source identity, and introducing a canonical companion makes
cached raw and display reads refuse. The 25-member handoff and an offline
ABI rebuild passed. The first run inherited a stale companion name in the
Unicode control. Correcting it required only the ABI test executable to be
rebuilt. Select the next demonstrated workflow gap.
[Handoff](../build/ics-reader/gzip-companion/results.json)

## Delivered locally - open planar and reordered ICS microscopy arrays

The 1.3.0 package refused an explicit planar ICS1/IDS image. The 1.4.0 module
maps unique X/Y/Z/T/channel axes by bounded storage strides. Raw HWC keeps
exact samples, and calibration remains attached to each named axis.

The relocated Windows package passed 82 positive fixtures, 107 refusals and
29 checks. The formerly refused file exported exactly the same NPY bytes as
the interleaved control. Project reopen preserved reordered time metadata.
The 25-member package and separate offline direct ABI build passed. The
cached build needed no retry. Select the next demonstrated workflow gap.
[Handoff](../build/ics-reader/axis-order/results.json)

## Delivered locally - open ICS microscopy time series

The 1.2.0 module refused a CXYZT capture. The 1.3.0 module accepts terminal
T, retains exact C/Z/T samples and publishes seconds calibration when declared.
The pinned reference checks ICS1/IDS, inline ICS2 and offset external ICS2.

The relocated Windows package passed 79 positive fixtures, 106 refusals
and 27 checks. They cover later-frame exports, exact project reopen and time
metadata when the provider is offline. The 25-member package and offline
direct ABI build passed. The cached module and consumers needed no retry.
Select the next demonstrated workflow gap.
[Handoff](../build/ics-reader/time-series/results.json)

## Delivered locally - open FITS images alongside ancillary ASCII tables

The 1.4.0 package refused a selected image preceded by a checksummed TABLE.
The 1.5.0 module traverses ASCII tables using checked row dimensions, column
starts and space padding. It verifies declared sums over padded records and
preserves physical image indices.

The relocated Windows package passed 47 positive fixtures, 79 refusals and
39 checks, including multi-block ASCII data, compressed project reopen and
export refusal for changed table data or padding. The 25-member package and
offline ABI test passed. The first Astropy fixture needed one serialization
before its fixed checksum comments were added. This used the same module.
Select the next demonstrated workflow gap.
[Handoff](../build/fits-reader/ascii-table/results.json)

## Delivered locally - open FITS images alongside ancillary binary tables

The 1.3.0 package refused a selected image preceded by a BINTABLE. The
1.4.0 module traverses ordinary binary-table HDUs using checked row and heap
extents, padding and declared sums. Physical HDU indices are preserved.

The relocated Windows package passed 45 positive fixtures, 73 refusals and
34 checks, including compressed-project reopen and CLI refusal without
export for a damaged unselected heap. The 25-member handoff and separate
offline build passed. The baseline probe needed a manifest option on that
host. The heap oracle needed an initial serialization to finalize PCOUNT
before adding checksums. Neither correction required a module rebuild.
Select the next demonstrated workflow gap.
[Handoff](../build/fits-reader/ancillary-table/results.json)

## Delivered locally - open externally gzip-compressed FITS images

The 1.2.0 package refused a gzip copy of an otherwise admitted FITS image.
The 1.3.0 optional module verifies a single gzip member under the existing work
budget, then uses the same HDU parser and FITS checksum verification.

The relocated Windows package passed 43 positive fixtures, 65 refusals and
30 checks, including saved compressed-source reopen and CLI refusal without
output for an invalid inner checksum. The 25-member handoff includes exact
zlib source and notice. The offline build passed direct ABI. The cached target
needed no retry. Select the next demonstrated workflow gap.
[Handoff](../build/fits-reader/gzip/results.json)

## Delivered locally - verify declared FITS image checksums

The 1.1.0 package refused valid checksum cards. The optional 1.2.0 module
verifies each declared DATASUM and canonical CHECKSUM against stored bytes,
including unselected HDUs, before exposing pixels.

The relocated Windows package passed 39 positive fixtures, 57 refusals and
27 checks, including CLI refusal without an export. The 22-member handoff
includes exact source and notices. An offline rebuild passed direct ABI.
The first validation mixed two generations of the same bundle fixture.
Using identical checksummed bytes for each option check required no rebuild.
Select the next demonstrated workflow gap.
[Handoff](../build/fits-reader/checksum/results.json)

## Delivered locally - open scalar NIfTI time series

The 1.3.0 package refused rank four. The optional 1.4.0 module preserves
XYZT scalar order, exposes exact raw Z/T samples and publishes optional
seconds calibration. Display stays on Z0/T0. Nonzero temporal origin and
unsupported timing metadata refuse before publication.

The relocated Windows package passed 42 positive fixtures, 76 refusals
and 29 checks, including exact later frames and reopened or offline time
metadata. The 25-member package retains matching source and notices.
A separate offline rebuild passed the direct ABI suite. The cached build
needed no retry. Select the next demonstrated workflow gap.
[Handoff](../build/nifti-reader/time-series/results.json)

## Delivered locally - open mixed-compression NIfTI image pairs

The 1.2.0 runtime refused both mixed pairs. The optional 1.3.0 module reads
both directions in NIfTI-1/2 through the existing ABI, preserves matching
compression precedence and tracks the actual selected companion.

Windows relocated validation passed 38 positive fixtures, 69 negative
fixtures and 25 checks. The 25-member package records exact module and
source identity and original notices. The offline rebuild passed the same
direct ABI suite. The first baseline attempt used a previously uninstalled
manifest and was corrected to the retained package. The first ABI launch
needed an updated fixture count and only the test executable was rebuilt.
Select the next demonstrated workflow gap.
[Handoff](../build/nifti-reader/mixed-pairs/results.json)

## Delivered locally - retain reviewed image sources in density checkpoints

The baseline omitted source verification from the density checkpoint. The
single-image save now carries the exact verification under the remapped ID.
Resume checks the saved locator and verification and creates a fresh checkpoint
if either differs.

Three Windows native suites and relocated CLI checks passed. They verified the
exact stored payload, reviewed OME-Zarr relocation, unchanged resume, and fresh
checkpoints after clearing or replacing the baseline. All 45 input hashes
matched. The first SQLite probe omitted URI mode. Correcting the read-only
probe required no rebuild. Select the next demonstrated workflow gap.
[Handoff](../build/gui-aggregation/gap-closure/density-reviewed-source/results.json)

## Delivered locally - retain owned expression assets in density checkpoints

The baseline failed to checkpoint an image with a relative owned matrix.
Single-image checkpoint publication now keeps the source link base through
native Save As. Resume runs the existing bounded verifiers against the
checkpoint's owned QMX and QPR before reuse.

Four Windows native suites and relocated CLI checks passed. An exact matrix
with a count above 2^53 reopened while the input's owned copy was offline.
Missing QMX and changed QPR copies refused without repair. Exact restoration
permitted reuse. Source files were unchanged. One cached fixed build followed
the reproduced failure. Select the next demonstrated workflow gap.
[Handoff](../build/gui-aggregation/gap-closure/density-owned-checkpoint/results.json)

## Delivered locally - protect linked stores during density manifest generation

The baseline CLI published JSON inside an attached NGFF label store. The
generator and run preflight now share the saved-input traversal. Generation
checks the destination before creating atomic scratch and uses the same opened
project as the class preview.

Three Windows native suites and the relocated CLI passed. The CLI refused
unselected image and label destinations and an offline store locator.
Preview, external manifest generation and running the generated manifest
succeeded. All 48 input tree entries were preserved. One cached build
sufficed after baseline reproduction. Select the next demonstrated gap.
[Handoff](../build/gui-aggregation/gap-closure/density-manifest-store-output/results.json)

## Delivered locally - protect linked stores from project-density output

The baseline wrote density output into an attached NGFF label store. The
shared manifest and preview runner now refuses overlap with linked image
and label directories before creating output or its parent. It uses saved
metadata for unselected images and offline stores without loading a decoder.

Four Windows native suites passed. The relocated CLI refused three unsafe
destinations, computed and reused an external run, and preserved all 46
input hashes. The offline ancestor case was added after the first fixed
build, requiring only density and CLI relinks. Select the next demonstrated
workflow gap from the existing inventories.
[Handoff](../build/gui-aggregation/gap-closure/density-linked-store-output/results.json)

## Delivered locally - protect attached NGFF masks from batch scratch output

The baseline wrote batch scratch into an attached NGFF label store. Capture
now reads protected store roots from retained label records across the project.
The existing directory guard runs before creating the output root, lock or
scratch. It uses saved properties without opening the label decoder.

Three native suites and five relocated CLI checks passed. They cover selected
and unselected images, directory aliases, exact source preservation and
successful external output and reuse without configuring the decoder. The
first launcher loaded OpenSlide from a different directory. Co-locating the
test host with the admitted provider resolved it without a rebuild.
Select the next demonstrated workflow gap.
[Handoff](../build/gui-aggregation/gap-closure/batch-ngff-label-output/results.json)

## Delivered locally - verify owned expression assets before checkpoint reuse

The baseline reused a checkpoint with a missing owned QMX or changed owned
QPR. Resume now verifies the checkpoint-owned files through the existing
bounded matrix and archive readers before reporting reuse.

Three Windows native suites passed. Both faults refuse without recreating
or changing the failed asset, and exact restoration permits reuse. The QPR
fixture also exposed a Windows path over 260 characters. The first fix reached
a second canonical open and still failed. Applying the physical-path helper
to the archive decoder and byte read corrected the full reopen path. Select
the next demonstrated workflow gap from the existing inventories.
[Handoff](../build/gui-aggregation/gap-closure/batch-expression-reuse/results.json)

## Delivered locally - clean pending owned expression assets from batch scratch

The baseline exported a valid checkpoint but left the pending owned matrix in
scratch. Normal completion and restart now share the marked-directory reclaim
path. It checks the two managed expression directories and accepts only plain
files with recognized hash-based names or writer temporary suffixes.

Three cached native Windows suites passed. The unsaved-project regression
reopened the exact matrix, verified scratch removal and reused the checkpoint.
A marked abandoned directory containing QMX and QPR scratch was reclaimed.
An unrecognized child preserved its directory and neighboring files. The
build needed no retry. Select the next demonstrated workflow gap.
[Handoff](../build/gui-aggregation/gap-closure/batch-pending-expression/results.json)

## Delivered locally - preserve owned expression matrices through native batch isolation

The baseline refused a saved project with a relative owned matrix. Temporary
batch IO now uses the captured project directory for links. The final
checkpoint publishes its own exact copy and can reopen after the original
project moves.

Four native suites passed, covering exact UInt64 matrix counts, reuse, scratch
cleanup, reduction save/replay and the general batch and OME-Zarr routes.
The first fixed run reached publication and failed at the Windows temporary
filename length. Using the existing physical-path helper for owned asset
copies corrected that failure. The follow-up above verifies ownership cleanup
for pending assets from an unsaved project.
[Handoff](../build/gui-aggregation/gap-closure/batch-owned-expression/results.json)

## Delivered locally - pool UTF-8 signature-marked TMA CSVs

The baseline CLI refused signature-marked inputs in both case and cohort
pooling. Parsing now skips the leading UTF-8 mark while provenance preserves
the exact first file's header bytes. Later files' headers and marks are omitted
from the pooled digest.

The native TMA suite and all 33 case-export tests passed with the relocated
Windows CLI. The two new parameterized tests exercised single, mixed, quoted
and reordered inputs, checked independent SHA-256 digests and weighted means,
and verified input preservation. The cached build needed no retry.
Select the next demonstrated workflow gap from the existing inventories.
[Handoff](../build/gui-aggregation/gap-closure/tma-utf8-signature/results.json)

## Delivered locally - retain study metadata and class defaults in batch checkpoints

The baseline lost project metadata and class definitions during batch
isolation. Each checkpoint now retains the captured study metadata, class
definitions and ordered defaults. The existing budget accounts for that
context, and the input writer releases it before reopening the temporary
project.

Two native suites and seven relocated Windows checks passed. They covered
submission-time snapshots, reuse, class inheritance after reopen, a materialization
limit refusal, and exact context rows in both checkpoints of a two-image batch.
Removing any of the three context tables' data failed seal verification.
The cached build needed no retry. Select the next demonstrated workflow gap.
[Handoff](../build/gui-aggregation/gap-closure/batch-project-context/results.json)

## Delivered locally - carry captured source verification into batch checkpoints

The baseline reproduced an OME-Zarr batch dropping the captured source
verification. The isolated project now retains the selected image's association
and includes it in the existing input digest and checkpoint seal.

Two native suites and six relocated Windows checks passed. They verified exact
association bytes, reviewed-relink admission on reopen, reuse with the same
capture and a distinct checkpoint after a new capture clears the baseline.
Deleting the stored association from a copied checkpoint failed CSV seal
verification. The cached build needed no retry. Select the next demonstrated
workflow gap from the existing inventories.
[Handoff](../build/gui-aggregation/gap-closure/batch-source-verification/results.json)

## Delivered locally - explicitly review moved native OME-Zarr stores

The baseline refused native OME-Zarr at the explicit directory review entry
point. The shared native API now reviews the caller's root and verifies its
primary metadata fingerprint for built-in and plugin directory links.

Three cached Windows native suites passed. The relocation regression rejects
the old store's fingerprint even when metadata bytes are identical. It also
checks explicit acceptance, save/reopen on the new locator and preservation of
historical raw-record provenance. The cached build needed no retry. Select the
next demonstrated workflow gap from the existing inventories.
[Handoff](../build/gui-aggregation/gap-closure/native-zarr-explicit-relocation/results.json)

## Delivered locally - prevent sealed batch CSV exports from replacing inputs

The baseline reproduced checkpoint CSV replacing a companion TIFF. Export
now atomically publishes to a new destination while keeping checkpoint
verification and existing source guards. The CLI help and batch guide state
the new-file rule.

Two native suites and ten relocated Windows checks passed. The tests covered
direct TIFF and hard-link refusal, exact offline extraction, occupied paths,
two writers racing for a new destination, source-directory refusal and changed
seals. The first tamper check kept Python's SQLite handle open and hit the
Windows file guard. Closing the controller's handle reached the checksum check
with the same cached runtime. Select the next demonstrated workflow gap.
[Handoff](../build/gui-aggregation/gap-closure/batch-checkpoint-csv-output/results.json)

## Delivered locally - protect compound raw inputs during measurement CSV export

The baseline reproduced raw CSV overwriting a companion TIFF after opening
historical measurements. The native table snapshot now protects primary and
companion paths, observed file identities and directory roots using saved
source facts. CSV generation and export work without opening a decoder.

Four cached Windows native suites passed. The actual multifile fixture refused
direct replacement, a hard-link alias, and the same alias after its recorded
name moved. It permitted byte-exact export and replacement at a sibling CSV.
OME-Zarr project, managed viewer and scientific resampling regressions passed.
The cached builds needed no retry. Next inspect the separate sealed batch CSV
export, which currently guards only the checkpoint and source locator.
[Handoff](../build/gui-aggregation/gap-closure/raw-csv-source-protection/results.json)

## Delivered locally - verify directories with their admitted reader limits

The baseline review refused a valid saved OME-Zarr source whose primary metadata
exceeded the default 1 MiB limit. Review and apply now check the candidate's
captured source observation and use its own whole-directory currentness check.
Primary-file and fingerprint-alias guards still protect the publication.

Four cached Windows native suites passed. The saved 2 MiB metadata budget
admitted the fixture through reopen, review and apply. A subsequent chunk
change refused apply and preserved the baseline. Native directory, optional
reader-store and fingerprint regressions also passed. The focused build
needed no retry. Select the next demonstrated gap from the existing inventories.
[Handoff](../build/gui-aggregation/gap-closure/directory-review-reader-limits/results.json)

## Delivered locally - check saved OME-Zarr identity on the first source review

The baseline native check reproduced a first review treating a modified chunk
as unverified when the metadata file and its fingerprint were unchanged. Review
now checks the saved directory identity and requires acceptance for changed
dependencies even without a separate fingerprint baseline.

Three native suites passed in the combined Windows cache. They covered first
review of an unchanged store, refusal without state mutation, accepted save
and reopen, preservation of the historical measurement observation, plugin
directory review and fingerprint regressions. The first test build used a
private project revision field. The corrected regression checks public dirty
state, source ownership and the baseline. Select the next demonstrated gap
from the existing inventories.
[Handoff](../build/gui-aggregation/gap-closure/zarr-first-source-review/results.json)

## Delivered locally - select the matching plane in automatic raw batches

The previous CLI aborted when automatic selection encountered an annotation
on another plane. Native capture now uses the same plane rule as measurement.
Explicit off-plane IDs still refuse. Valid empty planes write a header-only
CSV, and invalid axes refuse even when no objects match.

Two native suites and 13 relocated CLI checks passed, covering natural and
grid values, channel-specific ROIs, explicit axes, empty tables, plugin-backed
measurements, exact checkpoint reuse and unchanged inputs. The single cached
build recompiled core consumers of the shared plane header and needed no retry.
Select the next demonstrated workflow gap from the existing inventories.
[Handoff](../build/gui-aggregation/gap-closure/raw-batch-mixed-planes/results.json)

## Delivered locally - retain raw statistics from admitted image plugins

Native raw statistics now admit explicit axes from a provider's saved raw
series. Natural and grid digests bind the exact decoder implementation. The
existing QRA archive retains the source observation, and the project can reopen
and export those records with the decoder absent. Replay verification requires
the exact recorded source observation.

Five native runs and 14 relocated Windows checks passed. The NGFF 0.5 Int8
natural and grid batches matched independent signed-byte measurements and
reused their checkpoints. Both checkpoint projects reopened. Native tests
verified exact plugin replay, refusal to reverify a relocated source and
historical QRA compatibility. The first build exposed a missing C++17 setting
on the authored test module. The first reopen exposed a duplicate TIFF/Zarr
allowlist in SQL loading. The test target now declares C++17 and record restore
validates the saved quantitative series and provider. The admitted module and
combined native cache were reused. Select the next demonstrated workflow gap
from the format and application inventories.
[Handoff](../build/gui-aggregation/gap-closure/plugin-raw-statistics/results.json)

## Delivered locally - follow saved raw axes in project batches

The batch now takes Z and time from the source's saved raw view. Project reopen
uses the same view authority for persisted annotation planes, including with the
OME-Zarr store offline. Explicit axes continue to select the requested plane.

Two native suites and ten relocated CLI checks passed. Native tests covered
natural and grid ROIs at Z=1/time=1, explicit origin axes, and exact checkpoint
and input digest reuse. The CLI verified exact default-versus-explicit input
digests, CSV measurements, off-plane refusal and source preservation.
The first run exposed the TIFF-only annotation reopen check. Later checks
accounted for binary64 mean roundoff, the case of the Windows environment
variable and the per-run CSV observation timestamp. Builds reused the cached
native targets. [Handoff](../build/gui-aggregation/gap-closure/batch-raw-display-axes/results.json)

This work exposed the plugin statistics admission gap addressed above.

## Delivered locally - protect linked directory stores from batch output

The previous batch CLI wrote a lock and run report into a linked OME-Zarr
store. Native batch requests now apply the destination guard before creating
the output directory. It covers built-in and plugin-linked stores, including
missing stores and images outside the selected batch.

Three native suites and nine staged checks passed. Direct paths and Windows
aliases refused with exact store preservation. A sibling directory accepted
the raw batch with explicit axes and reused the same checkpoint. The fixture
builder needed the native SDK and a co-located plugin. The CLI controller then
placed the plugin manifest flag before the command. The product build used the
combined cache. [Handoff](../build/gui-aggregation/gap-closure/batch-source-output-guard/results.json)

## Delivered locally - pool reordered TMA measurement columns

The previous CLI refused CSVs with the same column names in a different order.
Case and cohort pooling now accept them with `--match-columns-by-name`.
Strict default matching is preserved. The pooled digest follows the first file's
column order and retains original field bytes. Duplicate columns, changed
column sets and stale selectors still refuse.

The combined cache rebuilt the affected targets without retries. The native
suite, 19 relocated CLI tests and four independent output checks passed. They
covered reversed inputs, embedded quoted newlines, EOF boundaries, exact
digests and foreign project or stale revision refusals.
[Handoff](../build/gui-aggregation/gap-closure/tma-column-order/results.json)

## Delivered locally - pool TMA CSVs without a final newline

The previous TMA exporter refused complete final rows at EOF. Native case and
cohort pooling now accept those rows, keeping exact single-file digests and
previously accepted pooled output bytes. A separator enters the pooled digest
only when required before later rows. Malformed quoted records still refuse.

The relocated CLI passed 19 focused tests and four independent output/digest
checks. The native suite passed from a short scratch directory after cleanup
of a 267-character path failed in the combined cache. The build needed no
retry. [Handoff](../build/gui-aggregation/gap-closure/tma-unclosed-record/results.json)

## Delivered locally - extract verified OMERO raw arrays offline

The previous Windows CLI refused extraction. The native `extract-raw-bundle`
route now streams the exact NPY bytes through the existing verifier and atomically
publishes after verification. It returns the same receipt without network calls.
The combined native cache rebuilt the affected targets without retries.

Three native suites and 26 staged checks passed across five valid archives,
including a multi-block array. Invalid framing, altered CRCs and digests,
destination collisions and aliases, and cancellation refused without output
publication. [Handoff](../build/gui-aggregation/gap-closure/omero-extraction/results.json)

## Delivered locally - signed 8-bit OME-Zarr 0.5 images

Reader 1.12.0 refused the producer's regular and indexed-shard Int8 stores.
Version 1.13.0 preserves the signed samples and sparse fills through the existing
typed ABI. One-byte serializers share byte-width handling. The selected profile
now covers all ten real scalar sample types represented by the native reader ABI.

Thirty-one profiles passed 1,259 commands. The 89 new checks also passed with
the combined host, relocated module and offline rebuild. The admitted 128-member
module package includes matching source and notices. Removal preserved the host,
its existing OpenSlide DLL and the export. Thirteen earlier archives pass
admission. The module build reused the SDK and cache without retries.
[Handoff](../build/gui-aggregation/catalogue-closure/ngff05/int8-candidate/results.json)

## Delivered locally - lossless Float64 OME-Zarr 0.5 images

Reader 1.11.0 refused the pinned producer's regular gzip and indexed-shard
Float64 stores. Version 1.12.0 preserves binary64 samples and fills, including
signed zero, subnormals, infinities and NaN payloads. Float32 retains its
direct decimal rounding. The module and NGFF 0.4 regression builds reused
their caches without retries.

Twenty-nine profiles passed 1,170 commands, and the built-in NGFF 0.4 suite
passed. The 106 new checks also passed with the combined host, relocated
module and offline rebuild. The admitted 128-member module package includes
matching source and notices. Twelve earlier archives still pass admission.
Removal preserved the separate host, its OpenSlide DLL and the exported array.
[Handoff](../build/gui-aggregation/catalogue-closure/ngff05/float64-candidate/results.json)

## Delivered locally - exact 64-bit integer OME-Zarr 0.5 images

The 1.10.0 provider refused both original 64-bit producer stores. Version
1.11.0 retains the full Int64/UInt64 range across export and sparse fills in
both byte orders. Integer range validation now shares the typed conversion
used for missing chunks. The pinned producer wrote the 64-bit endpoints after
resampling its pyramid to keep those base-level samples exact.

Twenty-seven profiles passed 1,064 commands. The new profiles passed 98
checks on the combined host, the relocated installation and the offline-rebuilt
module. Eleven prior packages passed admission. The 128-member module package
includes matching source and notices. Removal preserved the native host,
its shared OpenSlide DLL and the exported user array. The focused module build
reused the SDK and cache without retries. Select the next demonstrated gap.
[Handoff](../build/gui-aggregation/catalogue-closure/ngff05/int64-uint64-candidate/results.json)

## Delivered locally - 32-bit integer OME-Zarr 0.5 images

The pinned OME-Zarr producer wrote a signed big-endian gzip store and an
unsigned transposed, checksummed shard store. Reader 1.9.0 refused both types.
Version 1.10.0 reads them through the existing typed store ABI and preserves
exact Int32/UInt32 samples, calibrated levels, sparse fills and display endpoints.
The focused build reused the module cache, SDK and pinned codecs. The first
oracle run lacked a crop channel argument. The first staging run used swapped
profile counts. Correcting the controller and staging check required no rebuild.

Twenty-five fixture profiles passed 966 commands on the retained host. The two
new profiles each passed on the combined host, the relocated package and the
offline-rebuilt module. The admitted 128-member module package includes matching
source and notices. Removal preserved the host, its existing OpenSlide DLL and
the exported user array. [Handoff](../build/gui-aggregation/catalogue-closure/ngff05/int32-uint32-candidate/results.json)

## Delivered locally - combined Windows native runtime

One offline Release build linked the desktop, region, labels, format import,
batch and OMERO against the current core with Python disabled. CMake staged the
runtime, SDK and both plugin development kits. Four focused native tests passed.
Twelve staged commands verified exact FITS bundle extraction, AnnData matrix
identity and provider-free SDK reopen, ImageJ ROI import and desktop project
reopen, exact NGFF label replay, batch label input and offline OMERO verification.
The smoke controller needed Windows' uppercase environment key, the declared
label-provider dependency and the authored record ID from the reference CSV.
The corrected run reused the same build. The receipt hashes all 294 staged files.
[Build log](../build/native-format-closeout/validation-packet/configure-root-02/current-combined-build.log) ·
[Native tests](../build/native-format-closeout/validation-packet/configure-root-02/current-ctest.log) ·
[Stage receipt](../build/native-format-closeout/validation-packet/configure-root-02/current-smoke-04/results.json)

The eight-gap [feature expansion](feature-expansion-plan.md) has 22 of 22 items
complete in their stated profiles. The combined runtime is a local engineering
stage. Any release using it needs a frozen source/package inventory and clean
install, run and removal checks. Windows candidate9 retains its own scope.
Select the next demonstrated workflow gap.

The [format roadmap](format-plugin-coverage-plan.md#full-format-roadmap-2026-11-14)
still tracks candidate formats and unqualified profiles of delivered formats.
The [application queue](api-delivery-plan.md#execution-order-for-finishing-the-remaining-work)
tracks TMA frontiers, batch and large-dataset limits, external exchange, and
platform and release claims. Select further implementation when a concrete
workflow calls for it. Public OMERO raw/point exchange has evidence. Live IDR
masks still lack it, and private access and writes are separate scope decisions.

## Delivered locally - extract verified raw arrays from saved bundles

The retained CLI had verification but no extraction route. The native
`--extract-raw-bundle` command streams exact NPY bytes through the existing
verifier and publishes them atomically after verification. Both receipt
versions worked offline. Tests cover multi-block validity, byte-identical
Float64 NRRD samples, archived NGFF 0.5, application cancellation, corrupt
archives, hard-link aliases, occupied targets and failure cleanup.

Fifty-four bundle CLI checks, nineteen NGFF checks, two supplemental
extractions and the native raw/PAM suite passed. The targeted build reused
the region cache and SDK without retries. Select the next demonstrated gap.
[Baseline](../build/fits-reader/raw-region-bundle-extract/baseline.log) ·
[Handoff](../build/fits-reader/raw-region-bundle-extract/results.json)

## Delivered locally - verify saved raw region bundles offline

The retained CLI could write its region ZIP but could not verify it. New
bundles carry SHA-256 digests of the full NPY and the canonical receipt.
`--verify-raw-bundle` validates framing, CRCs, dtype, extent, and structured
validity counts without opening the source. It also accepts historical
CRC-only bundles. Changed array and receipt bytes with updated CRCs refused.

Thirty-three bundle CLI checks, the legacy NGFF 0.5 archive, thirteen
FITS/TIFF and nineteen NGFF checks, and the native TIFF/PAM suite passed.
The shared framing parser passed ten OMERO archive checks and the native CLI
and exchange suites. Both targeted builds reused their SDK and caches without
retries. Select the next demonstrated gap.
[Baseline](../build/fits-reader/raw-region-bundle-verify/baseline.log) ·
[Handoff](../build/fits-reader/raw-region-bundle-verify/results.json)

## Delivered locally - package raw regions with their source receipt

The retained region CLI refused a bundle request and wrote reports to stdout.
The native `--bundle` route now publishes one ZIP with exact NPY bytes and a
versioned source receipt. Independent ZIP checks verified both CRCs, embedded
reports, source identity and calibration for TIFF, FITS and translated NGFF
0.4. NGFF 0.5 matched the previous verified structured NPY bytes. Protected,
occupied, missing-parent, cancelled and duplicate requests refused without
publishing or losing user files.

Thirteen bundle CLI checks, nineteen NGFF checks, thirteen FITS/TIFF checks
and the native export/PAM suite passed. The targeted build reused the native
region cache and SDK without retries. Select the next demonstrated gap.
[Baseline](../build/fits-reader/raw-region-bundle/baseline.log) ·
[Handoff](../build/fits-reader/raw-region-bundle/results.json)

## Delivered locally - open bzip2-compressed NRRD scalar volumes

Version 1.10.0 implements `bzip2` and `bz2` with the signed, pinned Sourceware
libbzip2 1.0.8 library. All four Z planes of the previously refused attached
and detached inputs matched exactly. The native wrapper enforces one checked
stream per payload, decoded extent and a bounded 3 MiB codec workspace.
The DLL has the two reader ABI exports and no codec DLL imports.

The built, relocated and offline-rebuilt modules each passed 155 valid
profiles, 153 refusals and 60 native commands. The 24-member package includes
the exact upstream source and original notices. Detached project replay and
removal passed, as did admission of all ten earlier archives. The first Git
GPG invocation needed a forward-slash workspace-relative path. The corrected
invocation verified the upstream signature. Select the next demonstrated gap.
[Baseline](../build/nrrd-reader/bzip2-candidate/baseline.json) ·
[Upstream signature](../build/nrrd-reader/bzip2-candidate/signature-final.log) ·
[Handoff](../build/nrrd-reader/bzip2-candidate/results.json) ·
[Original input exports](../build/nrrd-reader/bzip2-candidate/original-inputs/results.json)

## Delivered locally - open hexadecimal NRRD scalar payloads

Version 1.9.0 reads attached, detached and multi-file hex scalar inputs.
Both original files refused by 1.8.0 now export exact typed bytes, including
Float64 signaling and quiet NaN payloads. The in-place decoder shares the
endian, offset and companion authorities. It rejects malformed and excess
digits and verifies the declared decoded extent.

The built, relocated and offline-rebuilt modules each passed 149 valid
profiles, 144 refusals and 54 native commands. Detached projects reopened,
replayed exactly and detected replacement. Removal preserved user files,
and all nine prior archives passed admission. The focused build reused the
SDK and zlib without retries. Select the next demonstrated gap.
[Baseline](../build/nrrd-reader/hex-candidate/baseline.json) ·
[Handoff](../build/nrrd-reader/hex-candidate/results.json) ·
[Original input exports](../build/nrrd-reader/hex-candidate/original-inputs/results.json)

## Delivered locally - read the remaining NRRD scalar types

Version 1.8.0 adds Int8, UInt32, Int32, UInt64, Int64 and Float64 using
the existing ABI. All six previously refused producer inputs export the
exact dtype and raw bytes. UInt64 values above 2^53 and original Float64
NaN payloads survive project reopen. Display keeps its binary64 window and
deals with nonfinite samples without evaluating signaling NaNs.

The built, relocated and offline-rebuilt modules each passed 141 profiles,
135 refusals and 48 native commands. Eight earlier archives pass admission,
and removal preserved user files. The checked build reused the SDK and zlib
without retries. Select the next demonstrated catalogue or integration gap.
[Baseline](../build/nrrd-reader/scalar-types-candidate/baseline.json) ·
[Handoff](../build/nrrd-reader/scalar-types-candidate/results.json) ·
[Original input exports](../build/nrrd-reader/scalar-types-candidate/original-inputs/results.json)

## Delivered locally - open numbered detached NRRD series

Version 1.7.0 resolves bounded signed decimal filename patterns through the
existing ordered companion and extent contract. All three planes of the
previously refused descending stack match the pynrrd reference raster. The
CLI also read and observed all 63 generated files in the limit test.

The built, relocated and offline-rebuilt modules each passed 92 valid profiles,
129 refusals and 44 native commands. A saved project replayed exactly and
detected a byte-identical companion replacement. Removal preserved project
and payload files. All seven earlier archives passed admission. The targeted
build reused the SDK and zlib cache and required no retries. Select the next
demonstrated catalogue or integration gap.
[Baseline](../build/nrrd-reader/pattern-candidate/baseline.json) ·
[Handoff](../build/nrrd-reader/pattern-candidate/results.json) ·
[Original stack and host cap](../build/nrrd-reader/pattern-candidate/ordered-exports/results.json)

## Delivered locally - open ordered detached NRRD file lists

Version 1.6.0 reads explicit LIST rows, slices and equal slabs with the
existing raw, ASCII and gzip decoders. The retained provider refused the
three-file reference stack. All six Z exports from its two declared orders
match the producer samples. The host also recorded all 63 companions of the
maximum-sized list. A saved project replayed exactly after reopening and
refused current reads after a byte-identical companion replacement.

The built, relocated and offline-rebuilt modules each passed 86 valid
profiles, 120 refusals and 39 native commands. All six prior archives passed
admission and removal preserved the project and all of its payload files.
The baseline producer needed a string filename for pynrrd's write API.
The checked build reused the retained SDK and zlib. Select the next
demonstrated catalogue or integration gap.
[Baseline](../build/nrrd-reader/list-candidate/baseline.json) ·
[Handoff](../build/nrrd-reader/list-candidate/results.json) ·
[Source order and host cap](../build/nrrd-reader/list-candidate/source-order/results.json)

## Delivered locally - read NRRD scalar payloads with line preambles

The retained provider refused four exact pynrrd controls, including a
detached PGM raster. Version 1.5.0 exported all four exactly. It skips LF or
CRLF lines before byte skipping and decompression. Raw EOF-relative placement
takes precedence. Sparse fixtures exercised the exact 256 MiB detached scan
boundary, with a terminator at the limit accepted and one beyond it refused.

The built, relocated and offline-rebuilt modules each passed 79 profiles,
113 refusals and 34 native commands. Detached projects replayed exactly,
companion replacement was detected, and removal preserved user files.
All five older archives passed admission. The first oracle process looped
through the maximum line count before seeking to EOF. Stopped it and checked
the EOF raster with literal bytes already cross-checked by pynrrd. Select
the next demonstrated catalogue or integration gap.
[Baseline](../build/nrrd-reader/line-skip-candidate/baseline.json) ·
[Handoff](../build/nrrd-reader/line-skip-candidate/results.json) ·
[Original inputs and boundary](../build/nrrd-reader/line-skip-candidate/original-and-boundary/results.json)

## Delivered locally - read NRRD scalar payloads with byte offsets

The retained 1.3.0 provider refused five attached and detached controls.
Version 1.4.0 exported their ten original Z planes exactly. Detached raw
reads selected the same 16-byte raster by absolute offset and EOF-relative
placement from a 300 MiB sparse companion, preserving its full identity.
Gzip offsets apply inside the decoded member per the NRRD definition.

The built, relocated and offline-rebuilt modules each passed 72 valid
profiles, 107 refusals and 29 native commands. Projects reopened and replayed
exactly, companion replacement was detected, and removal preserved user files.
All four older archives still passed admission. The builds reused the retained
host and zlib cache without retries. Select the next demonstrated gap.
[Baseline](../build/nrrd-reader/byte-skip-candidate/baseline.json) ·
[Handoff](../build/nrrd-reader/byte-skip-candidate/results.json) ·
[Original planes and range reads](../build/nrrd-reader/byte-skip-candidate/original-and-range/results.json)

## Delivered locally - export complete translated OME-Zarr levels

The retained full-dimension export of a translated level padded the left edge
and missed the right edge. Plain export refused it. Built-in OME-Zarr now
supports `--level-coordinates` over the native decoder. Both layouts exported
the complete translated plane exactly. Signed-index crops preserved validity,
and overflowing indices refused before publication. The legacy coordinate
route exported identical bytes and source identity. The report identifies
the input coordinate space.

Nineteen built-in checks, thirteen FITS/TIFF CLI checks and both native suites
passed. The rebuilt and retained NGFF 0.5 modules matched the earlier exact
producer arrays at both levels. The focused builds reused the SDK and caches
without retries. Select the next demonstrated catalogue or integration gap.
[Baseline](../build/fits-reader/ngff-level-coordinates/baseline.log) ·
[Handoff and checks](../build/fits-reader/ngff-level-coordinates/results.json)

## Delivered locally - report native OME-Zarr crop placement

A translated level-1 store maps the level-zero request (1, 1) to array index
(-1, 1). The report now adds that signed index, dataset path and the declared
axis order, units and effective transform. The native read and report share
the same mapping. The translated crop's NPY bytes and observed directory
identity match the retained run exactly. Fourteen built-in and thirteen
FITS/TIFF CLI checks, native NGFF 0.4 and TIFF/PAM suites passed. Retained and
rebuilt NGFF 0.5 modules matched producer arrays at levels 0 and 1, including
a negative odd coordinate and padding.

The scoped native test harness initially omitted the fixture path definition.
Giving it a CMake-normalized path fixed both the missing definition and Windows
escape warnings. Only the affected consumers were rebuilt against the retained
SDK. Select the next demonstrated catalogue or integration gap.
[Baseline](../build/fits-reader/ngff-export-placement/baseline.json) ·
[Handoff and checks](../build/fits-reader/ngff-export-placement/results.json)

## Delivered locally - report selected OME-Zarr pyramid spacing in raw exports

The retained level-1 export reported level-zero spacing of 1.5 by 1.0 µm.
Both array layouts now report the selected output grid, 3.0 by 2.0 µm in
the fixture. The original NPY bytes and observed directory identity match
exactly. Thirteen built-in OME-Zarr checks covered level-zero and level-one
samples, Z/T calibration, missing units and rejection before publication on
unit-conversion overflow. FITS/TIFF, native TIFF/PAM and both retained
NGFF plugin regression routes passed. The targeted build reused the SDK.

The verification receipt was written successfully before the console failed
to render a unit symbol. Verified the persisted receipt and binary hashes
directly. Select the next demonstrated catalogue or integration gap.
[Baseline](../build/fits-reader/ngff-pyramid-calibration/baseline.json) ·
[Handoff and checks](../build/fits-reader/ngff-pyramid-calibration/results.json)

## Delivered locally - export raw regions from built-in OME-Zarr

The retained region CLI advertised built-in OME-Zarr raw support but refused
the plain NGFF0.4 fixture at plugin-only source capture. The export now reuses
the native store verifier through publication and reports the directory identity
and available calibration. Nine CLI checks covered exact two-channel Z/T and
pyramid selection in three encodings, valid sparse fill versus invalid padding,
protected outputs and damaged chunks. All 169 inventoried fixture hashes matched.
Thirteen FITS/TIFF CLI checks, the native TIFF/PAM suite and both retained NGFF
plugin exports passed. The later CLI is preserved in its own local runtime.

The first controls used an imprecise diagnostic and the wrong FITS argument
paths. The existing NGFF plugin also required its manifest beside the loaded
OpenSlide DLL. Corrected the checks and runtime layout without another build.
The targeted consumers built against the retained SDK. Select the next
demonstrated catalogue or integration gap.
[Baseline](../build/fits-reader/ngff-raw-export/baseline.log) ·
[Handoff and checks](../build/fits-reader/ngff-raw-export/results.json)

## Delivered locally - read attached and detached ASCII NRRD scalar data

The retained 1.2.0 module refused both pynrrd ASCII producer files. Version
1.3.0 exported all four original Z planes byte-for-byte. The built, relocated
and offline-rebuilt modules each passed 66 exact profiles, 95 rejections and
24 native commands. Detached project reopen and saved analysis replay passed.
Byte-identical payload replacement and a changed companion declaration both
invalidated the old binding. Removal of 22 admitted package files preserved
the user research files. The three older archives still pass admission.

The oracle exposed the text NaN convention, so both signs now decode to one
quiet NaN. Binary fixture bits still match exactly. The direct test driver's
fixture-count guard needed updating for the new cases. The supplemental
producer check initially assumed an XY output and an explicit empty companion
array. It now verifies native YXC layout and each observed source scope.
The candidate reused retained zlib and the native host. Select the next
demonstrated catalogue or integration gap.
[Baseline](../build/nrrd-reader/ascii-candidate/baseline.json) ·
[Handoff and checks](../build/nrrd-reader/ascii-candidate/results.json)

## Delivered locally - verify OMERO raw bundles offline

The retained CLI rejected verification of its own raw ZIP. The native
`verify-raw-bundle` route checks ZIP framing and both CRCs, exact NPY shape
and type, the original sample/validity digest, and the canonical receipt
digest. It reports the verified receipt without transport. Four historical
bundles passed. Six changed archives refused, including a sample and a
receipt change with recomputed ZIP CRCs. The current CLI also verifies
freshly written bundles, refuses cancellation, and issues zero requests.

The retained OMERO integration cache built without retry. The native
protocol, CLI and exchange suites passed. Bundle writing and verification
share the existing framing and size profile. Select the next demonstrated
workflow.
[Baseline](../build/gui-aggregation/gap-closure/omero-bundle-verify/baseline.log) ·
[Handoff and checks](../build/gui-aggregation/gap-closure/omero-bundle-verify/results.json)

## Delivered locally - publish OMERO raw samples and provenance together

The stub transport reproduced legacy split export leaving a completed NPY
when the receipt write failed. The new `--bundle` publishes one ZIP with
the same NPY and JSON bytes under an atomic new-only write. The protocol,
CLI and exchange suites passed. An independent ZIP reader verified CRC and
byte equality for CLI, C-order, Fortran-order and Blosc source fixtures.
Occupied destinations and missing parents refused before transport.
Cancellation refused publication.

The command conflict is a usage error, so the assertion now checks status
2 and zero requests. The first exchange run used the Blosc-only manifest.
The retained Blosc and PNG manifest passed the full exchange suite. The
OMERO targets reused the integration cache and the retained core.
Select the next demonstrated workflow.
[Baseline](../build/gui-aggregation/gap-closure/omero-bundle-candidate/baseline.log) ·
[Handoff and checks](../build/gui-aggregation/gap-closure/omero-bundle-candidate/results.json)

## Delivered locally - export complete raw regions with validity

The retained region CLI refused a FITS HDU with both a valid zero and a
missing pixel. `--with-validity` exports it as one atomic structured NPY
with native values and a Boolean validity flag per pixel. Astropy verified
the fixture. NumPy and CLI checks verified FITS missingness and padding,
exact float bits, integer BLANK across writer batches, three-channel TIFF,
source identity, and existing-file protection. Both historical plain
NPY routes matched byte for byte. The 13 CLI checks and native TIFF/PAM
suite passed, including cancellation without publication.

The region targets reused the installed SDK and native store overlay and
needed no failed build retry. Select the next demonstrated workflow.
[Baseline](../build/fits-reader/validity-export/baseline.json) ·
[Handoff and checks](../build/fits-reader/validity-export/results.json)

## Delivered locally - select native reader options per region export

The retained region CLI required a separate manifest for each FITS HDU or
window. The new per-open option replaces manifest defaults on the normal
raw, region, thumbnail, resample and nominal RGB paths. Both selected HDUs
exported exact archived arrays and their canonical bindings matched the
manifest controls. Display matched the manifest controls and the expected
literal window colors. The 24-command FITS check, native TIFF/PAM suite and
exact NGFF directory-store default/empty-options exports passed.

The first link lacked store symbols until the retained NGFF overlay was
added with the existing SDK. The directory-store check needed the plugin
and its already-loaded OpenSlide DLL beside the host. FITS comparisons
exposed per-open timestamps inside the snapshot checksum and the fixture's
missing samples. Verify each checksum and stable binding, and retain the
missing-sample refusal while comparing the valid crop. No codec or SDK was
rebuilt. Select the next demonstrated workflow.
[Baseline](../build/fits-reader/invocation-options/baseline.json) ·
[Handoff and checks](../build/fits-reader/invocation-options/results.json)

## Delivered locally - import four-bitplane PCX with the header palette

PCX 1.0.0 refused authored version-2 and version-5 files that Pillow
reopened with exact indices and header palette colors. PCX 1.1.0 recovers
the four-bit index in the native UInt8 raw channel and uses the header
palette for display. The built, relocated and offline-rebuilt modules
passed 17 exact profiles and 56 CLI checks, with nonzero plane padding,
tail bits, cross-plane runs, all 16 indices and earlier RGB/indexed routes.

The installed SDK reopened a project with distinct indices sharing a
color. Unavailable-provider and changed-implementation checks passed,
including review against a 1.0.0 project. Both 15-member archives still
pass admission. Removal preserved source, project and export. The first
build exposed a version variable shadowing warning. The local field was
renamed and only the adapter recompiled. Select the next demonstrated gap.
[Baseline](../build/pcx-reader/planar-candidate/baseline.json) ·
[Handoff and checks](../build/pcx-reader/planar-candidate/results.json)

## Delivered locally - import version-5 eight-bit PCX rasters

The retained CLI refused Pillow's RGB, indexed and gray-palette PCX files.
The optional ABI1 plugin exported all three with exact samples. The built,
relocated and offline-rebuilt modules each passed seven exact profiles and
32 CLI checks. Raw data preserves indices and planar RGB. Display uses
the file palette and both outputs omit planar padding. The native SDK saved
and reopened a project with distinct indices that display the same color.
Missing and changed provider checks passed. PCX, TGA, and QOI read exact
samples together under one manifest.

The 15-member package passed admission and removal preserved the source,
project and export. The adapters share codec-free staging. Restaging TGA
reproduced all 15 existing files and left the inventory byte-identical.
The first staging summary printed a member name instead of the format
name. Corrected the label independently of archive contents. The adapter
and small SDK consumer built without retry. Select the next demonstrated
catalogue workflow.
[Baseline](../build/pcx-reader/baseline.json) ·
[Handoff and checks](../build/pcx-reader/results.json)

## Delivered locally - import grayscale and indexed TGA rasters

TGA 1.0.0 refused all four exact Pillow controls. Version 1.1.0 reads
plain/RLE 8-bit grayscale and index data, with the 24-bit palette used for
display. The built, relocated and offline-rebuilt modules each passed 29
exact cases and 88 CLI checks. These include distinct indices with the same
RGB, partial palettes, out-of-range index refusal and historical true-color
profiles. Existing QOI and JPEG 2000 providers passed their shared transfer
regressions after the channel-name change.

The retained SDK saved and reopened an indexed image with exact indices
and display. Missing and changed implementation checks passed, including
source review of a project linked to 1.0.0 against the new contract. Both
15-member packages passed admission. Removal preserved the source, project
and raw crop. Build reused the host and required no retry. Select the next
demonstrated catalogue gap.
[Baseline](../build/tga-reader/scalar-candidate/baseline.json) ·
[Handoff and checks](../build/tga-reader/scalar-candidate/results.json)

## Delivered locally - import true-color TGA rasters

Pillow 13.0.0 reopened exact 24-bit BGR and 32-bit BGRA producer files,
each in plain and RLE form. The retained native CLI refused all four.
The optional ABI1 module preserves three or four exact stored channels,
including RGB under zero alpha, with canonical origin and separate display.
After adding preallocation RLE extent and footer/ID overlap checks, the built,
relocated and offline-rebuilt modules each passed 19 exact cases and 60 CLI
checks. Shared transfer passed 40 QOI and 14 JPEG 2000 profiles.

The 15-member source/binary ZIP passed admission. The retained native SDK
saved and reopened a linked RGBA project with exact display and source values.
Missing and changed provider checks passed. Removal preserved source,
project and crop export. No external codec is linked. Indexed/gray,
premultiplied alpha and additional metadata profiles remain separate.
Select the next demonstrated catalogue gap.
[Baseline](../build/tga-reader/baseline.json) ·
[Handoff and checks](../build/tga-reader/results.json)

## Delivered locally - read tiled grayscale JPEG 2000 JP2 and J2K

Version 1.0.0 refused valid tiled inputs. Version 1.1.0 preflights up to 4096
zero-origin tiles with one finite part each. The built, relocated and
offline-rebuilt modules passed 14 exact producer profiles, 69 CLI commands
and direct ABI checks. The controls cover both seams, partial edge tiles,
reordered tiles, duplicate/missing/out-of-grid IDs and the tile-count limit.
The installed SDK saved and reopened a tiled project with exact seam samples.
Provider-unavailable and changed-implementation checks passed.

Pillow's first tiled UInt16 file changed samples. FFmpeg produced exact J2K
but wrote a mismatched JP2 precision byte. Use FFmpeg's unchanged codestream
and an authored conforming wrapper, independently decode both with OpenJPEG,
and verify mismatched precision still refuses. The module build reused pinned
codec objects. Both 112-member archives passed admission. Removal of the
new install preserved the tiled source, project and exported samples.
Select the next demonstrated catalogue gap.
[Baseline](../build/jpeg2000-reader/tiled-candidate/baseline.json) ·
[Handoff and checks](../build/jpeg2000-reader/tiled-candidate/results.json)

## Delivered locally - read baseline grayscale JPEG 2000 JP2 and J2K

The retained CLI refused two lossless UInt16 JP2/J2K producer files. The new
ABI1 plugin preserves every decoded sample in both files. The original,
relocated and offline-rebuilt modules each passed eight independent UInt8/
UInt16 producer profiles, 39 CLI checks, and direct ABI padding, cancellation,
budget and buffer checks. The installed SDK saved and reopened the linked
project, refused a missing provider, and required review for the different
rebuilt module despite exact samples. The shared transfer change passed
all 40 QOI profiles and guards.

One component, one zero-origin tile, one reversible layer and unknown
calibration define this profile. The 112-member Windows local handoff passed
material admission using 91 pinned JasPer 4.2.9 files, original notices and
the matching ABI kit. Tested removal preserved three source/project/export
files. The initial build needed explicit paths to private upstream headers.
The first input also exposed JasPer's minimum sample limit of 256 for one
tile. The adapter still checks the exact raster before decode. QOI used
its original CMake executable after the first cache mismatch. The host and
bindings reused retained builds. Select the next demonstrated catalogue gap.
[Baseline](../build/jpeg2000-reader/baseline.json) ·
[Handoff and checks](../build/jpeg2000-reader/results.json)

## Delivered locally - pool separate measurement files in single-grid TMA export

The retained `--export-tma-cases` rejects a second `--measurements` file.
Reuse the cohort's bounded native file and row path to pool a selected grid
without constructing an intermediate CSV. Compare the complete output and
digest with one equivalent table, preserve historical inputs and schemas,
and verify header, input quota, alias and partial-record refusal. Rebuild
the affected TMA file, table and CLI inputs only.
[Baseline](../build/gui-aggregation/gap-closure/tma-single-grid-pooling/baseline.log)

The unchanged single-grid CSV schema now supports repeated `--measurements`.
The split input matched the complete combined-table export byte for byte,
including the digest and weighted mean 3.5. Reversing input order produced
the expected digest. Aliases, invalid headers, incomplete records, limits,
and wrong image/project selectors in later files refused before publication.
The native suite and 19 focused CLI checks passed. The first native run reached
final cleanup and failed under a long Windows working path. Preserve the fixture
and rerun the same binary in a shorter working directory. No rebuild was needed.
Take the next demonstrated catalogue gap.
[Native CLI and checks](../build/gui-aggregation/gap-closure/tma-single-grid-pooling/results.json)

## Delivered locally - run pinned raw classifier recipes across image-specific ROIs

The retained batch CLI refuses raw classifier recipe input and direct `--object`
selection with multiple images. Reuse the versioned native recipe entry and
existing raw stream, contour, per-image grid and checkpoint authorities. Verify
different ROI selections per image, exact model/project pins, model-grid and
explicit-axis equivalence, resume, refusal and unchanged RGB recipe behavior.
Recompile only recipe handling and the affected native test driver.
[Baseline](../build/gui-aggregation/gap-closure/raw-batch-recipe/baseline.json)

The CLI now accepts a native raw recipe for streams and contours under the
existing `--pixel-batch-recipe` command. Two images used distinct ROI IDs while
excluding distractors. Model-derived and explicit grids produced the same
input commitments. The physical recipe verified 8 and 48 samples on its two
images. The unit-grid control matched the saved-model grid for a unit model.
Native cancellation preserved one checkpoint and resume verified it without
new inference. Incorrect project/model pins and oversized axis integers refused
before creating output. Both native recipe suites, 19 focused CLI tests with
one conditional skip, and seven existing RGB CLI commands passed. Only recipe
handling and its native test driver were recompiled, with no build retry.
Take the next demonstrated catalogue gap.
[Native CLI and checks](../build/gui-aggregation/gap-closure/raw-batch-recipe/results.json)

## Delivered locally - open NIfTI-2 scalar images and volumes

NiBabel 5.3.2 reopened plain and gzip single and paired NIfTI-2 producer
volumes with exact anisotropic binary64 spacings. NIfTI reader 1.1.0 refused
all four. Add versioned header decoding over the existing scalar and file-set
route. Test exact samples, native display, replay, wider dimensions, overflow
and unsupported header refusal, existing NIfTI-1 profiles and offline rebuild.
Build only the module and affected test drivers.
[Baseline](../build/nifti-reader/nifti2-candidate/baseline.json)

All four NiBabel producer exports matched exact Int16 voxels. Native checks
preserved binary64 calibration and read the far edge of a 40,000-pixel-wide
image. Each built, relocated and offline-rebuilt module passed 34 supported
profiles, 66 refusals and 21 native commands. Byte-identical companion
replacement refused currentness and preserved the saved result. The new
25-member archive and both predecessors passed admission. Removal preserved
eight user files and the offline saved project. The fixture-only probe used
the wrong argument spelling initially and passed with the positional path.
No product build retry was needed. General transforms, scaling and other I17
profiles remain separate. Take the next demonstrated catalogue workflow.
[Handoff and checks](../build/nifti-reader/nifti2-candidate/results.json)

## Delivered locally - open paired NIfTI-1 scalar volumes

NiBabel 5.3.2 reopened plain and gzip same-stem header/image pairs with exact
Int16 voxels and calibration. NIfTI 1.0.0 refused both. Reuse the scalar reader,
file-set ABI and gzip decoder. Admit same-directory `.hdr`/`.img` and
`.hdr.gz`/`.img.gz` with exact extents and integral raster offsets. Verify
whole-pair identity, calibrated reads, earlier single-file profiles, atomic
refusal and an offline rebuild before handing off the new module.
[Baseline](../build/nifti-reader/pair-candidate/baseline.json)

Both NiBabel producer pairs exported exact Int16 planes. The built, relocated
and offline-rebuilt modules each passed 28 supported profiles, 53 refusals
and 16 native commands. Byte-identical companion replacement refused reads
while preserving the retained result. Additional CLI checks refused an aliased
or directory companion and kept the source payload unchanged. Both 25-member
archives passed admission. Removing 1.1.0 preserved eight user files and the
offline saved project. The earlier cache remained available and the new build
needed no retry. NIfTI-2, general transforms, scaling and mixed pair encodings
remain separate I17 profiles. Take the next demonstrated catalogue workflow.
[Handoff and checks](../build/nifti-reader/pair-candidate/results.json)

## Delivered locally - import ImageJ rotated rectangle annotations

The retained native plugin refused an authored ImageJ subtype 5 freehand ROI.
The pinned decoder uses two binary32 endpoints and a width to reconstruct its
four image-pixel corners. Reuse the same ABI3 polygon, metadata and ZIP routes.
Build only the plugin and verify exact corners, unchanged prior profiles,
native publication, atomic refusal and offline handoff.
[Baseline](../build/gui-aggregation/catalogue-closure/imagej-roi/rotated-candidate/baseline/validation.json)

The built, relocated and offline-rebuilt modules each passed 28 supported and
47 refused profiles and 16 native/CLI checks. Standalone and ZIP fixtures matched
the pinned reference arithmetic for exact corners. The archive of 66 admitted
files passed verification, and removing those files preserved the source ROI
and both saved projects. The generated manifest and shared ABI callback initially
disagreed on the module version. Use the configured version in both. The first
standalone rebuild found an omitted transitive header. Add `reader_input.hpp`
to the shared source inventory and reuse the offline codec cache. The previous
185- and 194-member archives still pass admission. Rotated ellipses and other
unsupported subtypes remain separate A02 profiles. Take the next demonstrated gap.
[Handoff and checks](../build/gui-aggregation/catalogue-closure/imagej-roi/rotated-candidate/results.json)

## Delivered locally - apply a saved raw model grid across a project batch

On the calibrated UInt16 fixture, explicit project-batch axes succeed and the
existing CLI rejects the saved-model-grid option. Single-image export already
resolves the same exact physical spacing. Reuse that native resolver once per
selected image, including effective project calibration. Check raw stream and
contour output against explicit axes, exact checkpoint reuse, conflicts and
missing calibration. Recompile the native resolver, batch callers and test
consumers affected by the new options field with the retained Windows inputs.
[Baseline](../build/gui-aggregation/gap-closure/batch-model-grid/baseline-product.log)

Both CLI batch modes now accept `--raw-pixel-model-grid`. The resolved and
explicit routes produced exact input/prediction digests, result rows and stored
contour geometry. Checkpoint reuse inferred no new windows. A three-image run
verified grids with 8 and 48 valid samples and refused the uncalibrated image
before publication. Native recipe checks and 18 focused CLI regressions passed
with one conditional skip. Correct the initial test's cross-capture content
hash assertion by authenticating each QWS against its own stored provenance.
Use read-only SQLite inspection for exact contour geometry because the frozen
Python runtime cannot open the candidate checkpoint schema. Adjust the failure
assertion to use reported paths and the failed image directory on Windows.
These test corrections required no second product rebuild. No wheel or release
package is selected. Take the next demonstrated catalogue gap.
[Checks and native CLI](../build/gui-aggregation/gap-closure/batch-model-grid/results.json)

## Delivered locally - open calibrated single-file NIfTI-1 scalar volumes

NiBabel wrote a calibrated Int16 volume as both `.nii` and `.nii.gz`.
The retained host refused both. [Baseline](../build/nifti-reader/baseline.json)
Use the existing image ABI and admitted static zlib. Preserve stored scalar
bits and accept explicit positive axis-aligned spacing with zero origin.
Refuse transforms or intensity scaling beyond this profile. Verify both
encodings, scalar modes, calibration, limits, project replay, source identity
and the matching offline handoff.

Both NiBabel producer volumes exported exact Int16 planes. The built,
relocated and offline-rebuilt modules each passed 22 supported profiles,
38 refusals and 12 native commands. Changed source bytes refused currentness
while preserving the saved result. The 25-member ZIP passed admission and
removal preserved six user files and the offline saved project. The offline
module built without SDK or Python discovery. The first compile required C++17
equivalents for three C++20 calls. The retry reused compiled zlib and corrected
the rank-two fixture to expect XY. The producer receipt needed plain Python
spacing values and fresh output paths to preserve the prior exports.
Keep NIfTI-2, paired files, general transforms and scaling as separate I17
profiles. Select the next catalogue workflow from a demonstrated blocker.
[Handoff and checks](../build/nifti-reader/results.json)

## Delivered locally - select image HDUs in a FITS bundle

Astropy wrote an empty primary plus UInt16 and Float32 IMAGE extensions and
reopened all three HDUs. FITS 1.0.0 refused the source before export.
[Baseline](../build/fits-reader/hdu-candidate/baseline.json)
Add explicit zero-based HDU selection using the existing reader options and
source identity. Check the complete bounded image-only layout and reuse
native sample, display and validity rules for the selected image. Verify
index selection, exact typed crops, saved replay after catalogue defaults
change, malformed skipped units, standalone compatibility and offline handoff.

Both explicit extension selections matched Astropy exactly. The built,
relocated and offline-rebuilt modules each passed 34 supported profiles,
50 refusals and 24 native commands. A saved HDU 1 result replayed exactly
with HDU 2 configured as the catalogue default. Fresh attachment selected
HDU 2 with its own binding. No build retry was needed. Both 22-member
archives passed admission. Removal preserved the five user files and the
offline saved result. Tables, general scaling, WCS, checksums and compression
remain separate I18 outcomes. [Handoff and checks](../build/fits-reader/hdu-candidate/results.json)

## Delivered locally - open primary scalar FITS arrays with exact validity

Astropy 7.2.0 validated an unsigned 16-bit volume and a Float32 image. The
retained host treated both as TIFF and refused export.
[Baseline](../build/fits-reader/baseline.json)
Add a codec-free optional native image plugin for one primary XY or XYZ array.
Preserve valid scalar bits, apply only exact standard unsigned offsets,
and map integer BLANK and floating NaN to zero with invalid native raw
flags, as required by the existing reader bridge. Keep
array index coordinates explicit. Verify all selected representations, malformed
cards and extents, typed crops, saved replay, and the exact offline handoff.

The built, relocated and offline-rebuilt modules each passed 29 supported
profiles, 32 refusals and 17 native commands. Valid UInt64 values above 2^53
and IEEE sample bits stayed exact. The first Float32 CLI probe included a NaN
and hit the existing complete-valid export rule. Use exact valid-region
exports and a separate full-region refusal, retaining the native undefined
mask checks. The 22-member archive passed admission. Removal preserved all
six user files and the offline saved result. Additional HDUs, general
scaling, WCS, checksums and compressed layouts remain separate I18 profiles.
[Handoff and checks](../build/fits-reader/results.json)

## Delivered locally - open calibrated scalar MRC2014 images and volumes

The pinned BSD-3-Clause mrcfile 1.5.4 producer wrote and validated a calibrated
Int16 volume. The retained host fell through to TIFF and refused raw export.
[Baseline](../build/mrc-reader/baseline.json)
Use the published MRC2014 specification and existing native image plugin ABI.
Preserve exact scalar bits and declared column/row/section axis permutations.
Expose orthogonal angstrom spacing as micrometers. Limit the first supported
workflow to single zero-origin images and volumes with no extended header.
Build only the plugin and its test drivers. Verify both endians, all selected
modes, independent channel bytes, calibration, display, native project replay,
refusal, packaging and a relocated offline rebuild.

Both producer planes exported bit-exact, including an asymmetric Y/Z/X storage
fixture. The built, relocated and offline-rebuilt modules each passed 57
supported profiles, 29 refusals and 12 native commands. The 48 mode, endian and
axis combinations used independent mrcfile readback and exact raw comparisons.
The first test build hit a Win32 macro collision. Rename the helper and rebuild
only the test drivers. The venv lacked pip, so extract the hash-verified oracle
wheel locally. The 22-member ZIP passed admission. Removal preserved six user
files and the exact saved result. Stacks, nonzero origins, extended headers,
other modes and older variants remain I19 gaps for demonstrated workflows.
[Handoff and checks](../build/mrc-reader/results.json)

## Delivered locally - honor EXIF orientation on WebP stills

Pillow and the pinned codec wrote EXIF orientation 6 on opaque VP8, alpha
VP8 and alpha VP8L stills. The previous plugin refused all three.
[Baseline](../build/webp-reader/exif-candidate/baseline.json)
Admit the bounded primary orientation tag and apply its exact pixel
permutation to metadata, display and raw coordinates through one mapping.
Keep the original decoded bytes and exclude other metadata semantics.
Build only the module and affected test drivers. Verify all eight
orientations, both byte orders, clipped regions, replay, malformed metadata,
earlier profiles and the exact offline handoff.

The six orientation-6 baseline exports matched Pillow exactly. The built,
relocated and offline-rebuilt modules each passed 32 supported profiles,
68 refusals and 23 native commands, including all eight orientation values
and both TIFF byte orders. An orientation-only source change refused
currentness while keeping the saved result. No build retry was needed.
The 90-member ZIP and both prior archives passed admission. Removal preserved
all six user files and the offline project. Keep broader EXIF metadata,
color management and animation in their existing rows for demonstrated needs.
[Handoff and checks](../build/webp-reader/exif-candidate/results.json)

## Delivered locally - open still lossy WebP with and without alpha

Pillow with the pinned libwebp 1.6.0 wrote an opaque VP8 still and an alpha
VP8X/ALPH/VP8 still. The previous plugin refused both before raw export.
[Baseline](../build/webp-reader/lossy-candidate/baseline.json)
Use the same private decoder and exact RIFF boundaries. Agree on a single
frame and its canvas before allocating. Preserve exact decoded straight RGBA,
including alpha, through raw, display and saved replay; keep metadata and
animation out of this profile. Rebuild only the module and affected test
drivers. Verify both output routes, malformed inputs, earlier lossless
behavior and the matching offline handoff.

WebP 1.1.0 exactly matched the pinned decoder for both raw and display.
The built, relocated and offline-rebuilt modules each passed 24 supported
profiles, 46 refusals and 15 native commands. The archived suite uses the
initial direct-alpha control. The later horizontal-prediction control passed
against both packaged and rebuilt module bytes without another build.
The new and prior 90-member archives passed admission. Removal preserved
all six user files and the offline result. Other extended profiles, metadata
and animation remain separate I05/I07 outcomes. Continue the next
demonstrated eligible gap.
[Handoff and checks](../build/webp-reader/lossy-candidate/results.json)

## Delivered locally - open ICS2 external payloads at the declared offset

The pinned libics oracle opened calibrated raw and gzip ICS2 references at
offset 24 and recovered the same CXYZ bytes as the inline control. ICS 1.1.0
refused both before export. [Baseline](../build/ics-reader/external-candidate/baseline.json)
Use the existing file-set ABI for one regular same-directory payload, with
strict offset syntax and bounded checked range reads. Raw reads take the
declared raster from its offset. Gzip uses the existing exact-member decoder.
Verify native CLI and project replay, payload identity, unsafe path and
truncation refusal, full earlier profiles, and the exact offline handoff.

ICS 1.2.0 matched libics on both Z planes for both payload layouts. A
192 KiB prefix was skipped under a 32 KiB work budget. The syntax review
identified extra TAB fields as malformed before basename validation. Relink
only that reader and use the same candidate cache and retained oracle.
The built, relocated and offline-rebuilt modules each passed 75 supported
profiles, 100 refusals and 24 native commands. Byte-identical companion
replacement refused currentness and kept the exact saved result. The
25-member ZIP and both prior versions passed admission. Removal preserved
all eight user files and the offline project. Alternate ICS1 naming and
other encodings remain separate I15 profiles. Take the next demonstrated gap.
[Handoff and checks](../build/ics-reader/external-candidate/results.json)

## Delivered locally - read gzip ICS channel volumes

Authored gzip payloads retain the calibrated CXYZ codes from the verified
uncompressed ICS fixture. ICS 1.0.0 refused both ICS1 adjacent IDS and ICS2
inline encodings before raw export.
[Baseline](../build/ics-reader/gzip-candidate/baseline.json)
Keep the existing ICS1 file-set and ICS2 single-file rules. Decode one exact
gzip member through the admitted static zlib, charging encoded, decoded and
workspace bytes to the existing limit. Build only the optional reader, pinned
native zlib-enabled libics oracle, and affected test drivers. Verify typed
channels/Z/calibration, saved replay, bounds, damaged streams and unchanged raw
profiles before staging and testing the exact offline handoff.

ICS 1.1.0 matched the zlib-enabled libics oracle on both compressed layouts
and exported exact channel values on both Z planes. The built, relocated
and offline-rebuilt modules each passed 71 supported profiles, 72 refusals
and 18 native commands. A byte-identical IDS replacement refused replay and
preserved the saved result. No rebuild retry was needed. The 25-member ZIP
includes exact zlib source and its notice. Removal preserved seven original
user files and the offline project, and the raw-only ZIP still passed
admission. Continue with the next demonstrated gap. External ICS2 data
references and alternate ICS1 companion naming remain separate I15 profiles.
[Handoff and checks](../build/ics-reader/gzip-candidate/results.json)

## Delivered locally - open detached NRRD scalar volumes with their source identity

pynrrd 1.1.3 wrote calibrated big-endian Int16 NRRD0005 pairs with raw and gzip
payloads. It reopens both; plugin 1.1.0 refused both before export.
[Baseline](../build/nrrd-reader/detached-candidate/baseline.json)
Use the existing reader file-set ABI to discover one same-directory payload
with a safe relative basename and guard both files. Reuse exact attached sample,
calibration and gzip interpretation. Build only the module and affected test
drivers. Verify exact raw Z planes, saved replay, payload replacement refusal,
unsupported file references and unchanged attached behavior before staging.

NRRD 1.2.0 passed 59 supported profiles, 84 refusals and 19 native commands
each on the built, relocated and offline-rebuilt modules. Both producer pairs
exported exactly on both Z planes. Saved replay refused a byte-identical payload
replacement and a header redirected to a second payload. The original and
gzip-only archives passed admission alongside the 22-member current archive.
Removal preserved all eight original user files and the offline result.
The ABI and project drivers reused one candidate cache. No build retry was
needed. Keep multi-file references and general affines in I16 for demonstrated
needs and continue with the next qualified gap.
[Handoff and checks](../build/nrrd-reader/detached-candidate/results.json)

## Delivered locally - read attached gzip NRRD scalar volumes

An independently written, calibrated big-endian Int16 volume with gzip
payload was refused by NRRD 1.0.0 before raw export. The
[baseline](../build/nrrd-reader/gzip-candidate/baseline.json) records that refusal.
Reuse the exact admitted zlib 1.3.2 source in a private static module build.
Keep attached header/axis/endian rules and bound compressed bytes, decoded
bytes and codec workspace before inflating one complete gzip member. Verify
raw bits, Z and calibration, cancellation, corrupt and overlong streams,
then stage the matching-source package and validate its offline rebuild.

NRRD 1.1.0 passed 55 supported profiles, 65 refusal cases and 14 native
commands each on the built, relocated and offline-rebuilt modules. The
22-member Windows archive includes exact zlib source and its notice. The first
removal probe stopped before running because its copied environment used the
wrong key casing. Reading the Windows root from the native environment fixed
it. Removal preserved all six original user files and the offline project
reopened with its retained result. The 1.0.0 archive also passed admission.
The separate 1.2.0 packet covers a single detached payload.
[Handoff and checks](../build/nrrd-reader/gzip-candidate/results.json)

## Delivered locally - verify OME-Zarr 0.5 whole-shard CRC32C

Pinned producers wrote checksummed shards with start and end indexes, gzip
Int16 and transposed Zstandard Float32. Zarr reopens both. The first frozen
CLI probe used positional region bounds; repeat with the documented flags
showed the 1.8.0 provider rejecting both codec chains before export.
Verify the serialized shard in 1 MiB ranges with existing file-version
checks, cancellation and bounded scan work. Use its body length for index
and inner-range interpretation, preserving inner checks. Rebuild only the
optional module and affected native NGFF 0.4 validator. Check exact samples,
corruption outside the selected inner chunk and bounded large shard inputs
before staging a matching-source handoff.

Both producer suites passed 25 commands, including valid multi-slice padding,
corruption outside the requested region, recomputed outer checksums over
damaged inner data and indexes, and oversized shard refusal. All 21 earlier
NGFF 0.5 suites passed 847 commands. The affected native NGFF 0.4 suite passed.
Stage the matching-source archive and verify a relocated install, offline
module rebuild and preservation of user files on removal.

The [1.9.0 Windows handoff](benchmarks/ngff05/README.md#whole-shard-crc32c-in-190)
admitted 160 files. Relocated and offline-built copies each passed 25 new
checks. Removal preserved three user files. Other codec combinations,
index encodings and coordinate layouts remain in I23 for demonstrated needs.

## Delivered locally - bind batch reuse to the observed linked source

The retained runner reused a linked PPM checkpoint after the source was
atomically replaced by a byte-identical file with a different filesystem ID.
The [baseline](../build/gui-aggregation/gap-closure/batch-source-identity/baseline/before.json)
records the reused path and distinct file IDs. Bind checkpoint lookup and
its existing sealed input field to a stable reader observation, including
primary, companion and directory facts. Preserve public project and pixel
digests and historical exports. Validate initial save, stable reuse, fresh
save after a byte-identical replacement, source races, and native regression.
Relink only the changed batch object and reuse the retained test driver.

The candidate saved and then reused a new checkpoint after byte-identical
replacement. The two public digests matched and the sealed observations
differed. The earlier checkpoint stayed byte-exact and exported normally.
Copying it to the new location still failed resume. Five focused CLI cases,
the native batch suite and the previous late-source-write probe passed.
No build retries were needed. Keep other reader and application gaps in the
existing queues. [Evidence](project-pixel-batches.md#checkpoints-and-reuse)

## Delivered locally - refuse batch publication after a linked source changes

On an authored linked PPM batch, the retained CLI published a checkpoint
after the source changed during result serialization. The
[baseline receipt](../build/gui-aggregation/gap-closure/batch-source-publication/baseline/before.json)
identifies the completed run and observed change. Generalize the existing
contour source check to each batch kind at fresh publication and reuse.
Reuse reader directory validation for directory stores and retain observed
file handles through the final operation. Relink only the changed native
batch object, replay the controlled source replacement, and run the retained
native batch suite for normal reuse and unchanged results.

The first candidate still published the changed PPM because project links
report an unopened observation until the first pixel read. Take the same
reader's captured observation after hashing or inference, preserving lazy
open for images without any reads. Relink only the changed batch object.

The Windows probe refused both in-place edits and atomic replacement of the
linked PPM during serialization, with no completed result or scratch left.
Each stable retry saved and reused one checkpoint and exported 1,400 exact
measurements from the revised source. The native batch suite and four dependent
CLI tests passed. Directory validation uses the existing reader contract; this
probe measured the linked-file case. [Evidence](project-pixel-batches.md#checkpoints-and-reuse)

## Delivered locally - seal all persisted batch checkpoint tables before resume

The retained CLI accepted and reused a schema-30 checkpoint after its saved
ROI statistics were deleted. The [baseline receipt](../build/gui-aggregation/gap-closure/batch-checkpoint-seal/baseline/before.json)
captures the deletion's accepted export and resume. Add a versioned full-project
seal and its own resume location. Keep the exact legacy seal for historical CSV
export and preserve prior results while the new runner computes a sealed
checkpoint. Verify legitimate resume, table/row corruption, old export and
preservation. Link only the changed batch/checkpoint objects and affected test
driver against the retained Windows candidate.

The first baseline probe retained a SQLite handle while invoking the exporter.
Close that handle before invoking the CLI to exercise normal file observation.

The first relink used the historical regression driver with a stale nominal
fixture path. Use the previously corrected driver with the same new seal
objects, and run it from the short build directory.

The corrected native batch driver passed ROI, tissue, cell, raw pixel,
contour and interruption/restart checks. Three CLI controls refused a
deleted statistics row, added image metadata, and an unknown table before
export and resume. A clean v2 checkpoint reused exactly. Verified legacy
CSV bytes and preserved the earlier checkpoint while computing and reusing
a v2 result in the same output root. See the
[checkpoint evidence](project-pixel-batches.md#checkpoints-and-reuse).

## Delivered locally - pool TMA checkpoint CSVs without an intermediate file

The retained CLI accepts one native checkpoint CSV and rejects a second
`--measurements` option. Accept repeated inputs for cohort export using the
shared native table parser and file guards. Process documents in argument
order and validate each complete CSV independently, with a shared header.
Hash one header followed by the original row bytes so the result matches
the equivalent combined CSV. Cap total input bytes and refuse aliases or
incompatible headers before publishing. Link only the changed native table,
file loader, CLI and help against the existing Windows candidate. Verify
exact pooled bytes, reversed order, late malformed inputs and legacy routes.

The [local Windows verification](tma-cohorts.md#files-cli-and-desktop) matched
the combined-input summary byte for byte. Reverse order produced the expected
distinct input hash and the same measurements. Duplicate file identity,
different headers, incomplete later records and aggregate byte-cap errors left
the output absent. All 89 focused cases and the relinked native suite passed.
The first candidate build and both validation runs succeeded. The established
single-file SDK interface and CSV schema are unchanged. Continue the next
applicable catalogue outcome from the format or R queue.

## Delivered locally - pool TMA measurements with their native batch image keys

Two authored TMA images have overlapping core IDs and revisions. The retained
CLI rejected an unchanged native batch CSV in cohort pooling, then accepted
the same second-image CSV under the first image's single-grid case summary.
The [baseline receipt](../build/gui-aggregation/gap-closure/tma-batch-pooling/baseline/before.json)
records both outcomes. Teach the shared table parser the batch image key and
check supplied project and single-image IDs where the exporter knows them.
Keep existing CSV formats and byte digests. Reuse the native link inputs for
the affected CLI, table loader and regression driver. Verify real checkpoint
exports, missing/vacant inventory, exact weighted pooling and refusal before
publication for a wrong project, image or core revision.

The first pooling test used binary64 OD in its oracle. The ROI table exports
means of binary32 samples, so round the independent OD values at that
stage and retain the strict weighted-mean check. The native driver reached
its cleanup check from a path longer than the Windows test expects. Reuse
the same executable from the shorter build directory and retain the failed
test tree.

The [local Windows check](tma-cohorts.md#measurement-and-statistics-contract)
accepted untouched batch rows and exactly pooled two captured images with
three vacancies, one ignored missing-core row and two measured cores.
Independent pixel-center counts and binary32 optical-density samples matched
the weighted result. Wrong project/image, inconsistent image aliases and
stale core revisions all failed before publication. The native TMA suite
and 89 focused SDK/CLI cases passed. Real-slide biology and vendor dearraying
remain in R10. Continue the application or eligible reader queue.

## Delivered locally - apply a raw pixel model at its saved sample spacing

The retained standalone export rejects `--model-grid`. A non-unit model
requires callers to compute both rational axes and feature halo by hand.
Resolve phase-zero coverage from the saved raw or calibrated sample spacing
using the existing exact matcher, feature halo and bounded raw area sampler.
Expose the opt-in grid in stream export and fresh contour commands. Compare
explicit and resolved grids on authored TIFFs, including a differently
calibrated image, missing calibration and dimension limits. Reuse the native
link inputs for changed classifier, CLI and focused test objects only.

The first run compared fresh file commitments, which include each run's
provenance header. Both files verified and had identical input and prediction
chains. Compare those chains and counts and keep each file's own commitment.

The calibrated target exposed a native QWS failure with either explicit or
resolved axes. The descriptor still compared training pixel steps even where
prediction matched exact physical spacing. Use the shared sampler check with
the file's already persisted effective calibration on write and reopen. Rebuild
only the changed matcher and QWS descriptor in addition to the prior packet.

The [native batch route](raw-pixel-cli.md#predict-resume-and-verify) matched
explicit grids for rational export and contours. A physically calibrated
target completed one-window resume and verified the exact prefix and new
stream commitment. Missing calibration was refused before output with the
batch argument diagnostic. The focused native classifier, retained native
pixel workflows and 14 selected CLI tests passed. The local SDK still reads
through schema 29, so its legacy bundled-project reopen test was outside
this run. The unavailable-backend test skipped on this enabled build.
Other R8 comparisons and arbitrary resampling remain in the queue.

## Delivered locally - estimate and reuse stains from a saved ROI headlessly

MODEL1 already loads an exact model from an estimator export. The retained
batch CLI rejected `--estimate-stains`; fitting a saved ROI currently needs
the desktop or Python. Add a thin native batch command over the existing
estimator and JSON serializer, with a create-only output and cancellation.
Keep optional PRESET1 parked. Build the batch executable and affected
native stain tests in the existing Python-off cache. Compare the command
against the same SDK request on the synthetic absorber fixture and pass its
JSON into the existing `--stain-model` batch route. Verify repeat identity,
invalid inputs, project/source protection and output preservation.

The first CMake invocation stopped at regeneration because the retained
cache enables an older annotation-plugin kit. Reuse the passing native batch
link recipe, compiling only the changed entry point and affected test drivers.

The [Windows native route](stain-estimation.md#native-batch-cli) passed 31
non-GUI estimator tests and the native estimator/model-file suites. Its
model and sample hashes matched the SDK, the numeric batch route reused
the estimate's result, and native reopen/replay matched the exact saved
basis and measurements. The retained Python SDK predates schema 30, so
the saved-project check uses the same native schema as the batch output.
The shared output guard covers case-insensitive Windows sidecar aliases.
No new wheel or runtime publication was selected. Keep the remaining
registry/specimen work in R7 and inspect the next applicable R8 integration.

## Delivered locally - verify CRC32C on OME-Zarr 0.5 chunk payloads

Pinned producers wrote [checksummed raw bytes](../build/gui-aggregation/catalogue-closure/ngff05/crc32c-regular-producer-01/producer.json),
[gzip shards with pre-compression checksums](../build/gui-aggregation/catalogue-closure/ngff05/crc32c-inner-producer-01/producer.json)
and [transposed Zstandard shards with both checksums](../build/gui-aggregation/catalogue-closure/ngff05/crc32c-double-producer-01/producer.json).
All reopen through Zarr. The retained 1.7.0 module rejected their pipelines
before writing output.

Build decision: reuse the CRC32C implementation already used for shard
indexes. Check the encoded stream before decompression and the decoded
stream before sampling, according to metadata. Include checksum bytes in
the existing work bounds and preserve range reads. Test checksum corruption
and legal sparse fills with independent fixtures. Build only the optional
module and legacy native NGFF validator.

The three checksummed suites passed 125 commands, including a gzip
payload recompressed with a valid gzip trailer but stale decoded CRC32C,
and a damaged encoded CRC32C on a transposed Zstandard shard. Both were
refused before export. The 18 earlier NGFF 0.5 suites passed all 722
commands, and the native NGFF 0.4 suite passed. Stage the matching-source
archive, then run the three new suites from a relocated install and an
offline module-only rebuild. Verify removal preserves user outputs.

The [1.8.0 Windows handoff](benchmarks/ngff05/README.md#chunk-payload-crc32c-in-180)
passed all 125 checks after relocation and again after the offline rebuild.
The first rebuild smoke command assumed a `bin` subdirectory. Running the
retained host from the module install prefix resolved the path without
rebuilding. Removal deleted all 159 owned files and preserved three user
files. Whole-shard payload checksums remain in I23 because they require a
bounded streaming verification contract. Trace R7 against the delivered
MODEL1 evidence before selecting another implementation.

## Delivered locally - read transposed OME-Zarr 0.5 chunks and shards

The pinned producer wrote regular and sharded stores with transpose codecs.
The retained 1.6.0 reader refused both at codec admission. The first attempt
to make a transpose outside a shard failed the producer's divisibility
validation, which checks the inner shape against the original chunk shape.
Use a C/Y permutation whose inner shape divides both original and permuted
shapes, while keeping distinct logical and encoded index ordering.

The corrected [outer-transpose producer](../build/gui-aggregation/catalogue-closure/ngff05/transpose-outer-producer-02/producer.json)
reopened through the independent Zarr reader and the retained 1.6.0 module
refused it. The other inputs are the [regular](../build/gui-aggregation/catalogue-closure/ngff05/transpose-regular-producer-01/producer.json)
and [inner-shard](../build/gui-aggregation/catalogue-closure/ngff05/transpose-inner-producer-01/producer.json)
producer stores.

Build decision: preserve the logical chunk grid and derive sample and shard
index strides from the declared permutations. Reuse the existing bounded
codec and byte-range decoders. Check physical chunk and shard order against
producer arrays, including noncommuting permutations and malformed orders.
Build only the optional plugin and the native NGFF 0.4 validator.

The new regular, inner and outer transpose suites passed all 131 commands.
They compared exact logical samples and calibrated geometry, tested an
equivalent chain with two transposes, and rejected malformed orders and
truncated payloads. The prior suites passed 591 checks and the legacy
native suite passed. Run all three new suites on a relocated install and
an offline module rebuild using the same retained host.

The [1.7.0 Windows handoff](benchmarks/ngff05/README.md#transpose-codec-in-170)
admitted 159 files. The relocated install and module rebuild each passed
all 131 new checks. Removal preserved three user files. Other codecs,
sample types and coordinate layouts remain in I23 for demonstrated needs.

## Delivered locally - honor OME-Zarr 0.5 channel display metadata

The [pinned producer](../build/gui-aggregation/catalogue-closure/ngff05/omero-display-producer-01/producer.json)
wrote two named channels, custom RGB colors and contrast windows in the
standard `ome.omero` metadata. The [retained render](../build/gui-aggregation/catalogue-closure/ngff05/omero-display-frozen.pam)
ignored the palette and windows, showing a generic green channel instead.

Build decision: interpret the linear color display settings in the native
NGFF 0.5 server, with bounded tiled composition. Keep the existing host
plane because the reader ABI cannot publish a different default plane to
analysis consumers. Verify exact raw names, samples and planes separately
from RGBA, including overlap, inactive channels, and explicit display
errors for unsupported settings. Rebuild only the optional plugin and
the legacy native NGFF validator.

The named producer's 45 commands passed, including exact RGB overlap,
activation and inversion, label-only fallback, invalid-palette refusal,
and raw reads after a display error. Existing NGFF 0.5 suites passed 546
commands and the native NGFF 0.4 suite passed. Verify the staged package
from a relocated install and offline module rebuild with the same CLI.

The [1.6.0 Windows handoff](benchmarks/ngff05/README.md#producer-channel-display-in-160)
admitted 159 files. The relocated module and offline rebuild passed all
45 display and raw checks. Removal preserved all three user files.
Additional rendering models and producer default-plane selection remain I23.

## Delivered locally - read signed Int16 OME-Zarr 0.5 images

Pinned writers generated calibrated signed data spanning the Int16 range in
[regular Zstandard](../build/gui-aggregation/catalogue-closure/ngff05/int16-regular-producer-01/producer.json),
[big-endian gzip shards](../build/gui-aggregation/catalogue-closure/ngff05/int16-sharded-producer-01/producer.json)
and [big-endian bytes shards](../build/gui-aggregation/catalogue-closure/ngff05/int16-bytes-producer-01/producer.json).
The retained 1.4.0 provider refused all three before writing output.

Build decision: admit exact Int16 storage and signed fill through the
existing typed sampler and reader ABI. Verify negative values and both
endpoints, byte order, sparse fills, calibrated axes and signed-range
display. Reuse the earlier codec and dtype checks, and rebuild only the
module, the native NGFF 0.4 validator and one UInt16-only bridge consumer.

The three signed suites passed 117 commands and the existing UInt8, UInt16
and Float32 suites passed 429. The native NGFF 0.4 suite and all 60 standalone
N5 checks passed. Stage the exact signed and regression receipts, then verify
the three signed profiles with the relocated module and an offline rebuild.

The [1.5.0 Windows handoff](benchmarks/ngff05/README.md#int16-images-in-150)
admitted 158 files. The relocated install and offline rebuild passed all
117 signed checks. Removal preserved three user files. Other sample types,
codecs and coordinate layouts remain in I23 for demonstrated workflows.

## Delivered locally - read UInt8 OME-Zarr 0.5 images

Pinned writers produced [regular Zstandard](../build/gui-aggregation/catalogue-closure/ngff05/uint8-regular-producer-01/producer.json),
[gzip shards](../build/gui-aggregation/catalogue-closure/ngff05/uint8-sharded-producer-01/producer.json)
and [bytes-only shards](../build/gui-aggregation/catalogue-closure/ngff05/uint8-bytes-producer-01/producer.json).
The byte serializers correctly omit endianness. The retained 1.3.0 provider
refused each store before export because it admits only UInt16 and Float32.

Build decision: admit native UInt8 in the optional module, reuse the bounded
sampler and leave the reader ABI unchanged. Verify exact byte outputs, fill
limits and RGBA projection through each codec. Reuse the existing layouts
and regression fixtures, including the legacy native NGFF reader and one
UInt16-only module that shares the typed ABI bridge.

The first harness compared the NPY sample byte count with the CLI's
in-memory payload count. The latter includes one validity byte per pixel.
Correct the assertion, keep the independent NPY dtype and sample checks,
and rerun against the same binaries in fresh receipt directories.

The corrected harness passed 123 UInt8 checks across the three producer
profiles and 306 existing UInt16 and Float32 checks. The native NGFF 0.4
suite and 60 standalone N5 checks passed. Stage the package with the exact
receipts and verify the three UInt8 profiles after relocation and an
offline module-only rebuild.

The [Windows 1.4.0 handoff](benchmarks/ngff05/README.md#uint8-images-in-140)
admitted 158 files. The relocated installation and offline module rebuild
passed all 123 UInt8 checks and removal preserved three user files. Other
sample types, codecs and coordinate layouts remain I23 candidates.

## Delivered locally - read bytes-only and gzip OME-Zarr 0.5 stores

Pinned ome-zarr and zarr-python wrote regular and indexed-shard stores
with uncompressed bytes or gzip. The [bytes regular](../build/gui-aggregation/catalogue-closure/ngff05/bytes-regular-producer-01/producer.json),
[bytes shard](../build/gui-aggregation/catalogue-closure/ngff05/bytes-sharded-producer-01/producer.json),
[gzip regular](../build/gui-aggregation/catalogue-closure/ngff05/gzip-regular-producer-01/producer.json)
and [gzip shard](../build/gui-aggregation/catalogue-closure/ngff05/gzip-sharded-producer-01/producer.json)
receipts record the exact layouts and decoded samples. The retained 1.2.0
provider refused all four as it requires Zstandard.

Build decision: reuse the existing exact bytes decoder and bounded gzip
decoder with the current Zarr 3 indexed-shard and region authorities.
Verify UInt16 and big-endian Float32 against the producer at both levels,
exercise fill and malformed payloads or gzip metadata, and regress the
Zstandard profiles and native NGFF 0.4. Build only the affected module and
native validation target.

The four new fixture suites passed 154 commands. The four Zstandard
suites passed 152 commands, and the native NGFF 0.4 suite passed. The
new cases checked each payload against independently decoded bytes and
covered truncated, oversized and empty chunks, CRC failures and invalid
gzip metadata. Stage the new version and rerun the four new profiles
after relocation and an offline module-only rebuild.

The [1.3.0 Windows handoff](benchmarks/ngff05/README.md#bytes-only-and-gzip-chunks-in-130)
admitted 158 files. The relocated install and offline module rebuild each
passed all 154 new checks. Removal preserved all three user files. Other
codecs and coordinate layouts remain in I23 for demonstrated workflows.

## Delivered locally - read Float32 OME-Zarr 0.5 images

The reader ABI carries pixel spacing and pyramid scales but no per-view
world affine. Keep general BDV affine registration in I24 until the native
frame contract covers measurement, export and replay.

The pinned upstream writer made a calibrated Float32 OME-Zarr 0.5 image
with signed samples and signed zero. The [regular producer receipt](../build/gui-aggregation/catalogue-closure/ngff05/float32-regular-producer-01/producer.json)
and the retained [1.1.0 refusal](../build/gui-aggregation/catalogue-closure/ngff05/float32-regular-frozen.log)
show a concrete I23 gap. The first sharded probe split a single option into
characters. Passing it directly generated the [sharded producer store](../build/gui-aggregation/catalogue-closure/ngff05/float32-sharded-producer-01/producer.json),
which the [same runtime refused](../build/gui-aggregation/catalogue-closure/ngff05/float32-sharded-frozen.log).

Build decision: reuse the native Zarr sampler, typed raw storage and
existing reader ABI. Add Float32 to the optional NGFF 0.5 provider and
exercise regular and indexed-shard stores. Compare raw IEEE bits, fills,
clipping and [0,1] display with independent oracles. Keep the legacy
NGFF 0.4 and UInt16 profiles covered by their existing checks. Build only
the affected plugin and native validation target.

The regular and indexed-shard Float32 fixtures passed 38 commands each,
including signed zero, decimal rounding above a binary32 midpoint, exact
nonfinite fill bits and display transparency. The UInt16 suites passed 30
regular and 46 shard commands, and the native NGFF 0.4 suite passed. Build
one representative UInt16-only store provider because its shared ABI
bridge now also instantiates the NGFF typed reader. Verify the staged
NGFF module in a relocated install and module-only rebuild.

The representative N5 build passed all 60 checks. The [1.2.0 Windows handoff](benchmarks/ngff05/README.md#float32-images-in-120)
admitted 158 files, passed both 38-command suites after relocation and an
offline module rebuild, and preserved three user files during removal.
Other NGFF layouts and general BDV affine registration remain candidates.

## Delivered locally - read available channels in BDV projects with missing views

Pinned pybdv produced independent sample oracles for the available views in
[N5](../build/gui-aggregation/catalogue-closure/bdv/missing-n5-producer-01/producer.json)
and [partitioned HDF5](../build/gui-aggregation/catalogue-closure/bdv/missing-hdf5-producer-01/producer.json).
The first channel is absent at timepoint 1. The independently read fixtures
contain no cell dataset for that pair, and the HDF5 XML has no partition
mapping for it. The retained [N5](../build/gui-aggregation/catalogue-closure/bdv/missing-n5-frozen.log)
and [HDF5](../build/gui-aggregation/catalogue-closure/bdv/missing-hdf5-frozen.log)
modules refused access to the second channel, which is present.

Build decision: parse explicit missing-view pairs in the shared XML reader,
keep the existing axes and use the first available view to establish level
extents. Check selected channels before sampling. The reader ABI provides
one validity mask per region, so any request including a missing channel
must fail explicitly. Preserve the distinction from a sparse zero-filled
block. Rebuild only the two modules and regress standalone N5 because its
compiled source also holds the BDV N5 sampler.

All 268 prior BDV checks and 60 standalone N5 checks passed. The first
missing-view harness expected the raw route's refusal status for a display
read. The CLI display route correctly returned status 1 and the specific
missing-view error. Check that status and absence of output, and rerun
only the two missing-view checks.

The rerun passed 42 N5 and 58 HDF5 checks, including explicit refusal of
missing data even when storage remains, preserved reads for another
channel, and rejection of undeclared gaps. Verify the two packages from
relocated installs and offline module builds using the same CLI and
fixtures. Retain the earlier package versions under their original
identities.

The [N5 1.3.0](benchmarks/bdv-n5/README.md#declared-missing-views-in-130) and
[HDF5 1.4.0](benchmarks/bdv-hdf5/README.md#declared-missing-views-in-140)
Windows kits admitted 126 and 138 files. Relocated installs and offline
module rebuilds passed the full fixture checks. Removal preserved the
three user files per install. Distinct tiles or angles and general affine
registration remain I24 workflows.

## Delivered locally - read BDV channels with shared acquisition roles

Pinned pybdv wrote [N5](../build/gui-aggregation/catalogue-closure/bdv/roles-n5-producer-01/producer.json)
and [partitioned HDF5](../build/gui-aggregation/catalogue-closure/bdv/roles-hdf5-producer-01/producer.json)
projects with two distinct channels acquired at one tile, angle and illumination.
Independent reads recovered both pyramid levels. The exact retained
[N5](../build/gui-aggregation/catalogue-closure/bdv/roles-n5-frozen.log) and
[HDF5](../build/gui-aggregation/catalogue-closure/bdv/roles-hdf5-frozen.log)
plugins refused both because they expect one channel catalogue.

Build decision: update only the shared XML admission and the two optional
modules using the retained host. Require identical declared acquisition roles
for every channel and retain exact geometry and registration checks. Verify
raw samples, display and partition routes, and refuse changed roles, unknown
IDs and nonzero tile locations or rotations. Recheck the existing profiles.

The first module build caught a string-view argument passed to the existing
owning UTF-8 validator. Convert the bounded role name explicitly and rebuild
only the affected module objects.

The rebuilt modules passed 38 N5 and 53 HDF5 acquisition-role checks.
All 177 existing single-setup, multichannel and partition checks passed.
Stage new module versions, then verify relocated installs and offline
module rebuilds with the same retained CLI and producer oracles.

The N5 1.2.0 and HDF5 1.3.0 Windows packages admitted 126 and 138 files,
respectively. Both relocated installs and both module rebuilds passed their
full integration checks. Removal preserved all three user files per install.
The [N5](benchmarks/bdv-n5/README.md#shared-acquisition-roles-in-120) and
[HDF5](benchmarks/bdv-hdf5/README.md#shared-acquisition-roles-in-130)
receipts link the exact artifacts. Missing-view support is covered by the
newer profiles above.

## Delivered locally - read aligned partitioned BigDataViewer HDF5 projects

The [partitioned fixture](../build/gui-aggregation/catalogue-closure/bdv/h5-partitioned-01/producer.json)
uses pinned pybdv pixels and a synthetic master/partition layout derived from
the upstream writer. h5py independently read both levels back through the master
links. The retained 1.1.0 runtime refused it without output. The first probe
passed a relative manifest path and stopped before reaching the reader. The
[absolute-path probe](../build/gui-aggregation/catalogue-closure/bdv/h5-partitioned-frozen-absolute.log)
reached the reader and confirmed the unsupported XML element.

Build decision: extend the shared XML admission and the HDF5 plugin routing.
Require complete, disjoint XML view maps, exact master-link agreement and
guarded direct reads of admitted local partition files. Reuse the existing
UInt16 block decoder, directory identity and retained CLI. Check remapped
channels and times against the independent oracle, exercise malformed maps
and link targets, and run the legacy HDF5 and N5 regressions.

The first HDF5 test run stopped at catalog admission because the compiled
descriptor still reported 1.1.0 alongside the new 1.2.0 manifest. Align the
descriptor and relink only the HDF5 module. The N5 single-setup regression
already passed its 35 commands.

With the descriptor corrected, 42 partitioned and 69 unpartitioned HDF5
commands passed. Upstream writes relative HDF5 object names and can emit
Windows separators in XML. Extend the fixture and admission to cover those
spellings, compare both byte orders, and exercise same-time channels split
across separate files. Repeat the affected plugin checks before staging.

The final HDF5 runtime passed 44 partitioned, 40 single-setup and 29
unpartitioned multichannel commands. The two fixture generations agree
on the oracle hashes. Verify the staged ZIP in a relocated install and
one offline module-only build, using the same CLI and partition fixture
to catch packaging or dependency regressions.

The N5 regressions passed 35 single-setup and 29 multichannel commands.
The [Windows ZIP](../build/gui-aggregation/catalogue-closure/bdv/qupathxx-bdv-hdf5-1.2.0-windows-x64.zip)
admitted all 138 files. The relocated install and offline rebuild each
passed all 44 partition checks. Package-owned removal preserved all three
user files. The [delivery receipt](benchmarks/bdv-hdf5/README.md#local-hdf5-partitions-in-120)
links exact artifacts and validation. Missing views, tiled and angle setups,
and general affine registration remain I24 candidates for a demonstrated need.

## Delivered locally - read aligned BigDataViewer channel setups

The pinned pybdv writer generated a two-channel project in both
[N5](../build/gui-aggregation/catalogue-closure/bdv/n5-multiset-02/producer.json)
and [HDF5](../build/gui-aggregation/catalogue-closure/bdv/h5-multiset-02/producer.json).
It wrote setup 7 before setup 2 and gave each a distinct channel ID. The first
generator run tried to audit nonexistent setup 0. The corrected receipt records
both setup IDs, registrations and independent sample arrays. The
[retained 1.0.0 modules](../build/gui-aggregation/catalogue-closure/bdv/multiset-retained-readers.json)
refused both inputs before creating any output.

Build decision: update the two optional modules and their shared XML admission.
Reuse the data block decoders, directory-store identity and exact retained CLI.
The decisive checks are channel order by explicit setup ID, interleaved and
selected UInt16 reads at both times/levels, proper RGB, and refusal of
inconsistent geometry or non-channel setups. Run both single-setup profiles
as regressions and rebuild only the standalone N5 module to check its shared
sampler. Preserve the frozen 1.0.0 handoffs.

Both modules passed 29 multi-setup commands. Single-setup regressions passed
35 N5 and 40 HDF5 commands, and the standalone N5 reader passed 60 commands.
Staging admitted 126 N5 and 138 HDF5 archive members. Verify the packaged DLLs
from relocated installs and rebuild only the two modules from those exact
source kits. Use the same CLI and pixel oracles to catch missing inputs or
dependencies. Finish with package-owned removal that preserves user files.

Both relocated installs and both module rebuilds passed all 29 runtime checks.
The first removal check correctly removed only ZIP members. Its strict final
file comparison also found bytecode caches created by running the development
checker inside the install tree. Use fresh removal-only extractions so the
user-file check has exactly the three intended inputs. Retain those caches
and the original verification results as evidence.

Fresh removal verified all 126 N5 and 138 HDF5 archive members and preserved
three user files per install. See the [N5 delivery](benchmarks/bdv-n5/README.md#aligned-channel-setups-in-110)
and [HDF5 delivery](benchmarks/bdv-hdf5/README.md#aligned-channel-setups-in-110)
for the download, exact runtime, source and removal evidence. Partitioned
HDF5 is covered separately in 1.2.0 above.

## Delivered locally - open an aligned BigDataViewer HDF5 project

The [pybdv HDF5 export](../build/gui-aggregation/catalogue-closure/bdv/h5-producer-01/producer.json)
uses `bdv.hdf5`, a setup-level XYZ resolution/subdivision table and signed
Int16 storage for legacy unsigned samples. The bit pattern for 32768 is
stored as -32768. The [retained N5 plugin](../build/gui-aggregation/catalogue-closure/bdv/h5-retained-reader.json)
refused the HDF5 loader without writing an output. The raw oracle reopens
the producer output and records the original unsigned interpretation.

Build decision: compile a separate native BDV HDF5 plugin. Reuse the
bounded XML parser, directory identity, HDF5 lease and existing region
CLI. The decisive checks compare unsigned samples above 32767 across two
times and levels, detect layout/filter/registration refusals and preserve
the selected XML/HDF5 source. The changed XML entry point also needs
the existing BDV N5 regression.

The [40 HDF5 checks](../build/gui-aggregation/catalogue-closure/bdv/h5-integration-01/results.json)
and [35 BDV N5 checks](../build/gui-aggregation/catalogue-closure/bdv/h5-n5-regression-01/results.json)
passed. The [137-member Windows ZIP](../build/gui-aggregation/catalogue-closure/bdv/qupathxx-bdv-hdf5-1.0.0-windows-x64.zip)
passed admission with the exact HDF5 headers from the retained Loom handoff.
The [extracted module](../build/gui-aggregation/catalogue-closure/bdv/h5-deployment-01/results.json)
and [offline module rebuild](../build/gui-aggregation/catalogue-closure/bdv/h5-rebuild-check-01/results.json)
each passed 30 runtime checks using the same CLI. [Removal](../build/gui-aggregation/catalogue-closure/bdv/h5-uninstall-01.json)
preserved all three user files. Multiple setups, HDF5 partitions and other
registrations remain separate I24 workflows.

## Delivered locally - open an aligned BigDataViewer N5 project

The pinned pybdv 0.6.0 and z5py 3.0.2 writer produced a
[61-member project](../build/gui-aggregation/catalogue-closure/bdv/n5-producer-01/producer.json)
with two times, two levels, XML calibration and dataset axes implicit in the
BDV profile. Independent generation reproduced the exact member hashes and
both sample arrays. The [frozen standalone N5 reader](../build/gui-aggregation/catalogue-closure/bdv/legacy-reader.json)
refused a level because it lacks TensorStore axis metadata. An initial
translated-view fixture used integer transform entries. The writer requires
floats and the [corrected fixture](../build/gui-aggregation/catalogue-closure/bdv/n5-shifted-02/producer.json)
preserves the requested translation.

Build decision: compile only the optional BDV N5 module using the admitted
XML parser and existing native N5 decoder, store inventory and region CLI.
The decisive run checks exact raw samples at both times and levels, calibrated
pixel geometry and source identity, and refusal of an unsupported affine or
unsafe companion path.

The [35 BDV commands](../build/gui-aggregation/catalogue-closure/bdv/integration-01/results.json)
and [60 standalone N5 regression commands](../build/gui-aggregation/catalogue-closure/bdv/standalone-regression-01/results.json)
passed. The
first stage hit a compiler-input variable shadow in the shared packager.
The corrected stage admitted all [125 ZIP members](../build/gui-aggregation/catalogue-closure/bdv/qupathxx-bdv-n5-1.0.0-windows-x64.admission.json).
The [relocated install](../build/gui-aggregation/catalogue-closure/bdv/deployment-01/results.json)
and [offline module rebuild](../build/gui-aggregation/catalogue-closure/bdv/rebuild-check-01/results.json)
each passed 35 commands using the exact retained SDK and CLI.
[Removal](../build/gui-aggregation/catalogue-closure/bdv/uninstall-01.json)
preserved three user files. The standalone N5 ZIP retained its digest.
HDF5-backed BDV and arbitrary registered views remain open under I24.

## Delivered locally - open an explicit TensorStore N5 dataset

The pinned TensorStore 0.1.86 driver wrote and independently reread
[three calibrated UInt16 datasets](../build/gui-aggregation/catalogue-closure/n5/producer-01/producer.json).
The raw, gzip and zlib-header gzip forms all use explicit XYZCT axes, mode-0
big-endian blocks and an absent fill block. The producer writes full-sized
corner block headers even when the image edge is partial. The
[retained native CLI](../build/gui-aggregation/catalogue-closure/n5/retained-reader.json)
has no N5 provider and refused the input through the existing Zarr route.

Build decision: compile the optional N5 provider and the existing native
directory inventory/guard with bounded member reads. Reuse the admitted SDK,
zlib and source-store CLI. The decisive run compares typed XYZCT regions for
all three producer profiles, verifies physical calibration, holes and clipped
edges, and refuses changed files or malformed metadata/blocks.

The [60 native region commands](../build/gui-aggregation/catalogue-closure/n5/integration-01/results.json)
and the [directory-store suite](../build/gui-aggregation/catalogue-closure/n5/directory-tests-01.log)
passed. The suite exercised Windows junctions and skipped the file symlink case
due to the runner's privilege. The [120-member Windows ZIP](../build/gui-aggregation/catalogue-closure/n5/qupathxx-n5-1.0.0-windows-x64.zip)
passed exact admission. The relocated install and offline module rebuild each
passed all 60 commands using the retained CLI. Removal preserved three user
files. See the [N5 receipt](benchmarks/n5/README.md) for the remaining I24 work.

The N5/NGFF UInt16 bridge extraction also changed the active NGFF wrapper.
Its [30 regular](../build/gui-aggregation/catalogue-closure/n5/bridge-ngff-regular-01/results.json)
and [46 sharded](../build/gui-aggregation/catalogue-closure/n5/bridge-ngff-sharded-01/results.json)
checks passed with the retained CLI and one rebuilt translation unit. The
admitted 1.1.0 NGFF package retained its digest.

## Delivered locally - import default AnnData Zarr 2 exports

Provider 1.1.0 imports the AnnData 0.12.7 / Zarr 2.18.7 writer's default
Blosc/LZ4 and explicit BloscLZ output. The provider-only build reused the admitted
C-Blosc static decoder. The [32 integration commands](../build/gui-aggregation/catalogue-closure/anndata-zarr/integration-blosc-01/results.json)
pass exact counts and IDs, compressed and direct-copy frames, truncated-frame
refusal, project reopen and exact legacy schema 4/5 comparisons.

The first installed smoke test added an owned copy beside the shared input
project. A private project copy isolated that expected write. The fixed test
restaged the source without rebuilding the provider. [All 11 deployment checks](../build/gui-aggregation/catalogue-closure/anndata-zarr/deployment-blosc-02/results.json)
pass, including reopening the default-Blosc project after the input and plugin
were removed from their original locations. Removal preserved 12 user files.
The [294-member Windows ZIP](../build/gui-aggregation/catalogue-closure/anndata-zarr/qupathxx-anndata-zarr-provider-1.1.0-windows-x64.zip)
passed [material and exact membership checks](../build/gui-aggregation/catalogue-closure/anndata-zarr/zip-blosc-final-admission.json).
The previous 285-member package and ZIP still match their recorded digests.

## Delivered locally - write sparse AnnData CSR/CSC directly to QMX

Provider 1.2.0 uses matrix ABI4 and `external-sparse-store/1` to stream stored
CSR/CSC entries into the existing schema 6 QMX writer. The [35-command integration](../build/gui-aggregation/catalogue-closure/anndata-zarr/integration-sparse-02/results.json)
includes 146 native checks, explicit continuous signed zero, exact ABI3 reuse,
and sparse `matrix-attach` recovery. The measured 1,024 × 1,024 case retained
all 1,024 entries and dropped from 22,810,624 to 331,776 bytes. A 10,240 ×
10,240 CSC store passed with 10,240 entries across three pages.

The first isolated build lacked the shared R11 attachment unit. Linking it
and updating the SDK's ABI3 preselection resolved both failures using the
same cache. Relocated SDK configuration and [13 deployment commands](../build/gui-aggregation/catalogue-closure/anndata-zarr/deployment-sparse-01/results.json)
passed, including sparse project reopen after removal. Removal preserved 15
user files. The [295-member Windows ZIP](../build/gui-aggregation/catalogue-closure/anndata-zarr/qupathxx-anndata-zarr-provider-1.2.0-windows-x64.zip)
passed [material and exact membership checks](../build/gui-aggregation/catalogue-closure/anndata-zarr/zip-sparse-final-admission.json).

## Delivered locally - run a versioned pixel batch recipe

The [versioned RGB recipe](project-pixel-batches.md#versioned-recipe-file)
pins whole project/model files and explicit image/ROI IDs. The native API and
`qupathxx_batch --pixel-batch-recipe` reuse the existing capture, inference,
QPS/CSV export, checkpoint and reopen paths. The [native check](../build/gui-aggregation/gap-closure/pixel-batch-recipe/native-workflow.log)
interrupted after one of two images, then reused it without inference and
reopened both saved projects. [Nine CLI checks](../build/gui-aggregation/gap-closure/pixel-batch-recipe/results.json)
cover fresh and resumed runs, CSV export, project/model replacement, duplicate
JSON, and both failure policies when one checkpoint is corrupt. The batch engine
and published runtimes were reused. The local build compiled the new native unit,
CLI entry and test. Normal root configuration passed with Python disabled.

## Delivered locally - reclaim abandoned batch scratch under the output lock

Restart reclaims marked project and stream scratch under the existing exclusive
output lock. It preflights each directory, preserves completed bundles and
unexpected contents, and skips links. The [forced-termination test](../build/gui-aggregation/gap-closure/batch-scratch-recovery-02/crash-recovery.json)
used synthetic scratch next to a real running batch. A contender refused
without writes. After termination, the next batch reclaimed the scratch,
preserved foreign and mixed contents and a junction, and reused the exact first
checkpoint before finishing the second image. The [native batch suite](../build/gui-aggregation/gap-closure/batch-scratch-recovery-02/batch-regression-final.log)
passed after rebuilding its nominal fixture locator for this checkout.

The first scratch test exposed a temporary path above the Windows length limit.
Removal now uses the existing extended-path helper. The first regression
harness run pointed to a retired fixture checkout, and its own cleanup needed
the normal, shallower build working directory.

## Delivered locally - preserve queued viewer work and seal shutdown

The [retained viewer reproduced](../build/gui-aggregation/gap-closure/viewer-queue/viewer-baseline.json)
an accepted command failing behind a held activation. The owner now leaves it
queued and charged until a worker is free, and prioritizes an admitted close
while work is active or paused. Shutdown seals intake before waiting, cancels
queued tickets and wakes observers. The [affected native suite](../build/gui-aggregation/gap-closure/viewer-queue/viewer-fixed.json)
passed queue capacity, close priority, shutdown intake, waiter cancellation and
retained pixels after take/close. Five [retained Python checks](../build/gui-aggregation/gap-closure/viewer-queue/python-retained.log)
passed ownership and lifecycle on the existing binding build. No binding or
published runtime rebuild was needed for the changed native checks.

## Delivered locally - preserve committed raw exports during viewer shutdown

The [retained viewer](../build/gui-aggregation/gap-closure/viewer-raw-export/viewer-baseline.json)
lost an already-published QWS outcome at shutdown. The native save job records
atomic publication before subsequent verification or live retention. The
[fixed viewer suite](../build/gui-aggregation/gap-closure/viewer-raw-export/viewer-fixed.json)
verified complete and partial files, detached path/commit state on close,
owner failure, explicit cancellation and workspace replacement, and refusal
when an existing destination cannot be replaced. No detached export adds
a project record. The [pixel workflow suite](../build/gui-aggregation/gap-closure/viewer-raw-export/pixel-workflows.json)
also passed owner staleness and a failed-request retry without carrying the
previous publication flag.

The first fixed run passed the export cases but leaked their workspace epoch
into a separate historical-read test. The corrected harness rebuilt the test
and reused all three changed production units.

## Delivered locally - enforce annotation plane compatibility in RGB prediction

The retained classifier already validates ordered feature schema, natural grid
compatibility and raw missing support. The [retained prediction build](../build/gui-aggregation/gap-closure/pixel-plane-r8/baseline.json)
accepted an ROI on the wrong display plane. RGB prediction now calls the
existing native training/display guard. The [pixel workflow suite](../build/gui-aggregation/gap-closure/pixel-plane-r8/fixed.json)
verified pre-read refusal for mismatched Z/time, an undisplayed channel and
a channel-specific ROI on a mixed composite. Matching and all-channel ROI
prediction passed. An actual TIFF project batch rejected the mixed composite
without writing a QPS or checkpoint project. Matched-grid replay, streamed
seams and archive tests passed with existing model formats and kernels.
Only the prediction capture unit and the affected test were built.

## Delivered locally - default OME-Zarr 0.5 writer profile

The pinned upstream writer produced a two-level TCZYX UInt16 image. The retained
native reader refused it before decoding, since the store has `zarr.json`
instead of `.zgroup`. The [I23 producer audit](benchmarks/ngff05/README.md)
records the exact chunk codec, axis calibration, and reduced-level pixel-center
translation the plugin must handle. The isolated producer install used `uv`
because the project's virtual environment lacks `pip`.

Build decision: compile only the optional plugin wrapper, the affected shared
NGFF source units and the pinned Zstandard decoder in the I23 cache. Reuse the
retained SDK and store-capable CLI. The decisive check is exact UInt16 regions
at both levels against the upstream arrays, including registration and malformed
chunk refusal. The fixture audit required no product build.

The first module built, but the host refused its empty dependency manifest.
PE inspection found the SDK's OpenSlide import through the shared image server.
Reuse the label plugin's exact dependency-graph staging and relink the module
with the corrected manifest command. Retain the first build and refusal logs.
The retained QOI CLI predates directory-store ABI negotiation. Build the region
entry and raw export adapter against the already-built store-aware native
overlay, then run with the plugin and its exact dependency in one directory.
That host loaded the plugin and exposed a raw export gap. The native exporter
now uses the plugin's retained directory snapshot, verifies currentness at
commit and protects the full store root. The [30 installed CLI commands](../build/gui-aggregation/catalogue-closure/ngff05/integration-05/results.json)
passed exact C/Z/T regions, both levels, odd origins, calibrated metadata,
RGBA, declared fill, corrupt/refused inputs and outside-store publication.
The [existing TIFF/PAM export suite](../build/gui-aggregation/catalogue-closure/ngff05/legacy-region-export.log)
passed its exact array/schema, protection and cancellation checks.

The [NGFF0.4-only build](../build/gui-aggregation/catalogue-closure/ngff05/legacy_ngff_source_tests.log)
passed existing dtype, transform, codec and source guard tests. Its isolated
harness needed the existing fixture definition in slash-normalized form.
The [155-member Windows ZIP](../build/gui-aggregation/catalogue-closure/ngff05/qupathxx-ngff05-1.0.0-windows-x64.zip)
passed exact material and archive checks. The first stage refused the generated
SDK ABI header until its existing source derivation and exact SDK digest were
bound. Both the [relocated install](../build/gui-aggregation/catalogue-closure/ngff05/deployment-01/results.json)
and the [offline module/CLI rebuild](../build/gui-aggregation/catalogue-closure/ngff05/rebuild-check-01/results.json)
passed all 30 commands. The build reused the exact admitted SDK and store host
archive. [Removal](../build/gui-aggregation/catalogue-closure/ngff05/uninstall-01.json)
removed only the 155 package-owned files and preserved three user files.
This is a local handoff for the declared profile. Shards and other Zarr 3
representations remain in the format queue.

## Delivered locally - indexed OME-Zarr 0.5 shards

The pinned direct writer produced a [23-member sharded store](../build/gui-aggregation/catalogue-closure/ngff05/sharded-producer-01/producer.json)
with UInt16 bytes/Zstandard inner chunks, a little-endian CRC32C index and an
omitted all-fill inner chunk. A shard spans both channels and multiple XY
chunks. The [frozen 1.0.0 CLI route](../build/gui-aggregation/catalogue-closure/ngff05/sharded-baseline.json)
refused the sharding codec without creating an output.

Build decision: compile only the optional provider and affected shared NGFF
units in the same cache. Keep the legacy archive and use the existing region
CLI. The decisive comparisons are original typed samples across both inner
chunk and shard boundaries, correct sparse fill, and hard refusal of corrupt
index checksums, malformed ranges and damaged inner frames. Each read fetches
only an index and the requested inner byte ranges.

The [46 shard cases](../build/gui-aggregation/catalogue-closure/ngff05/sharded-integration-02/results.json),
[30 regular cases](../build/gui-aggregation/catalogue-closure/ngff05/sharded-regular-02/results.json)
and [NGFF0.4 regression](../build/gui-aggregation/catalogue-closure/ngff05/sharded-legacy.log)
passed. The fixture/receipt marker update reran the small CLI checks without
a product build. The [157-member Windows ZIP](../build/gui-aggregation/catalogue-closure/ngff05/qupathxx-ngff05-1.1.0-windows-x64.zip)
passed archive admission. The relocated install and offline module rebuild
each passed 46 shard checks using the exact retained CLI. Removal preserved
three user files. The frozen 1.0.0 ZIP still matches its recorded SHA-256.
Other codec/layout work requires a new demonstrated input gap.

## Delivered locally - R11 attach a completed QMX after a project save failure

The [matrix-attach route](benchmarks/matrix-attachment/README.md) accepts the
exact completed QMX hash, checks the retained pixel frame against the selected
image, and saves a new project with owned matrix data. Sixteen native commands
passed. For both domains, a real save failure left an intact QMX. A separate
retry and fresh reopen succeeded with the original store, positions and provider
unavailable. Exact digest/frame/receipt, input aliases and existing output
protection passed. The cached build compiled only the new attachment authority
and changed CLI above the exact M07 package archive.
The first configure found a live Loom alias with the same name as the retained
SDK target. R11 now imports the exact M05 archive under a private target name.
The original configure log and recipe remain in the packet.

## Delivered locally - M07 native AnnData Zarr directory matrix workflow

User outcome: import an explicitly selected local AnnData Zarr2 X/layer and its
native observation/feature IDs, join complete native position TSV in an explicit
image frame, publish exact typed QMX, then keep/save/reopen an owned project.
The previous file-only matrix boundary cannot observe a directory or bind its
metadata/chunk members; pointing it at .zattrs would hide semantic inputs.

New matrix ABI3 is separate (96-byte open/56-byte table), reusing typed ABI2
metadata/axis/value pages. Native options explicitly select primary metadata;
the host observes ALL regular members, hashes complete canonical keys/lengths/
content with bounded streaming and retains a distinct filesystem observation.
Source mutation/escape/cancel/budget checks and three existing native writer
publication callbacks protect the source and new-only QMX. Public legacy
DTO/vtable/options layouts and ABI1/2 schema4/5 identities stay exact. Schema6
has its own profile/recipe/source hash and persisted content/observation receipt.
The third matrix input hash remains the ordered observation-axis digest; position
file SHA is separately bound in the recipe, never equated to that axis digest.

Selected encodings: AnnData0.1.0, dataframe0.2.0, dense array0.2.0 and CSR/CSC0.1.0;
string-array0.2.0/vlen-UTF8; Zarr2 C/F/endian, all8 integral and Float32/64;
null/zlib/gzip only. Counts retain UInt64 bits, continuous is finite/exactly
converted; integral fills are strict integer/null in this NEW profile. Explicit
index paths must match dataframe _index. No inferred domain, registration,
missingness, duplicate summing, graphs or unselected metadata interpretation.
Dense input remains chunk-streamed; a bounded16-byte/entry sparse index serves
pages without whole-matrix densification. Existing default100M logical-cell/import (validated option limits retained) and
1M-axis limits remain; no unrestricted sparse-time/large-corpus speed claim.

Provider, native host and independent C++ fixtures/tests have disjoint owners.
Root owns ABI/receipts/codec/CLI/SDK/build/inventory integration. Six pinned,
unmodified nonexecuted reference/schema/notice files (AnnData BSD3, Zarr CC-BY4.0,
Numcodecs MIT) are retained under their own terms in the
[reference intake](../build/gui-aggregation/catalogue-closure/anndata-zarr/references/intake.json).
No Python product parser/runtime, upstream dataset/model or restricted input.
Cell Ranger's current implementation has restrictive product/internal-use and
no-redistribution terms; it is not admitted. M06 schema eligibility remains
unreviewed independently. MuData was inspected as a future open candidate only.

## Build/check decision and current evidence

The [before state](../build/gui-aggregation/catalogue-closure/anndata-zarr/before.json)
preserves20 affected source/card/build inputs and10 retained product identities.
The [first Windows build](../build/gui-aggregation/catalogue-closure/anndata-zarr/build-01.json)
passed all eleven targets. Its initial native run passed dense counts and signed
continuous results, then stopped on a test comparing the internal recipe hash
with the public identity that also binds the file hash. The test now binds both
before comparing. The original failed run remains under `baseline-01`. The next
run passed 146 native checks, including copied-store identity. Separate checked
controls produced byte-identical file inputs for schema 4 and both schema 5
domains. The final provider revision and the SDK safeguards require their own
qualification before staging. All 23 [native and consumer commands](../build/gui-aggregation/catalogue-closure/anndata-zarr/integration-01/results.json)
passed on final products, including protected destinations and fresh-process
reopen. Four more pinned producer checks passed. The generated AnnData export
preserves all count values and IDs. Default Blosc and contradictory consolidated
metadata refuse before publication.
Changed inputs are the additive public headers, directory content fingerprint,
loader/runtime/shared import executor, schema6 metadata/writer and thin CLI/SDK.
Reuse unchanged products. The follow-up build covers the corrected test, SDK
destination and disk-reopen checks, and upstream producer compatibility fixes in
the existing cache. Link exact M05/A08/FP04 retained products.
The follow-up build passed. Its SDK link preceded the final SDK edit by five
seconds, so only that consumer target was rebuilt before the 23-command run.
No core/kernel/GUI/binding/wheel or general qualification campaign is selected.

Decisive checks: literal counts/continuous dense/CSR/CSC/chunk/order/endian/string
anchors; >64 selected chunks and page crossing; native trust, limits, actual
source changes, cancellation and protected no-partial outputs. Preserve exact
ABI1/schema4 and ABI2/schema5 baseline QMX on original input paths. Verify native,
CLI and SDK QMX/frame/receipt equality, owned project save/reopen, provider-free/
OFF reads and valid missing-provider errors. Then freeze actual compiled inputs,
stage matching optional module/native/source/licenses once, run material/ZIP and
missing installed/removal checks preserving user data; no publication selected.

Prebuild review corrected a mistaken new schema6 equality between position-file
SHA and observation-axis SHA; both identities remain separately bound. One worker
edit initially searched a nonexistent anchor and wrote nothing; a concrete anchor
fixed it. An initial rg path glob repeated the Windows syntax error; use explicit
directories plus -g filters. No compiler/numerical assertions were weakened.

The 1,024 × 1,024 sparse workload used 100 store members and 1,024 stored
entries. It took 9.3 seconds and created a 22.8 MB QMX, with 45.6 MB peak sampled
import output. Each distinct project folder needs another QMX-sized managed
copy. A relocated byte-identical store retains the content hash and has its
own capture and association identity.

[Local delivery](../build/gui-aggregation/catalogue-closure/anndata-zarr/results.json)
passes 285 source/package/ZIP material checks, relocated SDK configuration and
nine installed and post-removal commands. Removal removed exactly 285 owned
members and preserved all nine user files. Prior material groups and frozen
products remain exact.

## Prior deliveries and remaining scope

[I22 exact image fills](benchmarks/ngff-fill/README.md) passed214 cases and affected
native source/server/project routes; [M05 Loom](benchmarks/loom/README.md) retains
its34 native and12 deployment checks and284 admitted members. Frozen products,
source snapshots, failures and archives remain exact. All other eligible format
and application/integration rows remain open under their own terms/contracts.
Follow the [Windows checkpoint](api-delivery-plan.md#next-delivery-checkpoint-windows-research-workflow)
and [validation policy](api-delivery-plan.md#validation-and-build-policy). Keep one
active integration cache/runtime and one selected outcome; do not replay old
accepted campaigns. R11 matrix attachment retry now has its local source-scoped
receipt. Other qualified format and application outcomes remain in the queues.

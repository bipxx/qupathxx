# Image server contract

The first image-server packet separates reading pixels from document state and
display. Dense RGB images and OpenSlide share the same synchronous C++ interface.
It is available in the installed Python package and the native region CLI.
Verified development wheels use the pinned Windows/Linux OpenSlide SDK;
[reader runtime validation](reader-runtime.md) records exact small-level TIFF
checks and why the older Ubuntu system reader is outside that acceptance.

## Coordinates and pixel representation

Image dimensions and region coordinates use signed 64-bit integers. Each pyramid
level records width, height and its actual downsample factor; do not assume
powers of two. Region `x` and `y` refer to level zero, while requested width and
height count pixels at the requested level. There is no implicit resampling to
an arbitrary user-selected downsample. Unsupported levels fail before decoding.

The initial pixel type is unsigned 8-bit RGBA, packed in row-major order with
four bytes per pixel and no row padding. This is explicit metadata rather than
an assumption that future readers must return RGB8. Z/time axes and raw multichannel
or higher-bit-depth data remain future work. Outside-image pixels are transparent
black. Coordinates may be negative, but arithmetic overflow is rejected.

OpenSlide returns integer premultiplied ARGB. The adapter extracts channels by
bit shifts, converts to straight alpha with rounded integer unpremultiplication,
and zeroes RGB when alpha is zero. Output remains in device color space; the
current reader does not apply ICC transformations. A single output buffer is
limited to 256 MiB; the OpenSlide conversion temporarily holds a second equally
sized buffer. This limit is not a global cache or process-memory limit.

## Calibration and measurement

Physical pixel width and height are independent optional values. Missing,
nonfinite, nonpositive or malformed OpenSlide calibration stays unknown.
Python returns `None` for an unknown axis. Document construction copies source
calibration; an explicit document override does not modify source metadata.

Physical area requires both axes and raises an error otherwise. Setting
calibration validates both values before changing either. Clearing calibration
restores unknown values. This intentionally replaces the prototype's default
of 1 um/pixel. ROI geometry is double precision in level-zero coordinates and
must lie inside the image. Geometry at extreme coordinates remains limited by
double precision even though image metadata uses 64-bit dimensions.

Mean intensity includes pixels whose centers lie in the half-open rectangular
ROI. It averages straight RGB channels over pixels with nonzero alpha and reads
at level zero in blocks of at most 256 x 256. Fully transparent pixels are
excluded. This is a synchronous measurement, not stain quantification or a
background analysis job. Large regions should not be measured on the UI thread.

## Ownership and concurrency

Servers use shared native ownership; a document retains its server. The native
base uses `enable_shared_from_this` so nanobind reuses a factory server's existing
C++ control block when it passes through Python and back into a document or
scheduler. It must not create a Python-owned proxy deleter that a native worker
would need the GIL to release. C++ consumers must rebuild against this base-class
change. Timeout-guarded save/autosave subprocess tests drop the last live Python
server/document references while their native snapshot is being written, then
wait for completion and validate the saved pixels. Reads return
owning buffers independent of both document and server lifetime. Metadata and
annotation values exported to Python are snapshots. `PixelBuffer.data` currently
copies into Python `bytes`. Read-only NumPy views and explicit independent writable
copies are now available; see [NumPy exchange](numpy-arrays.md).

Read and close operations serialize on a per-server mutex. Close waits for an
in-flight synchronous read, is idempotent, and makes subsequent reads fail.
It is not cancellation. Metadata remains queryable after close. Python releases
the GIL around open/read/close, which touch only native state. Document mutation
remains single-threaded; `show` retains the GIL. A closed shared server also makes
its documents unable to read pixels.

The OpenSlide handle is created once, held through reads and closed by RAII.
Constructor failures close partially opened handles. Every OpenSlide operation
checks its terminal error state; invalid client requests do not poison the
handle. The optional reader-disabled build fails explicitly on `open_slide`.

## Validation boundaries

Exact dense/pyramid fixtures verify coordinate mapping, RGB/alpha conversion,
padding and pixel measurements. The public Aperio fixture is pinned by SHA-256
in `tests/fixtures/manifest.json` and compared with the official Python binding
and native CLI. That comparison checks adapter consistency, not independent
decoder correctness: both use OpenSlide. The generated TIFF's expected pixels
are specified independently of OpenSlide. The small Aperio fixture has one
level. The opt-in [extended Aperio checks](extended-reader-validation.md) add a
real three-level vendor pyramid, fractional downsample, partial-alpha edges,
truncated containers and damaged JPEG tiles. Other vendors remain unvalidated.

The desktop GUI now displays dense and OpenSlide sources through the
[tiled viewer](tiled-viewer.md), native scheduler and bounded GPU cache. Thumbnail
policy and interaction automation have separate bounded contracts and tests;
ICC display conversion and broader vendor stress remain M1/M2 work.

# NumPy region and result exchange

Install `qupathxx[arrays]` to add NumPy. Normal imports, readers, analysis jobs
and the native executables do not require NumPy. The existing `.data`, `.labels`
and `.mask` properties continue returning independent Python bytes.

```python
import numpy as np
import qupathxx as q

image = q.Image.from_numpy(np.zeros((128, 256, 3), dtype=np.uint8))
doc = q.Document(image)
region = doc.server.read_region(q.RegionRequest(0, 0, 0, 256, 128))
rgba = region.numpy()                 # uint8 HWC, read-only view
editable = region.numpy(copy=True)    # independent writable HWC array
uploaded = q.PixelBuffer.from_numpy(editable)

transform = q.StainTransform([[.65, .70, .29], [.07, .99, .11]])
concentrations = transform.deconvolve(uploaded).numpy()  # float32 HWC
```

`Image.from_numpy` accepts three channels in RGB order; `PixelBuffer.from_numpy`
accepts four channels in straight RGBA order. Both require CPU `uint8` data,
three dimensions, positive height/width within int32 and at most 256 MiB of
input pixel payload. Wrong dtypes reject without implicit casting or scaling.
No alpha premultiplication, RGB range adjustment, ICC or color-space conversion
occurs. Native geometry uses x/y; arrays use row/column/channel.

Inputs can be read-only, Fortran ordered, reversed, sliced, transposed or
broadcast. Contiguous HWC input uses one bulk copy; other layouts copy logical
pixels according to strides. The input array is never retained by the returned
native object. Subsequent input edits have no effect on native images/jobs.
The binding accepts compatible typed CPU arrays/buffers through nanobind; this
does not enable GPU memory or asynchronous device transfers.

## Array shapes and ownership

| API | Shape | Dtype | Default ownership |
| --- | --- | --- | --- |
| `Image.numpy(copy=False)` | height, width, 3 | uint8 | Read-only shared native RGB storage |
| `PixelBuffer.numpy(copy=False)` | height, width, 4 | uint8 | Read-only region storage, retaining the native wrapper |
| `FloatRgbImage.numpy(copy=False)` | height, width, 3 | float32 | Read-only transform storage, retaining the native wrapper |
| `TissueResult.labels_numpy(copy=False)` | height, width | uint32 | Read-only labels, retaining the native result |
| `TissueResult.mask_numpy()` | height, width | uint8 | Newly allocated writable 0/255 mask |

Exports are C contiguous with native-endian numeric types. Cell nucleus/body
results are `TissueResult` instances, so use `cells.nuclei.labels_numpy()` or
`cells.cells.labels_numpy()`. Existing-level label grids retain their documented
level/downsample/level-bounds coordinates; exporting does not resample them.

Read-only views share native pixels. Assignments and NumPy attempts to enable
writing reject. `copy=True` returns independent writable storage. Native capsule
ownership means the copy need not have NumPy's `OWNDATA` flag; resizing in place
is not part of this contract. Use ordinary NumPy copies if needed. Exported
views/copies remain valid after document/server closure, reader replacement,
project removal, scheduler cache eviction/shutdown, job completion and archive
removal, and after their original Python variables are collected.

RGB `Image` copies now share immutable backing storage. Each exported RGB view
retains that backing independently of the Image wrapper. `Document.image` now
returns a cheap owning Image copy, so a retained Image wrapper also survives a
reader relink. This matters when an
Image reference was borrowed from a document whose reader is later relinked:
retaining only the document would not preserve the old reader's pixels. The
other export types own stable buffers and expose no Python mutation/replacement
methods; their arrays keep the owning wrapper alive. Native C++ callers must
still respect their existing const/lifetime rules and must not mutate a region
buffer while Python reads it.

## Threads and workflow

Array import copies while holding the GIL; array metadata, capsules and Python
objects are created under the GIL. Native reader and analysis jobs retain their
existing GIL release and worker rules. Workers never retain imported Python
arrays or call Python callbacks. Imports cannot synchronize with external
NumPy/native threads that independently write to the same input: keep input
memory stable during the call. Read-only native views can be read concurrently
with native job completion because the published buffers are immutable.

The complete [region workflow example](../examples/region_workflow.py) opens a
slide/PPM, reads a NumPy region, creates an ROI, runs native OD analysis, exports
project measurements and saves/reopens a project. `--show` opens that saved
project in the existing ImGui viewer. It refuses existing output filenames.
Python tests exercise the example and opt-in viewer reopening, array lifetime,
strides, isolation, scheduled reads, archived jobs and interpreter shutdown.

This implements the defined M6 CPU pixel/label exchange. Public stubs and a typed workflow are included; see
[Python API declarations](python-api.md). Managed live-viewer command queues are implemented; external model adapters, raw higher-bit-depth or
multiaxis readers, GPU exchange and broader long-running memory validation
remain separate unfinished work.

The implementation follows nanobind's documented array constraints, ownership
and return policies: [nanobind ndarray documentation](https://nanobind.readthedocs.io/en/latest/ndarray.html).


## Validation and measurements

The focused installed-wheel array/public-stub suite passes 41 cases on both
NumPy 2.5.4 and 1.26.4. It covers all import strides, exact dtype/shape/bytes,
read-only enforcement and independent copies, retained Image wrappers and
arrays across relink, source closure, scheduler eviction, archived jobs and
owner collection, native job completion during array reads/imports, malformed
inputs, interpreter shutdown and the complete example with viewer reopening.
A separate clean environment without NumPy imports the package and reads native
regions successfully. Public stub regeneration matches the installed extension,
strict mypy accepts the declarations/example/positive fixture and rejects the
array/model and live-endpoint negative cases.

The warm 1024x1024 benchmark exports a 4194304-byte region view in about 0.0004
ms p95 and a float32 view in about 0.0004 ms p95 (1000 samples each, near this
timer's resolution). These calls allocate array metadata and retain ownership;
they do not read/copy the pixel payload. Writable region copies measure 1.905
ms p95, compared with 1.429 ms for the existing Python-bytes copy route. RGB
import measures 1.309 ms p95 over 30 samples; a reversed/column-sliced RGB input
measures 2.538 ms p95. Strided imports prioritize exact layout handling; this is
not a claim that all copies became faster. Source and binary hashes, NumPy/OS
versions and sample summaries are in the [raw benchmark](benchmarks/numpy-arrays-windows.json).

This evidence covers one warm synthetic size on one Windows PC. It does not
establish large-slide throughput, process RSS limits, all NumPy releases or
platforms, GPU/framework interoperability, or a cross-platform performance
budget. Remaining M0-M7 validation stays active.

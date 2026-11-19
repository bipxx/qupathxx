# Native COCO instance annotation plugin

`org.qupathxx.coco`, contract `coco-instance-pixel-masks/1`, annotation ABI5.
Optional Windows x64 MSVC1944/compiler19.44.35225.0 Release `/MD` module.
C++ owns parsing and rasterization; the shared native host owns image frames,
classes, contours, identities, preview/apply, undo and project persistence.

Select a standard instance-annotation JSON document containing `images`,
`categories` and `annotations`. `info` and `licenses` are optional metadata.
Pass `{"image_id":7}` explicitly through provider options. The matching image's
full width/height must equal the selected native level-zero image frame. File
names and URLs are retained as text and never opened. Alignment, scaling,
calibration and axes are not inferred from a filename or bounding box.

Each selected annotation becomes one native object. Its exact unsigned JSON
integer `id` maps to `annotation/ID`; `category_id` maps to the source class
`category/ID`. IDs through UINT64_MAX retain their exact stored identity. Map
these category IDs explicitly to existing native class keys. Repeated category
display names remain independent. Image/category/annotation IDs must be unique
within their catalogues, and all annotation references must resolve. Names,
supercategories and metadata remain source facts; no class hierarchy is created.
All-channel/unpositioned source planes use the shared native unique-plane or
explicit `--unpositioned-z` / `--unpositioned-time` rules.

The selected geometry profiles are:

- Polygon arrays use the pinned COCO API's fivefold boundary rasterization and
  union all parts. Nested parts add foreground; they do not designate holes.
  Finite binary64 source coordinates may cross image edges; raster coverage clips
  to the declared canvas using the COCO rule. Repeated vertices contribute their
  boundary point; zero-length edges avoid the reference C implementation's
  undefined NaN-to-int conversion. Nearest rounding, gradual underflow and masked
  FP exceptions are required; the module never changes the caller's FP modes.
- Uncompressed RLE uses exact nonnegative integer alternating background/foreground
  counts. Compressed RLE uses COCO's signed five-bit ASCII/delta coding. Both use
  column-major order, start with background, and must sum exactly to width*height.
  `size` must equal `[height,width]`. Zero-length runs retain their parity.
- Both yield exact whole unit pixel cells at origin zero. Native QBM1 transport
  carries the complete canvas and row-major MSB-first bits. Existing native
  LabelRaster/ScientificGrid/contours conserve pixel area, disconnected parts and
  holes. One source instance, including a crowd mask, remains one object.

Required `iscrowd` is exactly0/1 and persists independently. Original annotation
JSON (including segmentation, source `area`, `bbox` and crowd), selected image,
category, document info and license metadata persist as length-bound UTF-8 chunks.
Source area/bbox are metadata, not recomputed measurements or native calibration.
The raw file, options, module, target frame/classes, mask bytes and canonical
geometry participate in the shared native receipts and exact reuse guards.
Saved projects reopen without this plugin. COCO evaluation/IoU rules are separate
from native geometry measurement.

Empty coverage, unresolved/duplicate IDs, duplicate JSON keys, unsupported
schema/coordinate/transform extensions, detection-result lists, bbox-only,
keypoint and panoptic schemas, malformed numbers/UTF-8/RLE, unknown options,
mismatched canvases and exhausted quotas refuse before atomic publication.
The selected image controls geometry interpretation; other images are not imported.

Bounds: input1 byte..64MiB, plugin work256KiB..256MiB, JSON depth32/nodes1,000,000,
64 members/object, strings<=1MiB, numeric tokens<=1024 bytes; each catalogue
<=100,000 records. Selected canvas dimensions<=1,000,000 and packed QBM1<=1MiB
(at most8,388,512 pixels). Polygon parts<=1024, vertices/part<=100,000,
boundary events/part<=2,000,000 and total selected-image raster work<=64,000,000
steps. Native raster/edge/vertex/output work is independently budgeted. These are
explicit refusal limits, not promises that every mask below them fits every quota.

Offline module build (no SDK/core, Python, Java, codec or network discovery):

```powershell
cmake -S source/plugins/coco -B cache -G "Visual Studio 17 2022" -A x64 -T version=14.44.35207 `
  -DQUPATHXX_FORMAT_PLUGIN_KIT_PREFIX=C:/candidate/source/format-plugin-kit
cmake --build cache --config Release --target qpx_coco_plugin
```

For example, import into a separate project output with the installed native CLI:

```powershell
qupathxx_format_import --format-plugins C:/candidate/format-plugins.json annotations-import `
  org.qupathxx.coco input.qpproj 1 instances.json imported.qpproj `
  --plugin-options-json '{"image_id":7}' --class-map-json '{"category/3":"Tumor"}'
```

Implementation/shared producer/JSON/application code is AGPL-3.0-only.
`coco_mask.hpp` is a bounded adaptation of the BSD-2-Clause COCO mask API, with
its upstream and contributor notices retained. `COCO-LICENSE.txt` and
`reference-lock.json` pin exact upstream code/license references at
8c9bcc3cf640524c4c20a9c40e89cb6a2f2fa0e9. Original source and binary notice
obligations remain attached to the actual materials. The independent upstream
C oracle is a development test only and is never linked into the product.
No COCO dataset/image/model/annotation asset or derivative is admitted by the API
license or JSON container. See the repository's reader admission policy.

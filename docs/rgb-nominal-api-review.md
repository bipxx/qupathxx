# RGB nominal API contract review

The new surface is alpha. Existing `RegionRequest`, `resample_area` and
`read_resampled_region` are treated as stable contracts. The audience is native
C++, Python scripting and the installed region CLI. This is a local synchronous
read API, with no network resources, credentials or transport protocol.

| Finding | Evidence and user impact | Compatible resolution |
| --- | --- | --- |
| Existing sampling semantics must remain stable | Area sampling integrates complete extents; nominal sampling has a different transform, kernel and border behavior | Add `RgbNominalRequest` and `read_rgb_nominal_region`; keep existing calls and defaults. This follows the additive and semantic-compatibility principles in [AIP-180](https://google.aip.dev/180). No migration is required. |
| Source extents and returned dimensions have different meanings | A17-pixel extent at nominal downsample1.5 returns11 pixels; width/11 must not replace the requested step | Use immutable `x_px`, `y_px`, `width_px`, `height_px` in level-zero pixels, and dimensionless `downsample`. Unit-bearing names follow [AIP-141](https://google.aip.dev/141). The returned PixelBuffer reports output dimensions separately. |
| Request behavior and errors need to be explicit | Silent mutation, partial results or an ambiguous natural-level origin would make client workflows unreliable | Constructor validation is constant-time and precedes IO. Reads require unit level zero, bounded opaque RGBA windows and the interpolation halo. Results own their storage. Field/behavior documentation applies the principles of [AIP-203](https://google.aip.dev/203) to native comments and generated SDK properties; no proto annotations are involved. |

The policy identity is `qpx.rgb-nominal/1`, independent of a Java runtime or a
promise to match every QuPath reader. The pinned opaque-RGB corpus constrains
its bytes. Unknown transparency, vendor/pyramid policies, scientific recipes
and capture/replay authority require their own explicit contracts.

Invalid dimensions, nonpositive/nonfinite ratios and budget violations raise
`std::invalid_argument`/Python `ValueError`. Closed readers, decoder failures and
malformed windows raise runtime errors. Cancellation returns no partial output
and cannot interrupt a synchronous decoder. Python releases the GIL for the
native read. Construction and the CLI reject invalid requests before opening
pixel IO; the CLI protects its source path and creates output after a full read.

| Review area | Coverage |
| --- | --- |
| Resources/names | Network resources are not applicable; native names and pixel units reviewed |
| Methods/request fields | Reviewed in resampling.hpp, bindings.cpp and region_main.cpp |
| Compatibility/versioning | Additive operation with a versioned algorithm identity; existing defaults unchanged |
| Documentation/examples | Header contract and RGB resampling guide; generated stubs and typed examples required |
| Errors/auth/retry | Errors and cancellation reviewed; authentication and network retries are not applicable |
| SDK/client surface | Readonly Python request, owned output, GIL release and CLI argument route reviewed |

No proto/OpenAPI API-linter configuration applies to this repository surface.
Use the native captured-pixel/reader-fault checks, installed Python/CLI cases,
stub guards, strict typing and existing CI audit controls. Complete those before
platform qualification; UI and scientific workflow opt-in remain separate work.

## Original-document native cell extension291

`CellRequest(document, object_id, transform, NominalCellSampling, GridCellParameters)`
is additive to both existing constructors. The immutable descriptor wraps the
unit-bearing RGB request and two output-context sample counts. Its edges/centers
keep the nominal transform; returned dimensions never replace that transform.
The existing cell controller, physical kernels and publication owner checks are
shared with scientific area grids. Area grids retain their rational geometry.

Result masks expose `nominal_sampling()` separately from `scientific_grid()` and
their world-coordinate sample edges retain original-slide coordinates. Unsupported
world precision, context and memory budgets fail before pixel IO; late cancellation
produces no partial result. Physical units require both captured calibration axes.

QCL6 and QCA6 explicitly retain the nominal descriptor and algorithm identity.
Historical records decode without reading the source and return no live authority.
Fresh replay uses the original reader and exact request equality includes nominal
origin/extents, step and context. Embedded/external provenance must match exactly;
the source remains unit level zero. Older encodings/defaults remain supported.

SDK292 delivers readonly Python descriptor/request/result/classifier metadata,
exports, generated stubs and strict typing. `for_nominal_roi` reuses the area-grid
enclosing-bounds helper and retains the original ROI. Public fields return owned
copies. The existing detect/wait paths retain their GIL and native ownership
contracts.31 new installed cases pass.

CSV readers needed an explicit nominal branch before this exposure: typed results
previously assumed a non-null ScientificGrid. New table schemas7/8 discriminate
`nominal_rgb`, keeping requested extents, core counts and nominal steps separate.
Existing natural/area formats remain unchanged. Classifier scalar guards include
nominal sources, and readonly resolved parameters remain available.

Batch/CLI293 adds mutable `NominalCellRoiSampling` downsample/context options,
copied by `ProjectBatchRequest` and resolved through the same factory per ROI.
The fixed-source readonly descriptor remains separate. Existing typed detector
parameters, batch checkpoints and single-target selection are reused. Explicit
area/nominal and legacy flag conflicts reject before opening the project;
typed detector flag order is independent. Sampling and detector identities enter
the recipe, while unused legacy thresholds are canonicalized only for this mode.

The [local293 receipt](benchmarks/nominal-cell-batch-293/receipt.json) covers
installed Python/native CLI and standalone checkpoint/resume. Complete existing
GUI/managed creation consumers next; full platform, biological/resource/hardware
and stable/beta/release acceptance remain separate.

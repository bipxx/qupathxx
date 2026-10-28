# Python API declarations and validation

Use `import qupathxx as q`. The public package exports the names listed in
`qupathxx.__all__`; the `_native` implementation module additionally contains
types needed by return values. Wheels ship `_native.pyi`, explicit public
reexports in `__init__.pyi` and a `py.typed` marker. Type information covers the
current project, ROI, geometry, reader, measurement, job, provenance and GUI
surface as well as NumPy exchange.

Array methods declare their concrete `numpy.typing.NDArray` dtypes. Runtime
validation additionally enforces channels, shape, device, byte limits and
read-only ownership, which NumPy's static types do not fully express. Install
the `arrays` extra when using NumPy or type-checking these declarations. Normal
runtime package import and native workflows still work without NumPy.

The complete [region workflow](../examples/region_workflow.py) and
[typed API fixture](../tests/typing/valid_api.py) pass strict mypy checking. The
[negative fixture](../tests/typing/invalid_api.py) intentionally fails for a
float32 array passed to a uint8 importer, a string image ID, assignment to a
read-only property, the wrong request type submitted to a job and direct
construction of a native result object. Live endpoint checks additionally reject
a Project passed in place of a filename and an Image submitted as an edit.

## Delivery map

See [API/workflow delivery paths](api-delivery-plan.md) for existing entry points,
reuse-first implementation routes, remaining requirements and acceptance per
API family. These routes preserve the native authority and typed binding
boundary described here; proposed workflows are not new released API promises.

## Regeneration

Build and install the current wheel into the interpreter used below, with NumPy,
nanobind and mypy available. The recorded generator is nanobind 3.1.0 and the
recorded checker is mypy 2.4.0. From the repository root:

```powershell
python -I tools/generate_stubs.py
python -I tools/generate_stubs.py --check
python -I -m mypy --strict --no-incremental src/qupathxx/_native.pyi src/qupathxx/__init__.pyi
$env:MYPYPATH = (Resolve-Path src).Path
python -I -m mypy --strict --no-incremental examples/region_workflow.py tests/typing/valid_api.py
python -I -m mypy --strict --no-incremental tests/typing/invalid_api.py
```

The last command must fail with the seven intended type errors. Generation uses
the installed extension's signatures, excludes changing default values and
applies deterministic annotations for array methods that return Python objects
internally. Native-only result constructors receive a required private `Never`
argument so type checkers reject direct Python construction. It derives public reexports from the source
`__all__`. Never regenerate against an old editable installation. `--check`
compares the complete generated files without writing source.

Installed-wheel tests verify that markers/stubs are packaged, all public
exports are declared and native public classes, members, functions and constants
are represented. Regeneration checks and type checks verify signature/type
details beyond that surface-presence test. The implementation follows the
[nanobind typing documentation](https://nanobind.readthedocs.io/en/latest/typing.html).

## Compatibility and execution boundary

These declarations describe the current development API. They do not establish
QuPath/Java compatibility or a released stable ABI. Preserve existing public
names, positional order, defaults and result semantics when making additive
changes; update generated declarations and representative positive/negative
checks together. Breaking changes need an explicit migration before the planned
research beta can be called stable. Release compatibility/platform gates in M6
and M7 remain open.

Projects/documents and job capture/publication remain single-owner operations.
Readers and immutable result buffers have the concurrency rules documented in
[NumPy exchange](numpy-arrays.md), [batch processing](project-batch.md) and their
individual analysis/job contracts. Static annotations do not grant permission
to mutate a document from another thread or from a worker callback. Managed [live viewer queues/notifications](live-viewer.md) are implemented.
Broader command coverage, external model adapters and release compatibility
gates remain unfinished.

[Pixel recipes and masks](pixel-classifiers.md) expose typed feature options,
context windows, immutable models/results and uint32/float32/uint8 NumPy views.
The [pixel example](../examples/pixel_classifier.py) trains, saves, reopens and
predicts a synthetic grid; physical sampling and source authority remain with
the caller.

The public `GuiReport` includes typed pixel-worker/controller results and the
logical `canvas_content_bounds` used for image-to-framebuffer mapping. See the
[F5 pixel desktop contract](pixel-classifier-ui.md).


## Typed quantitative grid sampling

The native Python interface supports bounded raw TIFF sampling independently of
RGB display conversion. This example reads channel/Z/time zero over a bounded
level-zero window; grid sample counts may differ from source pixel counts.

```python
import qupathxx as q

server = q.open_tiff("quantitative.tif")
metadata = server.raw_metadata
assert metadata is not None
grid = q.ScientificGrid(
    metadata.width, metadata.height,
    q.ScientificGridAxis(0, min(metadata.width, 256), 32),
    q.ScientificGridAxis(0, min(metadata.height, 256), 32),
)
sampled = q.read_scientific_raw(server, grid)
values = sampled.numpy()          # readonly HWC float64; retains its native owner
reasons = sampled.reasons_numpy() # HWC uint8; ScientificRawReason codes
assert q.digest_scientific_raw(server, grid).input_sha256 == sampled.input_sha256

document = q.Document(server)
request = q.RawAnalysisRequest(document, grid)
result = q.analyze_raw(request)
assert result.grid is not None
record_id = document.keep_raw_analysis(request, result)
csv = q.raw_analysis_csv(q.RawTableRequest(document, [record_id], True))
```

Use `ScientificRawOptions` for explicit channel/Z/T selection and work limits.
A grid result's descriptor separates original dtype/calibration from derived
binary64 statistics. Natural results have `grid is None`; natural `region`
access rejects for grid results. Save/reopen keeps historical QRA2 records;
`RawAnalysisRequest.replay` plus fresh analysis/verification restores current
status only after validation. Array views and descriptor snapshots survive
reader close and parent-variable deletion; writable copies use `copy=True`.

This interface has local Windows isolated-candidate checks. Complete wheel,
platform and grid CLI/batch/ImGui producer qualification remain pending.

## Spatial distances

The [spatial workflow](spatial-analysis.md#installed-python-surface) now has locally
verified nanobind requests, immutable results, owner jobs, retained project records
and CSV export. Use bounded rows(start,limit) pages or row(index), and retain None
for absent targets/distances. Native-only constructions and optional job publication
are enforced by generated stubs and strict positive/negative typing fixtures.
[Receipt266](benchmarks/spatial-python-266/receipt.json) records the Windows
CPython3.12 installed/GIL/lifetime/schema tests. CLI and application controls remain.

# Native sampled shape watershed

The internal candidate now reproduces ImageJ 1.54p shape-stage masks and selected
maxima in native C++. It composes with native LoG regions and ROI-safe nucleus
filtering. Production recipes, Python bindings, UI controls, batch manifests
and archive identities are unchanged; integration needs an explicit new recipe.

## Algorithm and ownership

`cpp/shape_watershed.hpp` adapts the EDM-specific MaximumFinder stages by Michael
Schmid and Wayne Rasband: sampled ridge-height correction, prominence analysis,
maximum-area marking, quantization, suppression cleanup and simultaneous
directional growth constrained by neighborhood topology. Inputs and all work
buffers have native ownership. Processing calls no Java or Python APIs.

ImageJ [describes itself as public-domain software](https://imagej.net/ij/docs/intro.html).
The [attribution](../third_party/imagej-shape-NOTICE.txt) identifies the pinned
1.54p sources and is installed under `qupathxx/licenses/imagej-shape/NOTICE.txt`.
This does not select the qupathxx project license.

The distance map uses the existing exact squared-Euclidean transform with
outside-image background disabled. ImageJ uses an approximate distance transform
whose documented deviations can occur above distance 13. This implementation's
frozen conformance results do not imply universal equality for larger masks.
The [ImageJ EDM API](https://imagej.net/ij/developer/api/ij/ij/plugin/filter/EDM.html)
describes those approximation and boundary semantics.

Scratch/output admission requires `64 * pixels + 64 * max(width, height)` bytes.
It accounts for the EDT line workspace, float distances/ridges, pixel flags,
quantized levels, ordered candidates and work/change lists. Cancellation is
checked throughout. Traversal is capped at 1,024 counted steps per pixel;
candidate counts and dimensions also have explicit limits. The composed LoG
kernel retains its 128-byte-per-pixel admission and 64-times-raster bounding-box
traversal guard. These are logical admission bounds, not process RSS guarantees.
Tests cover blank/full fields, invalid input, cancellation, memory/candidate
limits, state reuse, ROI clipping and explicit seed-control requirements.

The standalone candidate requires zero additional radius/seed-spacing filters
and uses prominence 0.5. Those controls are recorded explicitly in every
comparison. Existing recipe defaults are preserved. New-mode API defaults and
saved replay semantics must be resolved together during integration.

## Frozen conformance and real tissue

`tests/fixtures/imagej-shape-1.54p/capture` contains 26 input, segmented-mask and
selected-maximum oracle sets captured through pinned public ImageJ APIs. Cases
include rows/columns, blank/full fields, plateaus, touching disks, thin necks,
border objects, diagonal contacts, a ring, 12 seeded irregular fields, large
disks and two native real-slide region masks. Byte manifests bind the runtime,
generators, inputs and outputs.

Raw derivatives under `native` let the C++ unit suite check every mask and
maximum without Python, compression libraries, Java or the whole slide. Python
verification proves those raw bytes equal the captured data. The normal native
`log_watershed` suite now requires all 26 cases; this is not an opt-in oracle.

The [conformance receipt](benchmarks/sampled-shape-conformance-windows.json)
accepts all 26 masks and maxima exactly on the local Windows build. All 18
synthetic detector maps (nine scenes, two feature producers) remain byte-identical
to the legacy control. The [synthetic receipt](benchmarks/sampled-shape-synthetic-windows.json)
retains ROI-hole IoU 0.969849 and resampled IoU 0.938983 as visible differences.

The [real-slide comparison](benchmarks/sampled-shape-ndpi-windows.json) uses the
same pinned CMU-1.ndpi pixels and 506-cell QuPath reference:

| Context padding | Current native cells / nucleus foreground IoU | Sampled-shape candidate cells / nucleus foreground IoU |
| --- | --- | --- |
| 32 | 299 / 0.506401 | 478 / 0.932730 |
| 0, explicit diagnostic | 394 / 0.673465 | 503 / 0.993882 |

For both contexts, the candidate's complete matched nucleus masks and centroids
equal the pinned ImageJ shape-stage control exactly. The remaining whole-plugin
differences include context and preceding LoG-region behavior. Neither context
is selected as a new default. No biological validation or complete QuPath
segmentation acceptance follows from one ROI.

## Validation and performance observations

The [local receipt](benchmarks/sampled-shape-validation-windows.json) verifies
all 254 committed compiler inputs at source `8616902`, 46 native suites and 53 focused
Python checks. It also verifies the installed attribution bytes and exact native
region CLI pixels. The new CI auditor flag `--sampled-shape` requires the native
26-case marker and the exact 53-case verification cohorts, plus the inherited
body workflows. The older fully audited body-mode source rejects this new scope.

The [fixed kernel profile](benchmarks/sampled-shape-kernel-profile-windows.json)
retains 36 fresh-child observations, including six excluded warmups. Trial order
alternates, and isolated-disk output maps/counts are exact across both modes.
At a 1,024 × 1,024 raster / 256 nuclei, median process wall time is 217.9 ms for the
candidate versus 212.9 ms for legacy. Maximum observed native-process peak working
sets are 25.3 MiB versus 31.6 MiB. Timings include startup and protocol file I/O;
the Windows peak counter is queried on each completed child handle. This
uncontrolled run shows the cost and memory tradeoff without accepting throughput,
full feature aggregation, worker/GUI behavior or hardware targets.

Windows/Linux run40079989538 and Mac run40079991124 target source4a07b0e,
which includes the same numerical kernel and mandatory oracle tests. They remain
separate pending audits. The subsequent attribution-install CMake line is
verified locally and is outside that dispatched source. Production integration
must carry the revised algorithm through versioned records/classifier sources,
input identities, thin bindings/stubs, direct and managed UI, CLI/batches,
physical admission, cancellation and complete installed/platform validation.

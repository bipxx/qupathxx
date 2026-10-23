# Explicit polygon sampling

ROI channel means and tissue detection accept `RoiSamplingPolicy`:

- `GEOMETRY_COVERAGE` is the existing default. Polygon boundaries participate
  in membership, subject to the existing half-open sample-grid bounds.
- `POLYGON_WINDING` uses a half-open ray-crossing rule for polygon boundary
  sample centers. Shells subtract their holes; multipart areas form a union.
  Ring orientation does not change the result. Analytic shapes, lines and
  points retain their existing predicates.

`Roi.contains` and interactive picking retain their geometry contract.
`Roi.contains_sample(point, policy)` makes the chosen sampling rule explicit.
This policy applies to ROI OD/stain means and tissue masks; cell segmentation,
raw measurements, classifiers and stain estimation retain their existing domains.

```python
import qupathxx as q

request = q.AnalysisRequest(document, object_id, transform,
    sampling_policy=q.RoiSamplingPolicy.POLYGON_WINDING)
result = q.analyze(request)
parameters = q.TissueParameters()
parameters.sampling_policy = q.RoiSamplingPolicy.POLYGON_WINDING
mask = q.detect_tissue(q.TissueRequest(document, object_id, transform, parameters))
```

The request captures a native value. Later changes to Python parameter objects
cannot change an executing job. ROI request/result properties expose the policy.
Tissue result parameters retain it. Replay restores the captured policy.

The ImGui Region analysis and Threshold tissue mask panels have separate polygon
sample controls. `GuiOptions.roi_sampling_policy` and `tissue_sampling_policy`
initialize them. Project batches either copy image analysis settings or use
explicit presets. `ProjectBatchOptions.roi_sampling_policy` controls ROI batches;
`tissue.sampling_policy` controls tissue batches. Manifests distinguish nondefault
policies, and checkpoint hashes prevent cross-policy reuse. A nondefault setting
on an unrelated workflow is rejected.

The native batch CLI accepts `--roi-sampling coverage|winding` and
`--tissue-sampling coverage|winding`. The managed viewer provides
`analyze_with_sampling_policy(image_id, object_id, transform, sampling_policy,
kind=..., nucleus=False)`; its existing `analyze` method retains coverage.
Tissue viewer commands capture the policy in their existing parameter argument.

Winding ROI means use algorithm `qpx.roi-channel-means/winding-1`; their input
format is `qpx.rgba-tiles/winding-1`. Winding tissue algorithms use
`qpx.threshold-components/winding-1`, `winding-2` (pyramid), or `winding-3`
(morphology). Pyramid input digests use `qpx.rgba-level-tiles/winding-1`.
Coverage identifiers and archive encoding remain unchanged. Tissue winding
archives use QTM2 with a checked policy field; coverage keeps QTM1. Unknown
versions or policies reject even when the checksum is valid. ROI analysis
archives recover the policy from their validated algorithm identifier.

Prepared geometry predicates handle ordinary interior and exterior samples;
only exact boundary points traverse rings. There is no per-sample geometry copy.
End-to-end performance and scientific acceptance require separate measurements.

Linux validation passes 42 native suites and a 320-pass focused Python run
with 17 optional skips. All 42 pinned reference compartment masks, counts and
OD/stain means match with explicit winding sampling, across the three declared
tile sizes and unchanged tolerances. Legacy coverage characterization remains.
The original focused run retains four expected failures; the resampled gate is
now an explicit winding acceptance test. Three isolated-cell segmentation gates
remain expected failures. Windows, full regression, package/typing, hosted
validation and performance qualification remain pending. This is a validation
candidate and does not establish segmentation parity or biological accuracy.


A larger diagonal regression exposed four incorrectly included boundary centers
in the initial binary. Boundary crossings now use robust orientation, preserving
exact collinearity without division. The corrected native build and its hosted
validation are pending; earlier local/hosted passes and timings predate this fix.

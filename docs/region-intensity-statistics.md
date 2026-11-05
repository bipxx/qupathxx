# Retained region intensity statistics contract

Implementation is pending. This contract selects R7-REGION1's first missing
workflow from the [trace evidence](benchmarks/region-statistics-r7/README.md).
The existing mean-only workflow and GRID1 product remain available.

## Selected outcome and easiest API route

For one selected area ROI or cell nucleus, collect RGB and transformed-channel
sum, minimum, maximum and sample standard deviation, retain them with the existing
means, then save/reopen/replay/export through the existing analysis authority.
Use natural level-zero RGBA8 sampling already implemented by AnalysisRequest.
Do not wrap an ROI in a cell detection or introduce a new reader/sampler/model.

| Surface | Planned route |
| --- | --- |
| Native/owning Python | Add explicit `AnalysisRequest.with_intensity_statistics` capture; retain existing constructors/default means unchanged |
| Result | Optional owning `intensity_statistics` value using the existing CompartmentIntensityStatistics/ChannelIntensityStatistics vocabulary |
| Reducer | Share the cell intensity moments at a neutral internal boundary; keep legacy mean accumulation/order unchanged |
| Jobs and managed viewer | Use the existing analysis action/job; capture the statistics choice on the owner and carry the same native result |
| ImGui | One opt-in region analysis control; read retained statistics and expose the known report field through the same result |
| Batch/CLI | One ROI-only opt-in included in enabled recipe identity; absent settings keep old JSON/behavior |
| Retention/replay | Version the added statistics payload through existing project/snapshot/provenance authorities; reject missing/inconsistent measured payloads |
| CSV | Add the measured fields only when selected results contain them; keep old-only ROI CSV unchanged and mixed rows rectangular |

The [pre-layout witness](../tests/fixtures/region-intensity-v1/native-boundaries.json)
records result256/request24824/record288 bytes and the reusable statistics value216
bytes on MSVC/x64. Use lazy immutable ownership for enabled statistics; do not
inline that216-byte value into every mean-only result. Preserve old default
bookkeeping and fully charge the enabled payload/control metadata.

Settle the request/result, GUI options/report and batch metadata together before
the first implementation build. This avoids late umbrella-header changes.
The exact persistence extension and all inline byte owners must be traced before
layout edits; the existing SQL analysis table has fixed mean fields, so an optional
versioned payload through current storage is the preferred route.

## Sampling and numeric invariants

- Use the captured ROI/nucleus, plane, sampling policy and source. Every sample
  accepted by the existing natural analysis loop contributes once to statistics.
  Retain alpha255 eligibility and center ownership. Do not add context/smoothing
  or change the source stream to obtain additional features.
- RGB samples are the source uint8 values. Channel samples are the existing
  float32 OD/stain outputs promoted to double for reduction. Preserve white,
  normalized stain basis, signed stain outputs and existing input digest rules.
  This adds moments of the existing samples; it does not upgrade old means to a
  different precision/sampling recipe.
- Preserve mean-only Kahan accumulation and order. The optional moments use the
  shared compensated sum/extrema/Welford path in the same pass. Retain existing
  means independently; check channel sum/sample count agrees with them under a
  declared numeric tolerance. No second decode/transform/segmentation pass.
- Standard deviation uses sqrt(M2/(n-1)). At n=1, expose absence and export an
  empty SD field; do not fabricate0 or persist NaN. With no accepted samples,
  keep the existing analysis error. A constant multi-sample channel has SD0.
- Reject nonfinite selected channel values or unrepresentable moment arithmetic
  consistently with the shared reducer. RGB units remain8-bit intensity; OD/
  stain values retain their captured transform units. Physical geometry export
  continues to use the existing captured calibration; intensity units are not
  converted by pixel calibration.

The pinned [QuPath RunningStatistics](https://github.com/qupath/qupath/blob/04ccfa4fb7d43e9b566393e08e83690b72248d44/qupath-core/src/main/java/qupath/lib/analysis/stats/RunningStatistics.java)
and [basic intensity plugin](https://github.com/qupath/qupath/blob/04ccfa4fb7d43e9b566393e08e83690b72248d44/qupath-core-processing/src/main/java/qupath/lib/algorithms/IntensityFeaturesPlugin.java)
support the selected sample-SD/basic-feature definition. QuPath plugin sampling,
color/OD-sum transforms and broader feature defaults remain separate comparisons.
Do not claim whole-plugin parity from these primitive definitions.

## Compatibility, checks and task order

1. Completed: record/layout values were captured with a small native observer
   against frozen existing headers. Preserve its distinct observer identity. Reuse the [five saved records](../tests/fixtures/region-intensity-v1/manifest.json),
   already pinned table/batch boundaries and installed product. Do not recapture
   the archive or add a public API solely to observe private byte accounting.
2. Trace all AnalysisResult/AnalysisRecord inline owners, snapshot/project formats,
   classifier feature consumers and table/batch identities. Keep absent-statistics
   legacy bookkeeping explicit; fully charge enabled payloads and ownership.
   Decide all known frontend metadata layouts before implementation compilation.
3. Implement the shared native pass and retained/versioned route, with independent
   tiny RGB/OD/stain expectations. Cover signed channels, both ROI policies,
   nonrectangular/alpha eligibility, a singleton, empty support, cancellation and
   source/object stale publication where affected. Calculate expected eligibility
   before choosing positive fixtures. A first bounded multi-sample comparison
   uses declared absolute/relative tolerances, not an assumption of bit-identical
   upstream samplers.
4. Wire thin consumers, settle bindings/casters and generate stubs once. Check
   actual changed UI/CLI/batch/managed paths, ownership and valid/negative typing.
   Reuse numerical checks at the shared native authority.
5. Package/install once after the staged route passes; verify source/configuration/
   members and one installed save/replay/export workflow. Keep failed candidates
   and old mean-only limits. Do not rerun the full GRID1 or qualification matrix.

After every bounded task or failed/stopped attempt, update the current card with
actual evidence/waste, the applied change and next action before proceeding.
This selected workflow leaves additional resolution/grid contracts, hue/circular
features, approximate median, textures and biological tolerances in the existing
R7/R12 queue. All M0-M7 remain Partial.

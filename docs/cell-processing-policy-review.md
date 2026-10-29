# Cell processing policy contract review

The existing native/Python cell API is treated as a compatibility-sensitive
research contract. Add `CellProcessingPolicy` and
`GridCellParameters.processing`, defaulting to `Quantitative=0`; the opt-in
`ImageJ154p=1` selects captured float32 OD/stain input and Gaussian/LoG arithmetic.
This names a numerical processing policy, not complete QuPath or biological
equivalence. The parameter remains copied by existing requests and consumers.

| Finding | AIP basis | Required implementation |
| --- | --- | --- |
| Replacing the Gaussian or channel precision changes existing results | [AIP-180](https://google.aip.dev/180), [AIP-202](https://google.aip.dev/202) | Add a default-preserving enum field; reject unknown values; retain existing quantitative recipes, archive bytes and batch hashes |
| Omitting processing from saved identity makes distinct requests equivalent | [AIP-180](https://google.aip.dev/180), [AIP-203](https://google.aip.dev/203) | Store the opt-in in new QCL/QCA versions, result algorithm, request equality, batch recipe and CSV; old versions decode as quantitative |
| Sampling and processing have different responsibilities | [AIP-141](https://google.aip.dev/141), [AIP-203](https://google.aip.dev/203) | Keep nominal spacing/context and current distance/area units; only nominal RGB supports this processing policy. Area-grid validation refuses it before IO |
| ImageJ names must not promise every plugin behavior or a runtime dependency | [AIP-182](https://google.aip.dev/182), [AIP-203](https://google.aip.dev/203) | Specify the frozen input/smoothing arithmetic and bounds. Reuse native transforms and the private kernel; retain independent split/merge, biological and supported-host gates |

The processing extent is the requested nominal sample lattice intersected with
source-supported centers. This intersection is rectangular for the current
opaque RGB sampler. Gaussian processing uses nearest-edge extension on that
rectangle, X then Y, with resolved per-axis sigma. Context inside the slide is
real input and can change results. Output labels remain limited to the captured
core/ROI domain; holes do not become a smoothing mask. Transparent source pixels
retain the sampler's explicit refusal. Input identity preserves the original
decoded snapshot, lattice, ROI, transform and calibration.

Use existing `StainTransform` float32 channels: sum OD with two float32 additions,
or select the returned float32 stain channel. Means use those captured channel
values with existing accumulation and calibrated geometry. Defaults retain their
float64 quantitative channel path. Unknown policies, unsupported sampling,
invalid units, budget failures and cancellation must not publish partial results.

Native requests/results and historical storage come first, then thin Python
bindings/stubs, CLI and batch recipes, direct/batch ImGui and managed commands.
All routes use the same native request and publication path. Before accepting
the workflow, check pinned masks/features, changed-policy resume, old archive
bytes, mixed-policy history/classifier sources, owner/stale/cancel behavior and
installed consumers. HTTP resource names, auth and protobuf annotations do not
apply to this in-process API. No project-wide API version change is required for
this additive opt-in; incompatible archive payloads receive distinct versions.

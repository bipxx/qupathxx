# Native watershed cell-body expansion candidate

The production LoG detector still uses nearest-label Euclidean expansion with
an inclusive radius. The separate `cpp/cell_expansion.hpp` kernel instead uses
a strict radius on float distances, stable row-major/FIFO frontier admission,
four-connected propagation and unassigned collision lines. It preserves every
nucleus seed, clips propagation to the supplied domain, admits bounded scratch
before allocations and checks native cancellation. Each nonseed pixel enters
the frontier at most once, with O(n) storage and O(n log n) frontier work.

The [source-bound Windows comparison](benchmarks/cell-expansion-native-windows.json)
starts from the immutable QuPath nucleus maps and exactly reproduces all seven
equivalent natural-grid body label maps: blank, isolated, touching, gap, size
filters, gradient/noise and anisotropic level zero. Every label pixel matches.
The tool verifies all captured file hashes, upstream/contract/generator identities
before comparing; the receipt binds the native binary, compiled source, test
evidence and executed tool. Unit checks cover strict cutoff, collision lines,
domain barriers, seed preservation, bounded frontier admission, invalid inputs,
cancellation and memory rejection.

This accepts only the expansion kernel on the named maps. It does not accept
production detector output, biological performance, context clipping, arbitrary
resampling, general upstream parity or performance budgets. The ROI-hole and
resampled cases are explicitly excluded from this expansion-only comparison.

Reproduce with the built native test executable and an installed scientific
Python environment:

```powershell
ctest --test-dir build/native -C Release -R '^cell_expansion$' --output-on-failure
python -I tools/compare_cell_expansion.py --executable build/native/Release/qupathxx_cell_expansion_tests.exe --output build/cell-expansion.json
```

The binary file protocol is an internal test probe, not a public analysis API.
Production integration requires an explicit captured expansion mode, distinct
algorithm/input identities, versioned archives/classifier sources, replay and
batch-manifest updates, Python types and ImGui controls. Existing saved recipes
must retain their current inclusive nearest-label behavior. The new mode must
then pass full detector/feature, archive, native/Python/CLI/GUI and platform gates
without weakening the pinned tolerances.

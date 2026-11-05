# R6-FEATURE1 numeric foundation — integration remains open

The private cell-intensity reducer is verified by the [native cells role](statistics-reducer.xml).
This is a completed implementation substep within FEATURE1, not a delivered
retained-measurement API or completion of R6/M0-M7. Production results, archives,
nanobind, CSV and consumers are still open in the
[measurement contract](../../cell-intensity-statistics.md).

[Six scalar references](../../../tests/fixtures/cell-intensity-statistics-v1/reference.json)
come from the cached actual QuPath0.7 RunningStatistics runtime. Empty/singleton
standard deviation is unavailable; multi-sample deviation uses n-1. Pair, classic,
constant and1e12-offset cases pin finite extrema/sums and stable central moments.
The [reference manifest](../../../tests/fixtures/cell-intensity-statistics-v1/manifest.json)
pins the independently authored script, runtime JARs and JSON/TSV bytes. Existing
pixels/segmentations are reused later; this capture adds no image corpus.

The C++ reducer accepts existing byte-RGB/float-channel natural inputs and double
scientific inputs. It computes nucleus, inclusive cell and nonnuclear cytoplasm
sums/min/max/sample SD, retains absent singleton SD/empty cytoplasm, and checks
shape/labels, finite foreground, RGB range, memory admission, cancellation and
numeric overflow. Background samples are excluded by labels. It never reads or
resamples pixels and leaves canonical legacy means unchanged. Tiny independent
three-pixel compartment expectations supplement the six external scalar vectors.

The [source scope](source-scope.json) records the compiled dirty-source hash and
changed compiler inputs; installed faa71c1f remains frozen. The build targeted
only qupathxx_cell_tests in the reused cache; CMake correctly refreshed source
metadata/core archive. Do not relabel it as the old runtime or rebuild a wheel
for this private helper. No bindings/stubs/install have been changed.

The first [reference attempt](statistics-reference-missing-json.log) failed at
importing an absent Groovy JSON module, before executing statistics. The repair
uses existing GsonTools, as retained reference scripts already do; the
[successful capture](statistics-reference.log) and failed attempt are preserved.
No new runtime dependency was installed. The first native reducer build/check
passed; its [build log](statistics-reducer-build.log) is retained.

Waste review: **reuse** pinned runtime, exact samples/masks, current build and
reference patterns; **remove** optional serializer dependencies, guessed paths
and another pixel corpus; **merge** the remaining interfaces/codecs/consumers
before one candidate/stub/install pass. Do not insert a large empty statistics
aggregate into every legacy CellFeature or create full nested archive copies.
Use a per-result contiguous vector, explicit new output identity and direct
versioned codec extensions. Feed the moments inside existing accumulation loops.

Next: expose typed per-result measurements, preserve measurement policy on replay,
then QCL10/11, QCA10 selected-row retention, opt-in CSV13/14 and owning SDK/consumer
integration. Close FEATURE1 only after the entire declared route is verified;
existing numerical tests are not evidence for unimplemented persistence or UI.

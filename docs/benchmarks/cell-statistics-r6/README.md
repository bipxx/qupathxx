# R6-FEATURE1: retained compartment intensity statistics

Locally verified at **60a0274e163495741044760ebec81d040124b8f5** on the reused
Windows Release/Python/GUI/GEOS/Project/OpenSlide/TIFF configuration. New natural,
area and nominal cell results retain RGB/transformed-channel sums, extrema and
sample SD for nucleus, inclusive cell and cytoplasm. Existing canonical means,
input hashes, detector choices and forest feature schemas remain unchanged.
This closes FEATURE1, not full R6/M0-M7, biology, other hosts, whole-slide budgets
or speed superiority.

The [measurement contract](../../cell-intensity-statistics.md) defines n-1 SD,
unavailable singleton SD, absent empty cytoplasm, retained output identity and
units. [Six QuPath0.7 references](../../../tests/fixtures/cell-intensity-statistics-v1/reference.json)
and independent two-pass/NumPy checks establish numerical expectations. Existing
pixels/masks are reused. One shared reducer feeds the existing kernel accumulation
loops; no second pixel read, sampler, mean calculation or per-row empty legacy
statistics object is introduced.

Native/Python accessors return owning values from a contiguous CellResult vector.
Selected classifier rows retain known statistics compactly and old unknown rows
remain absent. QCL10/11 and QCA10 preserve the new output; historical replay keeps
its saved measurement policy and canonical old encodings. Opt-in CSV13/14 adds
format, sum/min/max/range/sample-SD columns. The native export CLI accepts
`--intensity-statistics`; ImGui has **Include intensity statistics**. Default CSV
and model feature order/units remain unchanged.

| Boundary | Evidence |
| --- | --- |
| Final affected native source | [Eight roles](statistics-final-native.xml): cells, cell_records, cell_tables, log_cell_recipe, object_classifier, project_batch, cell_calibration and project_save. Independent moments cover four natural recipes, area and both nominal policies; old/new codec/replay, malformed data and legacy budget guards pass |
| First shipping candidate76ad0a2d | [69 passes/three explicit desktop skips](statistics-candidate-sdk.xml): owning values, all recipe precisions, zero/singleton/empty availability, new/old storage and replay, mixed classifier snapshots, CSV/CLI and public stubs. [Three actual ImGui flows](statistics-candidate-gui.xml) separately cover statistics export and both old nominal rechecks |
| Repaired shipping60a0274e | [Three passes](statistics-repair-candidate.xml): old exact classifier quota, new mixed known/unknown rows and native CLI statistics export. Only the legacy QCA header charge differs from the first candidate; numerical/API/UI code is unchanged |
| Final installed product | [Eight passes](statistics-repair-installed.xml): both historical quota boundaries, owning mixed snapshots and five public-stub checks. Earlier first-install [one](statistics-installed.xml) and [seven](statistics-installed-remaining.xml) passes retain their own source scope; they are not relabeled as final-source results |
| Typing and package | [Positive typing](statistics-typing-positive.log) passes; [negative typing](statistics-typing-negative.log) reports exactly three intended errors. [Integrity](integrity.json) binds480 compiler inputs,65 wheel/installed members and RECORD; generated stubs match source and both candidates |

Two real resource-compatibility defects were repaired:

- [First binding probe](statistics-binding-probe-first.xml): an old grid result
  at its exact18212166-byte workspace fails because new empty metadata added32
  bytes. Freeze the mean-only bookkeeping charge inside the existing fixed
  allowance; measured arrays keep full admission. The [boundary repair](statistics-boundary-sdk.xml)
  passes without raising the captured limit.
- [First installed classifier regression](statistics-snapshot-regression.xml):
  the cached old runtime can save/reopen a368-row snapshot at1126273 bytes, while
  the first new candidate cannot. The new empty Data vector adds24 metadata
  bytes. Preserve QCA1-9 header accounting; QCA10/new storage keeps full charge.
  The compressed [old witness](../../../tests/fixtures/cell-intensity-statistics-v1/legacy-snapshot-budget-manifest.json)
  reuses one retained cell result and existing pixels, and final reopen/resave
  retains its archive hash. [Repair checks](statistics-snapshot-repair.xml) pass.

Other retained failures/selection lessons: [initial native role](statistics-integration-native-first.xml)
uses an obsolete QCL2 fixed-algorithm rejection against explicit-tag QCL10;
unsupported tags and old version/body/domain validation remain covered after
[observer repair](statistics-recipe-repair.xml). A guessed grid-axis API stopped
an old witness capture before saving; use ScientificGridAxis. Initial typing
checked two unchanged imported equality annotations, then a guessed CLI flag
failed. [typing.ini](typing.ini) applies the intended silent-import policy to
stubs while retaining all three misuse diagnostics; runtime/stub-surface checks
are separate. A global pytest filter selected only one installed node; inspect
selection counts and run the remaining nodes explicitly, without repeating it.
Tiny classifier quota probes fell below the actual1MiB minimum; use measured
row/payload sizes to construct a valid old boundary before drawing conclusions.

Waste review: **reuse** cached native products, existing environments, source
pixels/masks and source-scoped receipts; **merge** interfaces/codecs/consumers
before packaging. Public-header changes require one slow binding compilation;
body-only repairs then refresh affected objects/links. One stub generation, two
cached wheels and two installs were needed. The late classifier boundary should
have been sized and tested before the first wheel/install. Preserve the
[first candidate](first-candidate-integrity.json) and failures; do not hide that
cost or repeat numerical/GUI/host campaigns for the one-line legacy decoder fix.
Future tasks select valid old quota witnesses and inspect actual APIs/tool options
before expensive operations. No new reader, pixel corpus, environment, general
reporting framework or full qualification was added.

Reproduction: use the isolated installed Python with
`-m pytest tests/test_cell_intensity_statistics.py`; desktop is opt-in via
`QUPATHXX_TEST_GUI=1`. The [executed backend](executed-backend.py.txt) ran from
root/build/ci/cell_statistics_r6_backend.py and relinked every packaged consumer
against the shared cache. [Build](statistics-wheel-repair-build.log), XML and
integrity receipts retain source/configuration. The earlier numeric foundation's
[source-scope.json](source-scope.json) and scalar captures keep their original
scope; they are references, not instructions to rerun campaigns.

Next: R6-SPLIT1 first traces existing split/merge and feature-domain evidence,
selecting only a demonstrated missing case. Broader vendor/context/scientific
acceptance remains in R6; resource/speed work stays R12.

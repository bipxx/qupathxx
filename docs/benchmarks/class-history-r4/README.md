# Class-only rename history admission (R4-CLASSHISTORY1)

Locally verified at runtime2892db74. This is a history-budget correctness/resource
fix, not a general CPU/RSS/whole-slide speed result or full R4/R12 completion.
All M0-M7 remain Partial.

The [installed baseline](baseline.json) at8722d96d rejected renaming Root to
Renamed on one2000-vertex polygon under a16384-byte history budget. Revision,
class and area stayed unchanged. The generic annotation journal charged full
before/after geometry payloads even though ROI data itself is immutable/shared;
this was not evidence of deep-copying the geometry buffer.

The [same installed probe](installed-admission.json) at2892db74 now succeeds
with1419 accounted history bytes, exact geometry and undo/redo. Native subtree
coverage additionally renames a large cell with4000-byte immutable properties
in3392 accounted bytes. These are the API's history-payload accounting values,
not allocator or process-memory measurements.

Rename now shares field preparation, class/count patches and the existing
ClassificationEdit atomic toggle with reclassification. It preserves reported
probability/properties for administrative rename; actual reclassification keeps
its prior probability-reset rule. Only changed object strings and class metadata
enter this journal. Parent paths, raw/resolved colors, class counts, immutable
snapshot blocks, object revisions and compound undo/redo stay at their native
authorities. No public header, SDK signature, storage schema or second journal
was added.

| Boundary | Evidence |
| --- | --- |
| Native field/state consumers | [Four roles](consumer-native.xml): classes, object_classifier, gui_replay and project_save. Dense admission, exact subtree/raw/absent-color state, property/probability identity, geometry/nucleus/parents/index, copy/undo/redo, snapshot reopen, overlap/unused classes and atomic budget/collision refusal pass; unrecorded edits and reclassification policy remain intact |
| Staged SDK | [45 passes/four existing desktop skips](staged.xml), including class/edit-state, async recovery/ownership and five unchanged public-stub checks |
| Installed SDK | [Two passes](installed.xml) plus the exact source-asserted [admission execution record](installed_probe.py) verify geometry/undo and document snapshot/recovery at the installed boundary |
| Source/package | [Integrity](integrity.json):476 compiler inputs and65 installed hashed members match wheel/RECORD. Existing public core header, binding sources and generated stubs are unchanged |

Waste review: the installed baseline established the defect before editing.
The first affected native build/checks passed, then one33.46s cached consumer
candidate reused binding/GUI products and relinked every packaged executable.
No binding-source recompile, stub generation/repackage, source/pixel capture,
new environment or broad import/schema/numerical/host campaign. One install
reused the existing isolated environment with source/path assertions. Prior
CLASS1 receipts keep their own runtime scopes. Zero product/test repair loops
were needed after the implementation.

Next: R6 context/halo sampling and replay, starting with existing CellRequest,
GridCellSampling, NominalCellSampling and retained recipe evidence. Do not repeat
this history probe or the CLASS1 integration campaign without a changed input
or unresolved risk. Broader R4/R5 gaps still need their named fixture/fault
contracts; no full-family or scientific acceptance follows from this closeout.

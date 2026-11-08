# R5-RELINK1 verified-reader handoff

Native/observer source `26b4f228dbaee536119104583705200a4d4bb5f2` makes
Project::relink retain the reader it verified. The prior code discarded it and
opened the path a second time on first use. The shared LinkedServer publication
helper now serves both lazy opening and explicit handoff, preserving the wrapper's
new generation and copying the actual reader observation. Compatibility is checked
once before project mutation. Recorded metadata, manual calibration, TIFF view/
budgets and independent old reader owners retain their existing semantics.
No public API, project schema or new dependency is introduced.

[Validation](validation.json) binds442 compiler inputs, native/wheel bytes and
all62 installed members/RECORD. Only project.cpp and project_tests.cpp differ
from the preceding native source. Public headers, bindings and Python sources
are byte-identical; prior typing checks apply within that unchanged signature
scope and the current installed generated-stub check passes.

Evidence:

- [Prior installed product](before.xml):three handoff checks fail because PPM and
  TIFF links still report unopened; rejection preservation already passes.
- [Model](model.xml):226 passes/four declared skips. All four new cases pass:
  removed/replaced PPM paths keep the verified owned pixels and historical source,
  old owners survive replacement/close, failed relink preserves path/generation/
  clean state, and TIFF relink retains observed source, axes/channel/display
  selection, budgets, raw samples, manual calibration and save/reopen state.
  Two additional observer controls reject missing/duplicated native handoff markers.
  Existing project, TIFF and source-provenance regressions are included.
- [Native](native.xml):project, qupath_project and interchange pass. The project
  marker verifies actual handoff/source/owned-pixel/old-owner behavior, including
  unlink before first use. No new suite-count convention was added.
- [Desktop](desktop.xml):the existing successful and failed F9 relink flows pass,
  including rendered replacement pixels and preserved state/view on failure.
  UI code and layout did not change; no additional rendering campaign was needed.
- [Installed stub check](stubs.log) and package/source proof pass. JPEG/TIFF static
  archives remain unchanged. Native products and explicit cache paths are retained
  locally, without distributing them here.

The archived [proof](prove_relink_r5.py) ran at the root checkout's
`build/ci/prove_relink_r5.py` using
`build/ci/project-relink-r5-env/Scripts/python.exe -I -X utf8`.
[Validation commands](validate_relink_r5.py) and per-mode JSON retain the actual
selected arguments. Original outputs/products remain under `build/project-relink-r5`;
archived helper paths describe that retained local layout.

Waste review: removed the discarded verified reader and second open; merged
compatibility/source publication into the existing local helper so initial lazy
opening and relink share one authority. One cached seven-target build and one
final install; no public-header/main-binding change or stub regeneration/repackaging.
All selected checks passed initially. Existing successful/failed desktop relink
checks were reused instead of adding a second UI test model. Prior typing and
codec evidence retain their source/scope. Build elapsed was not recorded; future
command wrappers should capture elapsed time while running, without a separate
measurement campaign. No numerical/reader/full-platform campaign or overall
performance claim follows from this structural reduction in work.

This is local Windows engineering verification. PPM retains its ordinary owned
buffer behavior; TIFF/OpenSlide keep their existing source/version/guard semantics.
A same-sized different image is still allowed by structural relink compatibility.
Persisted expected source fingerprints and explicit mismatch/replacement review
remain R5-SOURCE1. This task does not establish byte identity or lifetime file locks.
All M0-M7 remain Partial; source308 retains its own platform scope.

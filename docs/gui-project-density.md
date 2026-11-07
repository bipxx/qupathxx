# Project-wide density in ImGui

Distributed with [Windows candidate5](https://github.com/bipxx/qupathxx/releases/tag/v0.1.0-windows-candidate.5);
see the [install guide](benchmarks/windows-candidate5/RUN.md).

The new standalone desktop app adds **Project density...** in the Project pane.
It uses the existing native density API in a background worker. Open a saved
project, choose images/classes, review cohort counts and calibration, then run
into a new directory. The published candidate4 wheel and Python runner are
unchanged; they do not acquire this panel without a new app build.

1. Open the project and save edits with **Save project (Ctrl+S)**. The first
   viewport fit can itself create an unsaved view. You can save a separate copy
   using the existing destination field to preserve the original project.
2. Open **Project density...**. Check the images to include; **All images** and
   **Clear images** change only this panel's selection.
3. Enter exact window and tissue class names, one per line. Exclusions are
   optional. Choose explicit Cell classes or the visible **All Cell objects**
   checkbox. Named classes must have objects in every selected image; descendants
   are not automatically included. Zero actual Cells is allowed in all-Cells mode.
4. Choose nucleus/body centroid policy and explicitly accept curved-ROI
   approximation if needed. The existing512-segment policy applies to both area
   and membership. Native default resource quotas apply.
5. Click **Preview cohorts and calibration**. Check image IDs, window/tissue/
   exclusion/Cell counts, X/Y micrometers/pixel and per-image issues. Hover image
   IDs for full names; the image selection list also shows names. Missing classes,
   calibration or native eligibility errors block Run. Preview does no density
   kernel work and writes no output. Changing selectors clears its approval.
   These are selected-cohort counts; the CSV count column holds per-ROI membership
   counts after analysis.
6. Optionally enter a **Previous GUI run folder** to reuse eligible saved images.
   Enter a **new output directory**, with an existing parent, and click **Run
   project density**. Scroll the panel if needed to see controls/results. Run
   rechecks the saved file against the preview's SHA256 before creating output.
7. Progress reports the task's source, phase, current image and completed-image
   count. **Cancel density** requests cooperative cancellation. Open/save and
   individual synchronous geometry calls must reach a cancellation point; closing
   the app requests cancellation and joins its worker before retiring it.
8. Read the final state, image errors, CSV and report paths. On Windows, **Open
   CSV** uses the associated application; **Open results folder** opens the output
   directory. Windows may ask you to choose an application for CSV. The panel
   does not assign a default; an OS opener failure is reported in the panel.

The background worker owns a separately reopened saved project; it never receives
the live viewer's Documents or publishes edits into the workspace. Changing
displayed images or continuing work cannot redirect its saved results. Task/run
source labels distinguish its input from any newly opened project. Save dirty
edits before preparing another run. Source changes during preview/run are errors;
close/recover projects with SQLite journal/WAL/SHM sidecars before analysis.

Output files:

- `image-ID.qpxproj`: one image, native retained density record and source image/
  project metadata. Record IDs and checkpoint-local image IDs are in the report.
  Original image IDs remain distinct in the combined table. Native save_new
  refuses to replace a colliding file; other images continue after that failure.
- `density.csv` on full success; otherwise `density.partial.csv` contains only
  whole completed images. Seven context columns precede the unchanged20 native
  columns, using the same order as the Python runner. Native numeric CSV text is
  preserved. Quoting handles names/paths with commas, quotes or newlines.
- `report.json` (`qpx.gui-project-density/1`): saved source path/SHA and project
  identity, final state/error, completed/failed/row counts, and per-image record,
  checkpoint, input/result identities and errors. Unattempted images say not_run.
- `image-ID.json` (`qpx.gui-density-checkpoint/1`): bounded per-image receipt with
  source project/image IDs, exact engine, checkpoint path/SHA and saved record/
  result identities. Published after the checkpoint and before combined CSV;
  resume does not need a final report.json. Keep original checkpoint folders.

Per-image errors normally continue. Combined CSV exhaustion stops further work.
Completed historical checkpoints survive cancellation or later errors; no partial
image contributes CSV rows. The combined CSV is limited to64MiB, report16MiB,
source project1GiB, selections1000 images and1000 class selectors per role.
Native cohort/capture/work/geometry quotas remain authoritative. Single-image
save uses512MiB snapshot and1GiB file limits. These are admitted payload/file
limits, not a process-RSS guarantee. The worker hashes the source at each new
preview/run boundary and at successful completion, with file-version checks
between images and before checkpoint publication.

An eligible receipt uses native saved-record replay and compares the current
capture's role IDs/revisions/geometry hashes, Cell kind/nucleus/centroids, image
dimensions, calibration and nucleus/curve policies. Smaller resource limits
require fresh native admission/computation. Exact engine identity covers raw
source tree, compiler/flags, configuration, platform and GEOS, as in the CLI.
Reuse preserves native CSV/result identities and references the existing QPX;
it does not recalculate density or copy pixels. Unrelated metadata changes can
reuse; changed geometry/calibration/cohorts compute. Missing receipts compute.
Malformed/incomplete receipts and matching-engine checkpoint checksum, replay or
result-identity failures produce per-image errors and preserve prior files.

The UI shows computed/reused totals and reused/Fresh image states, with hover
reasons. Each resumed folder writes new receipts referring directly to original
checkpoints, so it can itself be selected for another resume. No parent/child
nesting of old/new run folders is allowed. A checkpoint interrupted before its
receipt is complete is not automatically reused. Pre-receipt GUI versions and
different engine builds compute once; their old files are not overwritten.

The [Python runner](project-density.md) retains its own resume format. GUI and
Python receipt folders are not interchangeable. Native QPX results remain
inspectable through existing APIs/editor. No model training, new readers, tissue
inference, biological acceptance, platform expansion or speed claim is added.

Build from the active source checkout using its configured native cache:

```powershell
cmake --build path/to/cache --config Release --target qupathxx_density_panel_tests qupathxx_app
ctest --test-dir path/to/cache -C Release -R "^density_panel$" --output-on-failure
```

The private controller is linked into the app; no new SDK headers, batch enum,
nanobind interfaces or wheel build are needed. [Coverage and evidence](gui-project-density-coverage.md)
record the selected desktop scope and the local development app's source identity.

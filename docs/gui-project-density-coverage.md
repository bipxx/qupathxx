# ImGui project-density coverage

Selected outcome: image/class cohort selection, preview, background progress/
cancellation, combined native CSV and per-image errors in the standalone app.
This is a desktop consumer of existing density APIs, not a new kernel or release.
All14 selected requirements are closed.

| Requirement | Status / evidence |
| --- | --- |
| Saved project requirement and read-only preview | Closed; actual desktop save/preview; controller source-byte checks |
| Image selection and exact role-class matching | Closed; controller duplicate/unknown/missing-class cases; desktop inputs |
| Explicit Cell classes versus all Cells | Closed; native kind rejection and all-Cells fixture/public route |
| Counts and X/Y calibration issues before Run | Closed; anisotropic/missing calibration tests; public preview |
| Preview invalidation for selector/source/catalog changes | Closed; stale-source controller case; final desktop image toggle clears old rows; catalog pruning in UI |
| Background owner independent of live viewer | Closed; value-only job inputs/private reopened project; source preservation |
| Native counts/area/CSV preserved | Closed; independent public pilot31/27,104006/101124px2; native CSV comparisons |
| Active cancellation retains completed whole images | Closed; slow second-image cancellation test and partial CSV |
| Per-image failure preserves existing file and continues | Closed; checkpoint collision with following successful image |
| New output directory and byte quotas | Closed; existing-output and CSV-exhaustion tests; no replacement |
| Per-image native record/checkpoint identities and replay | Closed; exact stored SHA/CSV plus native verification after reopen |
| Complete/partial CSV and JSON error report | Closed; controller complete/quota/cancel/collision routes; desktop completion |
| Readable preview/results and file-opening controls | Closed; final short-ID preview, source/completion labels, folder opener and Windows CSV chooser handoff |
| Source-scoped cached app delivery without bindings/wheel | Closed; desktop/source receipt and56-member local application bundle |

[Controller tests](../cpp/tests/project_density_panel_tests.cpp) exercise the
changed consumer. No unrelated native/Python/corpus/platform suite is required;
the density kernel, record/archive and native CSV implementation are unchanged.
The [public NuCLS pilot](benchmarks/research-density-pilot/README.md) supplies
existing rights/calibration and independent count/area oracles. Public nuclei
remain explicit centroid markers and the denominator is annotation FOV area;
this addition does not validate whole-cell or tissue segmentation.

The first test run incorrectly equated a replay's new revision-bearing result
SHA with the historical SHA. The repair retains exact persisted identity and
uses native verification for replayed numeric/geometry agreement. The original
failure remains in main build/gui-project-density/controller-tests.log.
The [native test log](benchmarks/gui-project-density/tests.log) and
[public controller log](benchmarks/gui-project-density/public-controller.log)
pass. Final [desktop/source receipt](benchmarks/gui-project-density/desktop.json)
and [evidence summary](benchmarks/gui-project-density/README.md) pin the actual
app and outputs. Passing controller evidence is reused where only private GUI
presentation/invalidation changed; no kernel or binding campaign was repeated.

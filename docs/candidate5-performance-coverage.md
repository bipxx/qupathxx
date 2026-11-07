# Focused performance coverage

Selected scope completed 2026-11-07. Evidence: [report](benchmarks/candidate5-performance/README.md).
This checklist measures the selected outcome, not all QuPath capabilities.

| ID | Requirement and decisive evidence | State |
| --- | --- | --- |
| B01 | Existing slide/reference hashes authenticated; inputs.json | Complete |
| B02 | Fixed ROI, calibration, stains, recipe and admitted capacities; native.py/qupath.groovy | Complete |
| B03 | Actual QuPath request trace establishes zero context; qupath-requests.json | Complete |
| B04 | One warmup + three timed workflows per engine; per-run reports and stage ranges | Complete |
| B05 | All four RGB/area/cell-count/nucleus/body partition gates exact; summary.json | Complete |
| B06 | Open/read/statistics/cells/density/CSV/save/reopen measured; incompatible work disclosed | Complete |
| B07 | Actual worker PID resource-only high-water RSS, startup included, oracles excluded | Complete |
| B08 | Published candidate5 failure preserved; exact affected project recovers unchanged in development | Complete |
| B09 | Both compatibility-policy clipping regressions, quantitative validation retained | Complete |
| B10 | Bounded geometry change, exact CSV/masks/WKT, original/candidate/later-control results and contrary probes retained | Complete |
| B11 | Five affected suites pass; cached binding objects reused; final app opens affected project | Complete |
| B12 | Source/runtime manifests, report, plan/waste notes, comparison retention and source handoff | Complete |

Published candidate5 packages are unchanged and do not contain these repairs.
No overall speed, biological acceptance, whole-slide segmentation, fresh-host,
cross-platform or next binary release claim follows from these checks.

Temporary comparison runtime deletion was rejected by automatic approval review
("blocked by policy"); copies remain local with preserved evidence.

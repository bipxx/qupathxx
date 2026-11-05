# Windows workflow checkpoint — 2026-11-05

User outcome and run instructions: [Windows workflow guide](../../windows-workflow.md).
Requirement-to-result audit and artifact identities: [checkpoint.json](checkpoint.json).
Native seed is MODEL1 `6219a08f1713d641633156e70f12b3fd70331d82`, source-tree hash
`432eab13b81702d423091d8bdd2113d75be9ada3ab2b9a05804128b80cccc892`.
The active worktree has later parked PRESET1 source and is not the installed seed.

| Required outcome | Actual evidence / boundary |
| --- | --- |
| Real slide and calibrated ROI | Existing pinned CMU-1.svs, ROI29185/2048/512/512;0.499um/px both axes;262,144 samples. No new corpus or QuPath capture |
| Python tissue/statistics/export/save | Updated existing examples/region_workflow.py; sdk-final/workflow.json; independent integer RGB sum/extrema/sample-SD oracle at unchanged1e-10 absolute SD tolerance; native analysis/tissue retained and replayed after project reopen |
| Component export correctness |246,394 foreground samples,11 components;11 tissue CSV rows with component sample sum matching the retained mask; one ROI statistics row |
| ImGui authors/runs/saves | installed_desktop.py, desktop-viewport/desktop-report.json and desktop.ppm: actual rectangle input, analysis/statistics and tissue ready, one of each retained, CSV succeeded, project saved; replay complete and zero reported overflow items |
| Cross-interface/native reopen | SDK and ImGui saved projects reopen; native replay verifies both record kinds. Installed app and extracted native archive successfully open their respective projects in smoke mode |
| Frozen binary identity | prepare_candidate.py verifies all65 original wheel/installed members against MODEL1 shipping receipt before native extraction. Final license wheel preserves63 original members; metadata/WHEEL/RECORD change and two project-license copies are added. No native rebuild or stub generation |
| Install/package/license | Owner selected AGPL, implemented as AGPL-3.0-only. Licensed build-tag1 wheel installed in an isolated CPython3.12 environment, NumPy2.5.4.72 installed RECORD members verified. Final native ZIP has59 files/six native images; wheel has68 files/seven native images; static dependency closure passes |
| Guide/notices/support | Windows workflow guide gives Python/native commands, required runtimes, output/ROI semantics, notice inventory, uninstall and explicit support limits. Public release and clean-machine claims remain separate |

Raw outputs and delivery files are under main-workspace
`build/windows-workflow-checkpoint`. `delivery-agpl` owns the final artifacts,
`candidate.json`, `wheel-license-overlay.json`, native/wheel runtime manifests and
`installation.json`. Its `clean-install` sibling is the isolated installed
candidate; `sdk-final` and `desktop-viewport` own successful user projects.
Failures and superseded experiment outputs retain their original directories.

## Attempts, corrections and waste

- Initial runtime lacked optional NumPy; added arrays dependency without changing
  QuPathxx. The old probe environment points to a different dirty stage and is
  ineligible for this candidate. Used the frozen installed runtime instead.
- Original example assumed coordinate order from an all-zero request. Nonzero
  ROI exposed it; changed to explicit x/y/level keyword arguments.
- A floating NumPy variance reduction differed by1.04e-10. The independent
  oracle now uses exact integer moments/Fraction before sqrt, preserving1e-10
  tolerance. This was an oracle repair, not a native algorithm change.
- Tissue-rich input exposed one-row CSV assumption; used actual per-component
  semantics and checked component sample accounting. Earlier partial sdk-tissue
  output remains; successful run uses sdk-final instead of overwriting it.
- Desktop waits at frame160 failed because no ROI was authored. Changing show
  overload/coordinates alone did not resolve it. A short diagnostic captured a
  blank view; source showed Viewport.origin is a screen translation. Negative
  translation fixed the probe; no increase in waits or product patch was needed.
- Successful desktop run returned CSV state `succeeded`, not the observer's
  guessed `saved`. Corrected the observer and continued saved-artifact checks
  without replaying the successful session. This is not a missing product state.
- Initial native manifest rejected the archive's filename; used its existing
  required candidate filename and retried only that check, without repackaging.
- Owner license decision changed packaging inputs. Repacked only metadata and
  license files, regenerated RECORD and checked the actual final install. Reused
  unchanged native/GUI/numerical evidence rather than rebuilding the engine.

The existing MODEL1/REGION1/GRID1 kernel, legacy, failure and typing evidence keeps
its original source/configuration scope. No qualification308, cross-platform
matrix, specimen campaign, new estimator/registry or speed benchmark was run.

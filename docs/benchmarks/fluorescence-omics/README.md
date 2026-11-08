# Open-format native workflow implementation

[Usage and limits](../../fluorescence-omics.md), [selected checklist](../../fluorescence-omics-plan.md).
Native implementation source99e75706; hosted Linux validation passes.
No new binary release, Python reader or numerical analysis kernel is introduced.

- [Local native tests](tests.log): all3 selected cases pass. Fluorescence fixtures
  are independently hand-encoded uint16/float32 OME-TIFF, three named channels,
  two Z planes, high-range/zero/nonfinite samples. Native raw analysis, display
  changes, CSV, saved replay, batch and rejection/stale/no-overwrite checks pass.
- Spatial tests preserve selected counts/zero observations, exact2^53 values,
  observation/feature IDs, pixel positions, existing objects and input hashes
  through project save/reopen. Malformed fields/axes/dimensions/counts, duplicates,
  resource limits and native pre-cancellation refuse; shuffled sparse entries
  retain the same values while exact source-byte identity changes.
- [Installed spatial CLI](installed-cli.log): native fixture/CLI/save/reopen and
  no-overwrite pass with PATH restricted to Windows System32.
- [Installed fluorescence batch](installed-fluorescence.jsonl): the existing
  native --kind raw command succeeds on an explicit three-channel Z1 recipe,
  also without an interpreter on PATH. The unchanged native viewer opens both
  a saved spatial-count projection and saved float32 fluorescence records. Point
  rendering and the imported historical-properties inspector are existing code.
- [Receipt](receipt.json): source identity, installed CLI/PE imports, no binding
  targets while Python discovery is disabled, all38 binding objects preserved.
  Existing optional development products were retained; this local stage is not
  a freshly qualified binary release or an entirely Python-free directory.
- Extended the existing native-only Linux job with TIFF and these native targets,
  CTest cases and an installed spatial CLI check. It retains its no-Python-target/
  product/runtime-import guards. [Hosted Linux run42516464079](https://github.com/bipxx/qupathxx/actions/runs/42516464079)
  passes configure/build/tests/install/installed-CLI checks on source99e75706;
  [job/step metadata](hosted-ci.json). It does not imply a new platform release.

[Package dependency regressions](manifest-tests.log):109 pass. Optional spatial CLI
is now an explicit entry point when present in Windows/Linux package manifests;
its dependency closure must remain Python independent. New synthetic PE/ELF
cases exercise both native and forbidden Python-linked forms. No wheel rebuild.

Waste/corrections: reused the raw batch pipeline instead of adding fluorescence
logic. The first compile needed a direct ObjectProperties include. The fixture
initially requested tile2 below the native16 minimum; then its bad-channel test
expected refusal during snapshot construction rather than at analyze_raw's actual
decoder boundary ([retained earlier result](pre-worker-boundary-tests.log)). Fixed
those test inputs without changing production validation/assertions/tolerances.
One existing cache, one component install; no binding build, HDF5/Zarr dependency,
new project schema, whole-slide campaign or wheel. Independent Python test/manifest
automation remains development code, not native runtime logic.

Evidence is synthetic engineering acceptance for the declared workflow. It does
not establish biological accuracy, vendor compatibility or broad omics analysis.

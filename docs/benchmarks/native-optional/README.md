# Native-first application and optional Python audit

User outcome: native C++ owns scientific/application implementation; Python is an
optional control interface. The audit covers all3 package modules and11 supported
examples. [Inventory](python-surfaces.json) records exact source hashes/imports;
manual role review is in the [boundary map](../../consolidation-python-boundary.md).
Absence of scientific imports alone is not the audit conclusion.

## Actual migrations

- Desktop startup was duplicated in src/qupathxx/__main__.py. Shared C++
  run_application now owns parsing/validation, project/image/recovery/demo startup,
  native layout/options and smoke failure handling. The native executable handles
  platform argv conversion; Python forwards argv and the return code. Headless
  builds still supply native help/argument validation and a missing-GUI error.
- Raw classifier grid/tile/feature defaults and checkpoint/train/predict/retain
  sequencing lived in the Python example. run_raw_classifier_workflow in C++ now
  owns them, native class selection, source matching, no-overwrite outputs and
  native resource lifetime. Python decodes/displays its native JSON report.
  The C++ CLI exposes --raw-classifier-workflow. Ordinary complete runs avoid
  the old artificial one-window save/resume round trips. Explicit partial/resume
  APIs and CLI remain native; their feature is not removed.
- Plain CMake defaults Python OFF. pyproject.toml explicitly enables it for
  Python wheel builds; the actual scikit-build settings reader confirms that.
  No native product launches Python to implement a workflow.

## Decisive checks

[Baseline](baseline.json), [optional interface](python-native.json), [C++ CLI](cpp-native.json)
and [Python-disabled installed CLI](cpp-only-run.json) match36 rows/four windows
and exact model/prediction hashes. Source bytes remain unchanged. Existing output
is refused without changing its contents; native numeric conversion rejects
invalid interface numbers before filesystem work.
[Control tests](controls-tests.log):11 passes. [Enabled GUI launch tests](launcher-tests-enabled.log):
8 passes, covering both entry points, missing sources, selected stored image,
empty/recovery/moved recovery and repeated native startup on the existing Windows
1MiB stack. The initial attempt skipped opt-in GUI tests; enabling the established
flag supplied actual coverage, not acceptance-by-skip.
[Public interface/stubs](public-api-tests.log):15 passes;
[generated-stub check](stubs-check.log) passes.

[Python-free configure/build/install proof](cpp-only-proof.json) clears the old
cache option and observes its new OFF default, disables Python/Python3/nanobind
find packages, and checks the generated CMake File API target graph. No _native
or nanobind target exists; app/batch/region/core targets remain. The freshly
installed native tree contains no .py/.pyc/.pyd files. CMake was invoked through
its native executable, not its Python launcher. Selected native
[workflow contracts](cpp-only-tests.log) pass. This uses the one existing cache,
not a competing checkout or build. The additional default headless source branch
also [compiles, links and exercises the exported native application entry](headless-entry-tests.log)
against core alone; this catches GUI-link dependencies that a full build hides.
The [same entry contract](gui-entry-tests.log) passes in the restored GUI build.

With PATH restricted to Windows System32 (no interpreter directories), installed
C++ CLI training/prediction succeeds, verifies the persisted stream, and the
native GUI opens both empty and saved-analysis projects. [Runtime closure](cpp-only-runtime.json)
records all6 actual PE binaries/imports/delay imports/forwarders: dependencies
resolve to bundled/declared native runtimes, with no Python DLL. This establishes
native build/runtime independence on this Windows host, not a fresh-host or new
platform support claim.

The cache and development install were restored to the optional Python-enabled
configuration after the native-only test. [Binding reuse](binding-reuse.json):
only two small binding objects changed for new entry points;36/38, including
bindings.cpp, were reused. All38 survived the native-only/restore cycle unchanged.
[Final receipt](receipt.json) identifies the installed development source.

## What remains Python

Native reexports, the argv forwarder, owner-thread event/cancel/callback adapters,
and optional caller examples. Examples may choose documented caller parameters
or sequence native API calls; they are not implementations needed by the native
GUI/CLI/SDK. Cohort rules, recipes/reuse, geometry, scientific values, import data
interpretation, persistence and authoritative exports remain C++. Independent
tests/oracles/fixtures/benchmark analysis/build automation may use Python; native
products do not import or execute them. This does not claim the entire repository
or development toolchain has been rewritten in C++.

No segmentation algorithm/schema, license, new reader, broad corpus/performance
campaign or new binary release is selected. Candidate7 remains frozen. Stop this
bounded audit after source handoff; future product policies go into native services.

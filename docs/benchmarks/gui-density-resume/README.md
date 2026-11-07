# ImGui density resume evidence

Completed2026-11-07: actual standalone-app first run computed2 public-project
images; the second run selected that folder and completed with0 computed/2
reused images. No QPX or pixel copies were created in the resumed folder. All
files from the first run and the source project remained byte-identical.
Both CSVs contain31/27 nuclei in the annotated FOVs, with identical20 native
columns. [Guide](../../gui-project-density.md); [10/10 coverage](../../gui-density-resume-coverage.md).

- [desktop-computed.png](desktop-computed.png) and
  [desktop-reused.png](desktop-reused.png) show the actual controls, action
  states, source and computed/reused summaries.
- [desktop.json](desktop.json) records the verified app/engine/source identity,
  reference-only reuse and exact native CSV agreement. [verify.py](verify.py)
  checks actual outputs and saved-record replay without executing density kernels.
- [tests.log](tests.log): density_panel and density_resume both pass,2/2 in1.64s.
  [Resume tests](../../../cpp/tests/density_panel_resume_tests.cpp) prove0 kernel
  calls for unchanged results and chains, partial-receipt recovery, reduced-limit
  admission, changed geometry/calibration, unrelated metadata edits, engine
  changes, duplicate JSON/checksum/native replay refusal and folder separation.
- [first-report.json](first-report.json), [reused-report.json](reused-report.json)
  and [reused-density.csv](reused-density.csv) are actual desktop outputs.
  The checkpoint pointers deliberately reference the first run's retained files.
- [source-inputs.sha256](source-inputs.sha256):520 raw inputs aggregate to
  e83ae913ef42993a7ac66afb92ca749354470019afa37e642bde6aea5d0f6698,
  matching both receipts' native engine. [application-files.json](application-files.json)
  pins all56 bundle members. Build base:b77f4b69+dirty; the exact raw source tree
  is authoritative. No clean-tag or fresh-host acceptance is claimed.

Local app:main build/gui-density-resume/application/qupathxx_app.exe. Previous
app and outputs remain intact. Reader DLLs, fonts and license notices are copied
unchanged from the previous verified app bundle; only the executable changes.
Published candidate4 wheels/SDKs and the Python consumers are unchanged.

The existing pinned MIT nlohmann JSON target and strict parser are reused. The
private controller target is instantiated only for GUI/tests, including when
the optional interchange feature is disabled. No new decoder, dependency version,
binding, public SDK field, wheel, dataset download or numerical kernel is added.

Receipts require exact engine and native captured-input identity. Pre-receipt
GUI versions compute; changed builds compute. Smaller quotas use fresh native
admission. Eligible corrupt checkpoints fail per image, without replacement.
Resume reads per-image receipts even if no final report exists, and resumed
receipts point directly to original QPX files. Keep those original folders.
Reuse is not a fresh numerical verification. GUI and CLI receipt formats differ.

No implementation/test run failed. One review pass added required-field checks
before engine fallback and filesystem-identity checks for Windows case aliases;
only the affected suites were rerun after those input-boundary changes. One
cached app build followed settled consumer interfaces. No unrelated kernel,
platform/corpus qualification or packaging campaign was repeated. The public
fixture and independent numeric oracles were reused within their declared scope.
No biological, segmentation, speed-superiority, new-platform or binary-release
claim is added. This selected outcome is closed.

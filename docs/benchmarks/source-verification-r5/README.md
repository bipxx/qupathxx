# R5-SOURCE1 persisted source verification

The native Project now retains immutable source-file associations and exposes a
review/apply workflow through C++, Python and ImGui. Changed bytes, changed
compound dependency metadata, or a changed observed source require explicit
acceptance. Save/reopen retains the baseline without opening or hashing images.
Matching-byte moves preserve historical facts; accepted replacements start a new
baseline without rewriting retained analysis provenance. Schema15 adds bounded,
checksummed QSV1 rows and transactional migration from readable schemas1–14.

[Validation](validation.json) binds final native source ca009219, first candidate
a66c8af5 and test observer7b6f871e separately. All446 compiler inputs match the
final product; all62 installed wheel members, RECORD and source stubs verify.
Only gui.cpp and the corrected native project test changed after the first
successful candidate. Earlier results retain their original source scope.

- [Original model](model.xml):687 passes/ten declared skips, including saved
  associations, matching moves, explicit replacement, stale review/file changes,
  snapshot retention, migration/rollback, corrupt/orphan/embedded refusal,
  unchanged rechecks and compound dependency changes.
- [Final identity/source workflow](final-identity.xml):18 passes/three desktop
  skips. This verifies actual final product identity and the source workflow.
- [Original native suites](native.xml):four pass, one test caller fails because
  it omitted required explicit acceptance. The [observer correction](native-final.xml)
  passes after an executable-only rebuild against unchanged core/product bytes.
  The [final-product project suite](native-ui.xml) also passes.
- [Corrected desktop](desktop-ui.xml) has two apply/save passes. The remaining
  decline assertion incorrectly required a clean viewer despite camera-state
  persistence; the [single corrected case](desktop-ui-final.xml) passes while
  proving the source and saved baseline stay unchanged. These are three distinct
  successful desktop outcomes, with real Ctrl+S and save/reopen in apply cases.
- [Final stub check](stubs-ui.log), [valid typing](typing-valid.log), and
  [three intended invalid calls](typing-invalid.log) pass their contracts.

The [declined review capture](review-declined.png) shows the byte mismatch,
checksum and explicit acceptance control; [accepted replacement](replacement-applied.png)
shows the returned workspace. Original failures are preserved: two missing-API
[baseline failures](before.xml), the first [compiler failure](build.log), native
caller failure and [disabled-modal desktop failures](desktop.xml). Moving the
modal outside the disabled editor scope fixed actual apply/accept interaction.

Waste review: three native build attempts (668.13s failed,546.58s successful,
70.11s GUI correction), plus a17.10s test-executable-only correction. Two candidate
product installations total, one final corrected installation. The GUI fix reused
stubs and model/typing evidence; the decline test reran only its failed case.
Stable current-reader verification avoids reopening/decoding, preserves generation
and keeps unchanged rechecks clean. The viewer invalidates tiles only when the
reader changes. No unrelated reader/numerical/platform campaign was repeated.

The archived [proof](prove_source_r5.py) ran from root `build/ci` with
`source-verification-r5-ui-env/Scripts/python.exe -I -X utf8`.
Helpers/commands retain the original local paths under `build/source-verification-r5`.
Native products/wheels stay local; hashes and cache paths are in validation.json.
This is Windows local engineering verification, not a release or speed claim.
Primary hashes plus companion metadata do not verify every compound-source byte,
and historical associations do not prove which bytes a past decoder consumed.
Broader R5 and all M0-M7 remain Partial. Next reconcile actual existing recovery
coverage before implementing another fault; do not recreate delivered kill/cleanup
workflows from stale documentation.

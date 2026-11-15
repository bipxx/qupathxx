# Molecular desktop workflow, end to end

Delivered locally on Windows x64,2026-11-15. Frontend source commit
`bda67ad86bd750eaebedc8f5d96935c2907dfb27` follows
`cee52459b15c9da57117742705a7823cfb59e2ba`; the tested candidate products are
identified in `build/gui-aggregation/results.json` and its source snapshot.
The [user guide](../../molecular-desktop.md) covers the complete selected outcome:
full-source QC/normalization, ROI summaries/pseudobulk, PCA/clustering and
replicate-level differential expression in Genes.

Published as [Windows candidate9](https://github.com/bipxx/qupathxx/releases/tag/v0.1.0-windows-candidate.9).
The [publication receipt](publication.json) verifies all seven uploaded assets,
public download routes and the release tag without changing the frozen ZIPs.

## Completion evidence

| Requirement | Decisive evidence |
| --- | --- |
| Shared native authorities and full source universe | Controllers call existing typed C++ services on owned worker inputs. The connected3-feature/4-observation fixture displays one overlay gene but fits all3 features; actual QC totals are1000 for every observation. Native A02/A03/A04 scientific services remain the authorities. |
| Explicit normalization and method | Actual QC/log-normalization/Keep controls feed the new normalization checkbox. The fit retains that exact full-source recipe/domain. Explicit uncentered SVD is exercised, then the original centered raw fit identity is restored. |
| PCA options and readable results | Native components/clusters/complete UInt64 seed and advanced controls are exposed. Authored means and sign-invariant score magnitude match analytical expectations. Results show actual stored parameters separately from next-run drafts. |
| Exact fitted lifecycle | Actual archive export, Keep/one-undo, Remove/Undo, project save/open, exact stored-array open and explicit refit verification pass. No open operation silently refits. |
| Pseudobulk to DE | Actual selected-ROI capture/Run/Keep creates4 explicit independent declared subjects. DE opens that kept pseudobulk; empty group inputs refuse, then explicit A/B groups and target1000 produce2 replicates each. |
| Native statistics and missing values | Analytical gene0 effect is ln2, with p=`1-1/sqrt(2)`. The full-source correction universe has2 testable genes; gene2 is all-zero with absent p/q. Native A04 numerical/status/replicate-consolidation evidence is reused. |
| DE lifecycle | Actual Keep/one-undo, Remove/Undo, native saved-recipe reopen and project save/open preserve exact identity. |
| All outputs | Actual embedding/loading CSV, scatter SVG and DE gene/replicate CSV bytes match native writer outputs. Existing destinations and a protected nonexistent path refuse without replacement dialogs. A02 summary/membership exports pass their existing GUI test. |
| Context, jobs and persistence | Both new workflows exercise blocked/stale Keep and cancellation without stamping old previews fresh. Actual Genes window closing/reopening preserves the fit. Cross-image retirement with an active export leaves no dangling watcher; nonspatial retained-matrix PCA and DE routes pass. |
| Scientific input preservation | Authored original matrix bytes remain exact. Existing Genes tests retain native point registration/colors and unchanged raw counts; A02 retains exact UInt64 values above2^53 and up toUINT64_MAX. |
| Usability/localization | Normal/French200 captures inspected; no horizontal layout overflow. Stored seed/target are drawn exactly despite different next-run drafts.130 new captions across the combined A02/A03/A04 work each have15 translations. Vertical scrolling at200% is expected. |
| Native application | Final app opens the saved workflow project in smoke mode with PATH restricted to runtime/System32 and Python environment variables removed. Input project bytes remain exact. |

Final connected fixture/captures:
`build/gui-aggregation/gui-molecular-438133752242600/`.
The related final A02 fixture is
`build/gui-aggregation/gui-aggregation-437522405444300/`.
`molecular-final.log`, `gui-tests.json`, `native-reduction-verified.log`,
`high-seed-compatibility.log` and `app-smoke.json` hold the local results.

Four GUI test executables pass: `gui_molecular_workflow`,
`gui_expression_aggregation`, `gui_normalization` and `gui_gene`.
The affected native `expression_reduction` regression suite passes, including
its independent oracle, cancellation/bounds, seeded exact archives and native
CLI/CSV/SVG routes.58 design/translation/layout/export/notification/vocabulary
source contracts pass. The connected test skips explicitly with CTest code77
when required optional native capabilities are disabled; this tested candidate
has project/matrix/algebra/statistics enabled and executed every assertion.

## Native correctness repair and frozen products

The connected test found a pre-existing archive-boundary bug: fitting accepts
UINT64_MAX as a seed, but mixed signed/unsigned JSON comparisons rejected its
saved archive. The native integer helper now compares explicitly typed values.
Negative, float, string, boolean and over-bound inputs still refuse. New native
regressions cover0,INT64_MAX,INT64_MAX+1,UINT64_MAX and exact high-seed archive/
refit identity. The previously failing project also opens and refits with its
original exact fitted identity under the corrected candidate.

Frozen SDK core SHA-256 remains
`4bf957c9ea6e46294c11b9bfb7d9c2496cf4fca607a9c28ba837768d301eb6bb`.
The distinct corrected candidate core is
`c6467b5c3580dee5282565af1abd597750eef7d9cb0749235bad2d89efde25b1`.
Exactly five reduction members were rebuilt with the retained native compiler
commands and replaced in a new library; all other members were retained.
`native/build.json` records sources, objects, members and both library identities.
No public ABI, archive schema, scientific tolerance or fitted identity changed.
All71 protected frozen/current product hashes remain exact. Frozen earlier SDK/
wheel packages were not rebuilt; this fix is scoped to the new source/candidate.

## Build, corrections and handoff

MSVC19.44.35225.0,Release `/MD`,C++17,UTF-8,ImGui193/32-bit glyphs. The local
development controllers and exact response files are under
`build/gui-aggregation/`:

```powershell
.venv\Scripts\python.exe build/gui-aggregation/build_native_boundary.py
.venv\Scripts\python.exe build/gui-aggregation/build_frontend.py
build\gui-aggregation\runtime\Release\qupathxx_gui_molecular_workflow_tests.exe
build\gui-aggregation\runtime\Release\qupathxx_expression_reduction_tests.exe
```

The standalone app is
`build/gui-aggregation/runtime/Release/qupathxx_app.exe`; its matching runtime
DLLs/assets are alongside it. Development Python orchestrates compilation/tests;
the app and scientific services do not require Python. No new dependency,
restricted material, model or reader was introduced.

Costs/corrections are preserved in the current card and failure logs: a Windows
`near` test-helper macro collision; caption vocabulary/helper lint corrections;
the newly exposed native high-seed decode bug (the first test dereferenced the
failed open, now guarded); a retired forward-slash oracle path fixed in compiler
command remapping; one Windows executable-lock failure from an overlapping
relink, then sequential relink; and a render-driven stored-parameter clarification.
There were two combined GUI compilations plus scoped native/test repairs and
relinks. Existing native/binding objects and accepted broad qualifications were
not rebuilt/replayed. The A02 passing source/products were preserved before
extension in `a02-before-end-to-end/`.

The selected desktop plan is complete locally. It makes no broader QuPath
parity, biological independence/discovery, platform, performance or new package
publication claim. The owner's follow-up continuation selects the source commit;
Git history records its identity. Before that handoff, all22 source files matched
the passing checkpoint, all six executables/corrected native candidate and all71
protected product hashes were exact. Only these handoff notes then changed; no
product build was repeated. The owner's subsequent continuation selected push,
desktop/source packaging and isolated install/use/removal, delivered below.
SDK/wheel repackaging remains unselected. The later owner-selected publication
of these exact desktop assets is recorded below.

## Windows desktop package handoff

Source commit bda67ad8 is pushed to origin/main. The portable candidate and
directions are in [the candidate guide](../../windows-molecular-desktop-candidate.md).
Artifacts are retained under `build/gui-aggregation/molecular-delivery/`:

| Archive | Bytes / members | SHA-256 |
| --- | --- | --- |
| `release/qupathxx-windows-x64-molecular-desktop-bda67ad8.zip` | 28,164,289 /67 | `0d236f9814154464681a3084f3016b04eb21382096fd375088181e3eac9eab20` |
| `release/qupathxx-windows-x64-molecular-source-bda67ad8.zip` | 6,685,815 /1190 | `0c8efa26009967d0df8e970b2c6b46743d667512d1692edbab69a01b233e6940` |
| `dependency-sources/qupathxx-dependency-sources.zip` | 118,600,406 /17 | `89f4882e9b26b65681fcb11ad63d8048121b743b0ff8b7ed75c480080a6ec0ad` |

The current corresponding-source input SHA-256 is
`786dc16d203851366e030151cec98255c60f50d71db5d89710735c97e7bf6586`.
The finite `windows-molecular-desktop` profile records all1186 source inputs/
support files and66 runtime inputs; four generated source metadata files and
one runtime RELEASE.json complete those ZIPs. All member hashes and open-license
lineages pass before and after ZIP creation. The fifteen dependency archives
retain their original reviewed bytes. Runtime notices/fonts/DLLs retain theirs;
ImGui's17 retained source members and the UI templates match compiled inputs
after CRLF/LF normalization. The six changed third-party notice source files are
EOL-only, with their original license declarations. Existing inventory records,
profiles and scientific inputs were not reclassified or changed. Validation
research data and the development probe are excluded from the ZIPs.

`RELEASE.json` records app SHA-256
`226fcf9b7e7f67b1e6f53b15faa00d857038fe6d6139c62febdf4a4086aca6d3`,
the retained base/core correction identities above, six correction-source hashes,
five removed/added library members, frontend/compiler configuration and source
correspondence. The embedded historical native manifest still describes its
retained base. No monolithic new SDK or reproducible-binary claim is made.

The installed check used a new external prefix:
`%LOCALAPPDATA%/qupathxx-validation/molecular-bda67ad8`.
The public-native development probe compiled/linked once against the retained
headers, corrected core and dependency closure; no engine/GUI rebuild occurred.
Its source is `tools/ci/desktop_delivery_probe.cpp`, with response files/logs in
`build/gui-aggregation/desktop-delivery-probe*` and
`qupathxx_desktop_delivery_probe-link*`.

The representative retained Tabula Muris kidney project has23,433 genes,865
cells, six independent declared mice (two female/four male) and17,002 testable
features. The native probe opens it, saves a relocated owned project, reopens
and replays the exact stored recipe, then writes gene/replicate CSVs. Record
`3a56e5983b8bd260428ffe950c57d6ccaa07154109dd6a0f7a4016e8ffb0be17`
and result
`41ecfcf7350f3c1c05460b4252958f82d91a0c40249e4147e8acb47b7bfecfd9`
remain exact. Both CSV hashes match the retained A04 references byte for byte;
original matrix/project/reference files remain unchanged. This reuses the
admitted A04 numerical/association evidence, not a new biological claim.

The installed app then opens that relocated project in smoke mode. PATH contains
only Windows/System32 and Windows; Python environment variables are removed.
All three native module paths captured by the probe resolve inside the installed
app directory. Authenticated fonts and UI resources load beside the executable
through the existing native resource locator, without source-tree fallback.
The saved project, sidecar and exports remain unchanged by GUI startup.

Manifest-owned removal passes after closing all prefix processes:68 owned files
(67 desktop members plus the unshipped development probe) are removed, while all7
protected project/sidecar/CSV/report/preferences/user-note files retain exact
hashes. It removes individual owned files, not the research directory. The local
receipts are `release/admission.json`, `dependency-sources/dependency-sources.json`,
`installed-check/handoff.json`, `installed-check/commands.json` and `completion.json`.
This is a clean per-user prefix on the existing Windows workstation, not a fresh
VM or another platform. MSVC runtime, Windows and OpenGL driver are external
prerequisites; no code signing or stable publication is claimed. The local
handoff initially stopped before upload; the subsequent publication is below.

Development check command (use `-B` to keep the frozen source stage free of caches):

```text
python -B tools/ci/check_desktop_handoff.py --release RELEASE --dependencies DEPENDENCY_SOURCES --probe NATIVE_DELIVERY_PROBE --reference RETAINED_A04_REAL_DIRECTORY --work NEW_EXTERNAL_PREFIX --report NEW_REPORT_DIRECTORY
powershell -NoProfile -File tools/ci/remove_native_handoff.ps1 -Receipt NEW_REPORT_DIRECTORY/handoff.json
python -m pytest -q -rs tests/test_material_staging.py tests/test_candidate_source_admission.py tests/test_native_release_admission.py tests/test_desktop_release_admission.py
```

Packaging checks:35 pass; one symlink-creation test skips because this Windows
host cannot create symlinks (the Linux CI retains it). Tests cover changed,
missing, extra, restricted, source-identity and substantive UI mismatch refusals.
All71 protected frozen products and all six validated app/test executable hashes
remain exact. Correction cost was one missing-file diagnostic in the new stager;
two import-generated bytecode files were removed from the source stage and its
1190-member admission rechecked, with archive bytes unchanged. No full platform,
binding, corpus or numerical qualification was repeated. This closes the selected
desktop delivery; source publication is pushed and the frozen assets were ready
for the subsequently selected prerelease publication below.

## Windows candidate9 publication

The owner selected publication after the completed local delivery. Public
[candidate9](https://github.com/bipxx/qupathxx/releases/tag/v0.1.0-windows-candidate.9)
targets packaging commit `a9422a1b8cc6a57b02281b935cee51cc33de63d1`; the frontend,
native-base/correction, corresponding-source and three ZIP identities above stay
unchanged. No SDK/wheel, product rebuild, numerical rerun or new platform claim.

Seven assets are published: the three original ZIPs, `RUN.md`,
`release-manifest.json`, `SHA256SUMS.txt` and the75,070-byte
`qupathxx-molecular-verification-candidate9.zip`. Its ten finite metadata members
retain admission/generated-material records, the compiled-input fingerprint,
sanitized install/removal/completion receipts, the passing CI result and license/
scope notes. Research files, the probe executable and build products are excluded.
All generated metadata and actual verification-ZIP members pass material admission.
No product archive was copied or repackaged for uploading.

[publication.json](publication.json) confirms public/non-draft/prerelease status,
the exact tag commit and all seven server-stored sizes/SHA-256 digests against
their admitted local files. Anonymous downloads of `RUN.md`, checksums and the
manifest match exact bytes; anonymous HEAD requests for all four ZIPs return200
with their expected lengths. All eight earlier release identities/assets remain
unchanged. [release-manifest.json](release-manifest.json) and
[SHA256SUMS.txt](SHA256SUMS.txt) are exact copies of the published metadata.

The already completed [material-admission CI](https://github.com/bipxx/qupathxx/actions/runs/43749635882)
passes all36 controls on Ubuntu; this supplies the Windows host's skipped
symlink-capability check without a new product/platform campaign. The scientific
and installed-route evidence retains its original support scope.

Publication tooling/evidence is retained in
`build/gui-aggregation/molecular-delivery/publication/`. Two control-layer
corrections were needed: decode GitHub API text explicitly as UTF-8 on Windows;
query an existing draft by release ID because the tag endpoint omits drafts.
There was one upload, with no release recreation or asset retry. Prior source
notes inside the frozen ZIP intentionally retain their pre-publication state;
the released RUN/notes and current guide record publication. This completes the
selected prerelease handoff; a research pilot or further feature work is not
selected by this continuation.

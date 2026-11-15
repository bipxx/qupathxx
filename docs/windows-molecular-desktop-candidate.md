# Windows molecular desktop candidate

This standalone Windows x64 candidate delivers the selected
[Genes workflow](molecular-desktop.md): full-source QC/normalization, regional
summaries and raw pseudobulk, PCA/clustering and replicate differential expression.
It is a portable desktop package; Python, Java, a wheel and an SDK are not needed
to run it. The earlier published packages keep their own identities and support.

The local handoff is under `build/gui-aggregation/molecular-delivery/`:

- `release/qupathxx-windows-x64-molecular-desktop-bda67ad8.zip`
- `release/qupathxx-windows-x64-molecular-source-bda67ad8.zip`
- `dependency-sources/qupathxx-dependency-sources.zip`

Keep the matching `admission.json`, `generated-materials.json`, source inventories
and dependency receipt with those archives. Archive SHA-256 values and the
installed-route result are recorded in the
[workflow receipt](benchmarks/molecular-desktop/README.md). No release upload is
part of this handoff.

Extract the desktop ZIP into a new directory that you can write to. Keep the
executable, three DLLs, `fonts`, `ui` and `licenses` together, then launch
`qupathxx_app.exe`. Open a project through Project, or from PowerShell:

```powershell
& 'C:\path\to\qupathxx_app.exe' --project 'C:\research\study.qpx'
```

Keep projects and their managed sidecars together in a research directory outside
the application directory. Outputs use the native new-only export contracts.
For removal, close the app and remove only the extracted package members;
preserve projects, sidecars, exports, preferences and other user-added files.
The development check uses `remove_native_handoff.ps1` with an authenticated
owned-file receipt to verify this behavior.

## Candidate identity and support

The frontend source is commit `bda67ad86bd750eaebedc8f5d96935c2907dfb27`.
`RELEASE.json` binds the exact executable, retained native base and distinct
five-unit reduction correction. It also records the current corresponding-source
input fingerprint and the six correction-source hashes. The embedded historical
native build manifest identifies the retained base; it does not identify a newly
rebuilt monolithic SDK. The packaged source includes the corrected implementation.
UI templates and the pinned ImGui code correspond after CRLF/LF normalization;
the validated runtime bytes remain exact. No pixel or numeric normalization is
part of packaging.

The build uses MSVC19.44/v143, Release `/MD`, C++17, ImGui1.93/32-bit glyphs and
the pinned native dependencies in the source inventory. Windows, the MSVC runtime
and a working OpenGL driver remain external prerequisites. A fresh external
per-user prefix on the existing Windows workstation is the installation scope;
this does not establish fresh-VM, other-platform, stable-release, performance or
biological acceptance. Scientific/UI evidence retains the scope in the linked
workflow receipt; the delivery check adds project relocation, exact saved replay,
CSV identity, application startup, package-local resources and owned-file removal.

## Source and notices

The application is AGPL-3.0-only. Each dependency and font retains its own admitted
license and required notices under `licenses`; restricted/NC material is excluded.
The source ZIP contains the exact recorded CMake input files and finite reviewed
build/support files. The dependency ZIP contains fifteen pinned reviewed source
artifacts, including code-only OpenCV and ImGui selections. The full upstream
carriers containing omitted sample images, models or unused binaries are not
included. Research validation data is outside the package.

Use the dependency overrides and offline build directions in
[the native material guide](native-release-materials.md#corresponding-source-and-notices),
with `QUPATHXX_BUILD_PYTHON=OFF` and `QUPATHXX_BUILD_GUI=ON`. This is corresponding
source and retained build provenance, not an offline compiler/SDK distribution
or a bit-for-bit reproduction guarantee. The desktop-only staging route is:

```text
python tools/ci/stage_desktop_release.py --runtime REVIEWED_RUNTIME --output NEW_RELEASE
python tools/ci/collect_candidate_sources.py --output NEW_DEPENDENCY_OUTPUT --cache REVIEWED_SOURCE_CACHE --inventory tests/fixtures/manifest.json --inventory assets/fonts/font-source.json --profile windows-molecular-desktop --source-id SOURCE_INPUTS_SHA256
```

`windows-molecular-desktop` is a finite reviewed profile in the existing material
inventory. Changed/missing/extra/unadmitted files, a mismatched source companion
or substantive UI-source changes refuse. Updating a build or dependency requires
a new review and candidate identity, never relabeling these bytes.

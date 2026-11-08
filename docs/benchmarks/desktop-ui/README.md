# Desktop UI and language validation

The QuPath-style desktop and 16-language chooser were developed from integration
commit `4546b6e6` on `feature/qupath-ui-design`. This folder preserves the existing
source-scoped validation and failure logs before retiring that temporary worktree.
It does not qualify a new scientific runtime or release.

- [UI validation](ui-design/pytest-final-verified.log): 56 selected real-GL cases.
- [Final language/UI validation](ui-language/pytest-combined.log): 80 passes,
  five unrelated installed-subprocess cases deselected.
- [Native localization checks](ui-language/native-tests.log): 126 catalogue
  entries across 16 languages; formats, identities, locale selection, search,
  preferences and per-session restoration.
- [Language receipt](ui-language/receipt.json) and [UI receipt](ui-design/receipt.json)
  identify the original source/build locations. Those paths are historical.
  The corresponding C++ inputs are retained in Git; the source hashes are checked
  again at merge. The receipts' stored paths do not identify a release installation.
- Intermediate/failing `.log` files are retained alongside the final logs.
  The build scripts record how the GUI-only overlay reused native libraries and
  38 binding objects without rebuilding the scientific core.

The full temporary `build/ui-design` and `build/ui-language` contents, including
passing preview products, synthetic projects, both preview recovery snapshots,
failure captures and disposable build/test outputs, are archived locally under
`build/ui-design-handoff-20261108/worktree-build.zip` in the main workspace.
`archive-receipt.json` there records every archived file's size/SHA256 and the
verified archive identity. The local pre-merge `AGENTS.md` router is preserved in
that handoff directory as well. Archived source under
`build/storage-cleanup-20261105` and the active integration cache/runtime are
outside this cleanup.

The initial catalogue covers desktop chrome and common controls; advanced text
falls back to English. Additional complex-script shaping and full advanced-dialog
translation are not claimed. See the [workspace guide](../../docked-workspace.md).

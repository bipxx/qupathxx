# Grouped header and useful Help

The selected header redesign is delivered: File/Edit/View/Analyze share one row,
Help stays at the right, and translated groups collapse under Menu when needed.
The existing gear owns General/Appearance/Advanced settings. Compact API viewers
share those settings under Edit. View groups Panels/Layout; TMA stays under
Analyze's shared tool catalog. Existing handlers and shortcuts remain authoritative.
Quick reference/F1, Documentation and About use one registered session Help
dialog with Close/Escape. Explicit link clicks use the checked native platform
callback; failure is visible and links can be copied.

## Main integration

The owner subsequently requested a commit and merge to main. The isolated
`feature/grouped-header-help` branch starts at
`33d6c1aefaec6d3ac60bc9472b23fdce65fd206a`. It reuses the qualified header,
Help and shared Analyze menu helpers, binding the twelve routes to main's
existing controllers. Main retains its native interchange availability and
handlers. Concurrent dirty reader, scientific and workflow changes are excluded.
The branch adds43 required caption rows, with15 translations each; seven of
these were already present in the wider original integration baseline.

The separate `build/ui-header-merge` consumer rebuild and replay establish this
main-based integration's behavior:20 source contracts and `header-tests-01.json`
exit0, including all eight actual interactions and one placement probe. Project,
calibration and RGB assertions remain exact. All14 native-library and38 binding
object hashes remain unchanged. One full frontend build and one replay pass on
the first attempt; core/binding rebuilds and packages are zero. `integration.json`,
`before-build.json`, `inputs/`, `final-inputs/`, `results.json` and logs preserve
its distinct source/results. The frozen engine remains separate from the new UI
source. This is a local commit/merge; no package publication or push is selected.

## Original integration source and runtime scope

The new UI consumer and evidence are in workspace `build/ui-header-grouping`.
`runtime/Release/qupathxx_app.exe` uses the retained native engine; About labels
that identity **Engine source SHA-256**. This is a local frontend delivery,
not a new wheel/SDK, release or scientific qualification.

- Final compiled-context fingerprint:
  `49988b2fc25e7d1df87d1f1c962cfda202af5a61ac8136d1a424c3ddafdaaa93`.
- Retained engine source:
  `bc8ca7384be33c6308130affeec66e8d84cf8b40f2bfaf91dc244651f57990eb`.
- `source-final-inputs.sha256`, `final-inputs/` and `source-final.json` preserve
  exact source bytes and UI hashes independently of later active-tree edits.
- All14 cache native-library hashes and38 binding-object hashes are unchanged.
  The new local GUI archive includes Help as the14th private registered dialog;
  test relinks must use that matching archive.

## Decisive validation

Twenty affected source contracts pass. `header-tests-04.json` records exit0 for
the native organization executable's `--header-grouping` route. It checks actual
production menus, settings and dispatch; narrow/translated geometry; command
enablement; F1/Ctrl+Shift+L and modal guards; all Help pages and Close/Escape;
preference retention through Reset; and one explicit link launch/failure using a
fake native platform callback. Passive rendering launches nothing.

Eight GLFW interaction replays plus one read-only placement probe cover normal,
French200% and narrow French200% views, language dismissal/history and light-theme
Reset. Project bytes, revision, calibration and RGB pixels remain exact. Retained
`run-04` captures establish readable values and usable controls, without claiming
pixel equivalence. The existing organization preflight also passes within its
previous UI scope; no broader biology, reader or speed claim follows.

## Cost and corrections

One full9-unit GUI build, one affected GUI increment and three test-only builds
used the existing MSVC cache. Core/binding rebuilds and packaging: zero.
Failed runs01–03 and their captures remain beside the passing run. Fixture
corrections cover a hover-open submenu toggled closed by a second click, nested
popup ID ownership, explicit current-context restoration and a settings anchor
above the footer. Tests now use actual emitted widget IDs and native pane bounds;
the original reachability, modal/history and exact-data assertions remain.

A separate authored test-project copy and launch metadata are under `preview/`
and `preview.json`; this does not replace or alter user research sessions.
Subsequent selected UI work must preserve this runtime and source evidence and
use a separate output directory.

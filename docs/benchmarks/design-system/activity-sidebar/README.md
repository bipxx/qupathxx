# Activity sidebar, slide toolbar and compact chrome

The owner requested a VS Code-style activity rail, drawing tools above the slide,
no side-panel dock dropdowns and less excess padding. The [guide](../../../design-system.md)
and its current dark/light/Chinese screenshots show the resulting layout.

Four shared vector activity buttons replace sidebar tabs. Translated hints,
selected indicator, keyboard navigation and separate saved activity remain native.
The toolbar is a child of Slide, so window/split changes keep it above the image.
Dock menu/tab arrows are disabled in default and retained arrangements. Outer
and slide insets are zero; panel12px/8px, toolbar8px/4px and field8px/4px spacing
retain readable fonts and existing action hit areas.

`pytest-final.log`: **101 passed in62.82s**. Real-GL cases cover eight activity
click/scale combinations, persistence, slide/toolbar ownership, split and resize
geometry, old arrangements, hidden panes, exact camera and RGB, project bytes,
translated selection,200% controls, saves/reopens, model loading and raw export.
Source/font contracts remain included. Installed-launcher cases outside this
GUI-only preview are excluded by the recorded selection.

The first82 checks had71 passes and11 interaction-helper failures. Hidden tabs
omit ImGui's former DockId order suffix; denser language rows changed click
coordinates. Adapt the helpers and retain their exact assertions. The corrected
32-case subset passes before the final101-case run. No product recompilation was
needed for the test-only corrections. Both failed logs/fixtures are retained.

Only GUI/font translation units were built, since the shared metric header affects
both. All38 compatible binding objects, native libraries and entry object were
reused with hashes checked. The concurrent dirty native-build/CI checkout and
its cache are untouched. This is a local UI preview, with no new wheel, release
or scientific qualification. `receipt.json` records source/binary/screenshot and
retained-object hashes. The build helpers, preview and failed outputs remain
under main build/activity-sidebar; ordinary CMake builds need no new dependency.

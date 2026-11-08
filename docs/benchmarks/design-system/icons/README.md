# Consistent native command icons

The follow-up to the COLMAP design-system port uses one vector family in
gui_icons.hpp and one IconButton recipe in gui_layout.hpp. Drawing tools and
project/image/file, refresh/reset, clipboard, class/default, metadata/tag,
model-file and record-navigation utilities share targets, stroke, colors, focus,
disabled behavior and translated hover/navigation hints. Complex scientific
commands and explicit destructive confirmations retain text.

`pytest-final.log`: **80 passed in55.74s**, with no deselections. These focused
checks exercise actual saves/reopens, model file IO, raw export/retention,
clipboard persistence, display/overview controls, language switching,16 locales,
200% layout, exact RGB and camera/project invariants. The current
[guide screenshots](../../../design-system.md) show dark/light/Chinese screens
over the retained CMU-1 research ROI. Historical receipt hashes keep their original
source scope; this packet records the updated source and screenshots.

The GUI was compiled against retained native libraries. The concurrent native
migration had replaced two binding objects with new calls unavailable in those
libraries; `build-first-link.log` preserves that mismatch. Compile only the two
unchanged original viewer/native-workflow binding sources privately and reuse
the other36 hash-unchanged objects. Font object and native libraries are reused.
No migrating cache is written, full binding/kernel campaign replayed, wheel or
published product replaced.

The first checks found a default-focus tooltip and clipped model/raw controls.
Use ImGui's native tooltip navigation policy. Model file icons now precede the
long form, feature options fold on shorter windows, and the raw panel fits its
viewport with a larger usable initial height. A baseline comparison confirmed
the previous clipped display-reset/model-load controls. The per-image display
test now opens the visible Image and display tab and samples its actual canvas
center, retaining the exact RGB assertion. A first tab-name typo and still-clipped
model Apply were corrected without weakening assertions. Failed logs/screens and
fixtures remain in main build/icon-consistency.

`receipt.json` records canonical source hashes, retained inputs, binary hashes
and the final screenshots. Build helpers and all passing/failed outputs are
retained under main build/icon-consistency. They are development evidence, not
runtime dependencies. The standalone preview is qupathxx_app.exe there; ordinary
CMake builds include the header-only icons without additional dependencies.

# Responsive native controls

The workspace and project batch form measure available pane width at the
current font size. Field labels stay beside their inputs when both fit and
move above them otherwise. Long buttons wrap within their original clickable
rectangle. Glyphs use CPU fine clipping, avoiding a new GPU scissor command
for each wrapped label. Wrapped checkbox labels remain clickable, and fields retain stable
IDs when their presentation changes during resize or UI scaling.

RGB/stain vectors stack in R, G, B order when three readable editors cannot
fit across a row. Narrow color editors show hex values while retaining the
color picker. Numeric fields omit stepper buttons when those buttons would
crowd out text entry. Long status/help text wraps at the pane edge.

Toolbar buttons flow onto another row as needed. Navigation shortcuts move
into the Help popup when the full hint does not fit. Auto UI restores the
monitor-aware 100% multiplier. Compact inspectors occupy at most 45% of the
window, leaving canvas space when the window narrows. The overview fits its
pane with its 5:3 display shape; its decoded-thumbnail and texture budgets
remain separate from the main tile budget.

Modal width and height are bounded by the viewport, with vertical scrolling
for longer contents. The batch form starts within the viewport at the current
font size. Hidden dock panes do not draw helper controls or publish ready
button replay targets.

`GuiInputTarget.SAVE_PROJECT` targets the rendered Save project button,
including its wrapped form, and supports `TARGET_READY`. Readiness requires a
visible, enabled button outside any blocking modal. Keyboard save remains
available through Ctrl+S.

`GuiReport` exposes measurements for the last painted frame:

| Field | Meaning |
| --- | --- |
| `ui_stacked_fields` | Fields/checkbox labels using a stacked or wrapped presentation |
| `ui_wrapped_buttons` | Buttons using wrapped labels |
| `ui_overflow_items` | Measured helper items extending more than 0.75 logical pixels past the right edge of their window/column work area |
| `ui_max_overflow` | Largest measured right-edge excess in logical pixels |
| `ui_overflow_label` | First measured overflowing helper label, or an empty string |

Counters start at zero for each viewer invocation, including a finite run that
remains minimized and never paints a frame.

These measurements cover the responsive helpers. They do not assert that
every data string or every pixel in the application is visible. Tabs and long
list entries can truncate, and text inputs scroll long values. Physical DPI
delivery, font coverage, vendor workflows and hardware performance acceptance
remain separate requirements.


## Validation and performance scope

The final installed wheel passes 79 focused layout, docking, scaling,
provenance, recovery and batch checks. These include real wrapped Save project
and scaled save-close clicks, a hidden-button readiness rejection, zero-frame
minimized counter isolation, camera/history preservation and exact cached
versus immediate framebuffer pixels. Windows native tests pass 34, Ubuntu
minimal passes 33, and public stubs and strict positive/seven-error negative
typing checks pass. The installed 125% workspace check measures no helper
right-edge overflow.

The [Aperio navigation sample](benchmarks/responsive-aperio-navigation-windows.json)
records 4.83/19.31 ms p95 CPU frame work for two views; the
[large synthetic sample](benchmarks/responsive-large-navigation-windows.json)
records 4.34/38.13 ms. Each 180-frame view includes three live scale changes,
four font builds, no pending tiles at exit, zero measured helper overflow and
less than 18 MiB of main tile GPU payload. Overview state/payload are separate;
the single-level Aperio overview remains unavailable under its default decode
budget.

An [alternating parent/new/new/parent comparison](benchmarks/responsive-profile-comparison-windows.json)
uses the same Python/NumPy, software renderer, reference benchmark and large
fixture. It retains complete repeat-view frame profiles, including initial
rendering spikes, as well as a steady subset after frame 130. In that subset,
median widget construction is 0.47–0.53 ms for the parent and 0.52–0.54 ms for
the new wheel. Median draw submission is 0.73 ms for the parent and
0.72–0.80 ms for the new wheel. The active host has substantial variation;
these samples establish neither a speedup nor the 16.7 ms hardware frame
budget. Driver calls can block; GPU completion latency is not measured.
Cold/dynamic rendering and physical hardware/DPI acceptance remain open.


The final full installed Windows software-GL regression suite passes 1,317
with six skips. The [local validation record](benchmarks/responsive-local-windows-linux.json)
retains JUnit/log hashes, final wheel/native identity, the installed workflow,
and matching navigation samples. The [hosted CI run](https://github.com/bipxx/qupathxx/actions/runs/39809787592)
checks the same code across the five supported CI configurations; hosted
results are recorded separately from local acceptance.


The [hosted run](https://github.com/bipxx/qupathxx/actions/runs/39809787592)
completed successfully with all five job conclusions successful. Windows
software-GL passes 1,316 Python tests/seven skips and 34 native tests. Windows
headless passes 1,033/290 and Linux headless 1,027/296 with 33 native each;
minimal Linux and ASan/UBSan also pass 33. Installed workspace checks measure
zero helper overflow at 125%. Source-exact stubs, positive/seven-error negative
typing, standalone project/slide/batch/thumbnail/layout checks and candidate
ZIP identity checks pass. The [CI artifact record](benchmarks/responsive-ci-windows-linux.json)
retains test/log hashes and verifies the standalone package against its wheel
without Python or Mesa payloads. These development candidates do not complete
physical DPI, installer/portability, macOS or the remaining M0–M7 requirements.

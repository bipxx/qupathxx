# Current implementation task

Goal: implement the project plan end to end and verify optimized performance.
The full M0 through M7 scope remains active. The previous packet implemented
and verified asynchronous whole-slide rendering; it does not complete M2.

Current sources include the tile planner, scheduler-backed GUI controller,
bounded GPU cache, scale bar, background mean measurement and CLI slide opening.
Evidence: 32 Python tests including two real OpenGL tests; four native suites
with and without OpenSlide; fresh headless wheel installation with 30 passing
tests and two expected GUI skips; real Aperio framebuffer visually inspected.
Performance scope is recorded in [tiled-viewer.md](tiled-viewer.md).

Next implement overview navigation and display controls, then actual viewport
interaction replay and frame timing on the plan's 2560 x 1440 workload. Add a
multilevel large-image fixture and exercise generation churn/cache pressure.
Keep image access, scheduler, planner, GUI framebuffer and installed tests passing.
After those checks, move to M3 command-based geometry, hierarchy and undo/redo;
do not abandon the remaining M0 decisions or M4 through M7 requirements.

Outstanding M0 decisions: geometry dependency probe, persistence schema probe,
project license choice, broader malformed-image fixtures, fixed benchmark hardware
and numerical comparison tolerances. Record decisions with actual evidence.

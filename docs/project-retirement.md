# Bounded native workspace retirement

Workspace replacement previously released every old document, image-session
selection and history on the render thread. A moved 100000-object project exposed
about a 39 ms maximum replacement frame when no external document owner remained.
The viewer now moves eligible native graphs to one retirement worker after the
new authority and render bindings are installed.

Eligibility comes from the shared-pointer control block's creation route, not
from a guess at reference counts or from project/file naming. Documents created
by native project/recovery readers and `Project.add_image` are recorded as having
native lifetime. `add_document` is conservative: any externally supplied document
keeps the whole retiring graph on the owner thread, including a C++ caller's
unknown custom deleter. One Python-owned inactive image is enough to require this
fallback. Its deleter may acquire the GIL or require some other owner-thread work.
Reading a native-created document from Python does not change the existing native
control block; external references continue to keep that document alive normally.

The retiring `Project` staging object is itself native-created. The viewer swaps
only canonical native state into it; it never gives the Python-owned live Project
object to the worker. Old active/inactive native selections move with that graph
and are destroyed before its documents. GPU textures, tile schedulers and all
OpenGL work remain on the context thread. Transient tool and publication state
is reset before subsequent frames can use the new workspace.

At most one retirement job exists. Another authority swap waits for it rather than
queuing old graphs. A loader may prepare a candidate in parallel; memory can
therefore include the live, staged and one retired graph, alongside the separately
bounded save/autosave snapshots. No global RSS bound or forced destruction of
externally retained documents is claimed. Shutdown and finite embedded runs join
the retirement worker before returning. If launching that optional worker fails,
the native captures are released on the owner thread and replacement still succeeds.

`GuiReport.retired_workspaces` counts launched native retirement jobs and
`max_retirement_ms` records the largest completed worker duration, including native
selection/model release. These measure retired ownership, not guaranteed memory
freed when other references still exist. Reports and files remain compatible;
no ownership flags are persisted in the project schema.

`tests/test_gui_retirement.py` verifies eligible loaded graphs with retained external
documents, native PPM and OpenSlide factory graphs, and mixed ownership. A timeout-
guarded subprocess adds a Python-owned inactive document to a native-loaded project:
replacement must use the owner-thread path and release that document without a GIL
deadlock. Existing workspace/lifecycle subprocess checks continue to cover Python
ownership, external document validity and frame-boundary rebinding.

The moved-project benchmark includes a complete native-loaded old graph with no
external document owner. It records native retirement separately from frame work.
Broader complex geometry/history pressure, external-control-block destruction,
vendor tile shutdown latency and native graphics driver stalls remain unfinished
performance work. Worker retirement does not make every replacement frame bounded.


The [recorded moved-project viewer run](benchmarks/recovery-association-view-1000-windows.json)
releases a native-loaded 100000-object old graph without an external document
owner. It measures 14.6 ms worker retirement, 17.0 ms CPU frame p95 and 26.7 ms
maximum frame work, compared with the [pre-retirement record](benchmarks/recovery-association-view-before-retirement-windows.json)
of 38.9 ms maximum on the same workload during implementation. This is a single-host comparison,
not a distribution or universal bound. The `add_document` fallback retains its
previous owner-thread cost. Source construction, cold/network storage and broader
vendor/complex-geometry pressure are outside this recorded run.


A separate lifetime audit found that a native server passed from a factory into
Python and then back into `Document(server)` could acquire a Python-owned proxy
control block. Snapshot cleanup then needed the GIL if the live document had been
dropped, deadlocking `wait()`. `ImageServer` now uses `enable_shared_from_this`:
the installed nanobind caster reuses its existing native shared ownership rather
than adding that proxy. The native image-server suite checks control-block identity;
two timeout-guarded subprocesses verify explicit save and autosave after removing
the live document and Python server references. This does not relax the conservative
external `Document` ownership rule for workspace retirement.

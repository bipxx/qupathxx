# Rendered-loop frame timing candidate

`GuiReport.rendered_frame_wall_p95_ms` and `max_rendered_frame_wall_ms`
measure a rendered loop from its existing frame start through swap, optional
capture and post-paint startup dispatch. The existing `cpu_frame_p95_ms` and
`max_cpu_frame_ms` retain their meanings and exclude explicit swap/capture.
These are CPU/driver loop measurements; they do not measure photon latency or
force GPU completion.

The wall and CPU samples share the same warmup policy: the first ten rendered
frames are excluded unless startup instrumentation is requested. Suspended
iterations and final worker/window shutdown are excluded. Storage is bounded
to a recent 2,049–4,096 sample window after its first capacity rollover.
`rendered_frame_wall_samples` reports the retained sample count. No timers or
Python callbacks run on analysis workers.

The viewer benchmark records both metrics at its actual framebuffer size.
Compare a defined cached-navigation workload to the proposed 33 ms rendered-loop
budget on frozen hardware. A CPU-work percentile alone cannot accept that wall
budget. This change is under validation and does not freeze hardware acceptance,
change archive formats or alter analysis results.

The first source-bound [NDPI navigation observation](benchmarks/rendered-loop-ndpi-windows.json)
uses a 2560 × 1440 framebuffer, software llvmpipe rendering, 180 frames per
view, wheel/overview navigation and live UI scale changes. Both views retain
170 wall samples, finish with zero pending tiles and UI overflow, and stay
below 64 MiB of GPU payload. Wall p95 is 167.49 ms for the first view and
192.10 ms for the repeat view; CPU-work p95 is 116.53 and 25.37 ms respectively.
This observation exceeds the proposed 33 ms budget. It is retained as a failed
performance observation on a shared, uncontrolled workstation, with performance
acceptance false. It does not establish a regression against a different-time
baseline or identify a native algorithm bottleneck. Phase profiling and frozen
hardware qualification remain necessary.

The rebuilt local wheel passes 46 native suites, installed-wheel binary/runtime
checks, generated-stub consistency, three positive typing inputs and all seven
intended negative typing cases. A separate 4,200-frame GUI run passes the
bounded recent-sample contract. The full GUI regression and hosted source-specific
Windows/Linux run remain pending; these checks do not accept performance.

The [Linux artifact audit](benchmarks/rendered-loop-linux.json) accepts run
40064597282 at exact source `551a7b4afc1b8c7c2cf9c194428b68024643ad3d`:
1,812 Python cases and 45 native suites pass headless; minimal and ASan/UBSan
also pass 45 suites each. The auditor requires all four timing contract cases
to be explicitly skipped on headless Linux. GUI execution remains a Windows
gate, and the Linux-only receipt leaves full-matrix acceptance false.

For diagnosis, `tools/benchmark_viewer.py --profile-directory <directory>`
retains separate first-view and repeat-view native phase traces. Their source
fingerprints, sample counts and SHA-256 hashes are checked and recorded in the
benchmark receipt. Phase traces include warmup and stop at 4,096 samples; their
CPU phases end before swap/capture. The report's wall percentile keeps its own
recent-sample policy described above. Software and system-driver trials must
use separate installations; retain the actual reported renderer for each.


The [completed local regression](benchmarks/rendered-loop-local-windows.json)
passes 2,205 Python cases, 46 native suites, installed/stub/typing checks and
the 4,200-frame retention check. Thirteen optional cases skip, and the same
three legacy scientific xfails remain explicit. This local run does not replace
the source-specific hosted GUI and standalone artifact gates.

The separate [system-driver observation](benchmarks/rendered-loop-profiled-hardware-windows.json)
reports NVIDIA RTX 4090, actual framebuffer 2560 × 1440, completed replay and
settled tiles. First/repeat wall p95 is 17.15/17.08 ms. The sequential
[llvmpipe observation](benchmarks/rendered-loop-profiled-software-windows.json)
reports 38.72/38.22 ms. Both reach identical final zoom/center with 25 generations,
four font rebuilds, no pending tiles or UI overflow. The original slower software
observation remains retained; shared-workstation results vary. Neither pair is
formal hardware qualification.

The [phase summary](benchmarks/rendered-loop-phase-analysis-windows.json)
checks raw trace hashes and exactly reconstructs each CPU p95 after warmup.
Native GL draw submission dominates the measured CPU/driver phase duration;
that interval can block inside the driver and is not a GPU-kernel measurement.
[Host/load context](benchmarks/rendered-loop-profile-context-windows.json) is
retained. Full traces are the `rendered-loop-phase-{hardware,software}-{first-view,repeat-view}-windows.json`
files alongside these receipts, matched by their recorded SHA-256 values.
The [frozen navigation protocol](navigation-performance-protocol.md) specifies
three independent trials for the newer qualified source and retains failures.

The source-bound [100,000-object full view](benchmarks/rendered-loop-overlays-100000-windows.json)
retains exact immediate/cached framebuffer equality, full selection and unchanged
document revision. Wall p95 is 127.30 ms immediate and 17.10 ms cached, with
12,042,180 cache bytes and 176 cache hits. The
[zoomed view](benchmarks/rendered-loop-overlays-zoomed-windows.json) culls to
3,010 visible objects and also records 17.10 ms cached wall p95. Maximum cached
frames are 43.59/47.53 ms and remain in the records; the proposed budget concerns
p95. Both are observations of source2c, not newer-source qualification.

An [initial failed overlay trial](benchmarks/rendered-loop-overlay-replay-failure-windows.json)
is preserved: Ctrl+A left zero selected objects. Independent diagnostic windows
selected all objects on both renderers, so the initial cause is unresolved.
The benchmark now establishes a canvas target before Ctrl+A, saves diagnostic
results before assertions, and retains the original full-selection/revision,
budget and exact-pixel checks. Both subsequent full/zoomed pairs pass. No native
input behavior or selection implementation is changed in this investigation.


Run40064597282 now passes the [full source-specific Windows/Linux audit](benchmarks/rendered-loop-windows-linux.json)
at source `551a7b4afc1b8c7c2cf9c194428b68024643ad3d`: Windows GUI passes
2,207 Python / 46 native, Windows headless 1,814 / 45, Linux headless 1,812 / 45,
and minimal/sanitizer each pass 45 native suites. All four ordinary/startup/
long-session timing cases pass on GUI and explicitly skip headless. The audit
also accepts inherited NDPI/LoG/parser/cache/scientific-characterization,
installed-wheel, standalone, runtime, stub and typing gates at that source.
This qualifies the timing change's engineering scope; the later Darwin
training/prediction source459 and its hardware protocol remain separate gates.

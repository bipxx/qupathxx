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

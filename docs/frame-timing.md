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

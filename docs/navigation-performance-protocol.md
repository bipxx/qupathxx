# NDPI desktop navigation qualification protocol

The first RTX 4090 observation is a baseline, not qualification. Freeze this
protocol before collecting the qualification trials. The existing proposed
33 ms p95 target is unchanged. Acceptance applies to this named workload on
this hardware; large-object, large-pyramid, reader, analysis and long-batch
budgets remain separate requirements in the project plan.

Use the exact Windows GUI wheel from native source
`459361c78f5994300f9bb5b7fb6ddd01f47eab3c`, after its source-specific installed
Windows/Linux artifact audit passes. The trial record must retain the wheel,
native and benchmark script hashes, compiled Git revision and source fingerprint.
Use a fresh installation without software OpenGL DLLs. Require the actual
reported renderer `NVIDIA GeForce RTX 4090/PCIe/SSE2`; a different renderer is
an ineligible hardware trial and its observation must still be retained.

Freeze the viewer script at Git-blob SHA-256
`acb0f034fcc818519cefc20f3bc7222c6b470b30169d76a49a07b0dc235a3bc2`.
Require the same script bytes in all qualification processes.

Hardware: Intel i7-12700K, 12 cores / 20 logical processors, 137167814656 bytes
RAM, RTX 4090 Windows driver 32.0.16.1088. Record Windows, Python, GL version,
driver, display mode and CPU load before and after each trial. The workstation
is shared; require both observed CPU-load readings at most 25 percent and run
no other owned GUI, capture, native build or performance workload concurrently.
A load reading outside that condition makes the trial ineligible, not a pass.
No OS cache flush is requested; this is warm filesystem/reader navigation,
with a newly opened reader per process and two separate windows per process.

Run three independent child processes, retaining every trial and both views:

```powershell
python -I tools/benchmark_viewer.py build/fixtures/CMU-1.ndpi --frames 180 --width 2560 --height 1440 --navigate --docked --scale-cycle --profile-directory <fresh-directory> --output <fresh-receipt>
```

Require pinned CC0 CMU-1.ndpi SHA-256
`edf4a1ccf395c7000ae93ad3b44c07d97043810e00be0c1d167dd09bbe436e46`.
For every view, require actual framebuffer 2560 × 1440, 180 rendered frames,
170 retained wall samples, completed replay, settled tiles, zero pending tiles
and UI overflow, four font rebuilds and GPU payload no greater than 64 MiB.
Require finite positive CPU/wall metrics, wall p95 at least CPU p95 and wall
p95 at most 33 ms. Retain maximum timings without discarding slow frames.
Phase trace hashes and CPU p95 must agree with the raw reported samples.

All six views must satisfy the target and all three processes must meet the
hardware/load conditions. Do not pool percentiles, select a best trial or turn
an ineligible/failed trial into acceptance. Preserve raw results and the reason
for any failure. This gate does not accept photon latency, GPU completion,
software-renderer performance, general QuPath parity or a supported release.

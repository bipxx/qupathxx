# NDPI desktop navigation qualification protocol

The first RTX 4090 observation is a baseline, not qualification. Freeze this
protocol before collecting the qualification trials. The existing proposed
33 ms p95 target is unchanged. Acceptance applies to this named workload on
this hardware; large-object, large-pyramid, reader, analysis and long-batch
budgets remain separate requirements in the project plan.

Use the exact Windows GUI wheel from native source
`75758a2bcb1f6455d9116900a7de65a433ab54dc`, after its source-specific installed
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
No OS cache flush is requested; fixture hashing may warm the filesystem.
Each process opens a new reader and uses first-view and repeat-view windows.
This does not establish cold-storage latency or a fully warm decoder cache.

Run three independent child processes, retaining every trial and both views:

```powershell
python -I tools/benchmark_viewer.py build/fixtures/CMU-1.ndpi --frames 180 --width 2560 --height 1440 --navigate --docked --scale-cycle --profile-directory <fresh-directory> --output <fresh-receipt>
```

Require pinned CC0 CMU-1.ndpi SHA-256
`edf4a1ccf395c7000ae93ad3b44c07d97043810e00be0c1d167dd09bbe436e46`.
For every view, require actual framebuffer 2560 × 1440, 180 rendered frames,
170 retained wall samples, completed replay, settled tiles, zero pending tiles
and UI overflow, four font rebuilds and GPU payload no greater than 64 MiB.
Require 25 generations, final zoom 5.681747443875566 (absolute tolerance 1e-9)
and final level-zero center (25579.198625411034, 19048.82822789989) within .01
pixels per axis. These baseline pose checks detect replay that was dispatched
without applying the expected navigation.
Require finite positive CPU/wall metrics, wall p95 at least CPU p95 and wall
p95 at most 33 ms. Retain maximum timings without discarding slow frames.
Phase trace hashes and CPU p95 must agree with the raw reported samples.

All six views must satisfy the target and all three processes must meet the
hardware/load conditions. Do not pool percentiles, select a best trial or turn
an ineligible/failed trial into acceptance. Preserve raw results and the reason
for any failure. This gate does not accept photon latency, GPU completion,
software-renderer performance, general QuPath parity or a supported release.

Run the qualification driver from a clean installation of that audited wheel:

```powershell
python -I tools/qualify_navigation.py --engine-audit <full-engine-audit.json> --image build/fixtures/CMU-1.ndpi --output-directory <fresh-directory>
```

The driver rejects earlier/partial engine audits before opening a window. It
checks the installed binary against the audited GUI artifact, retains all raw
trials, phase traces, logs, load readings and failures, and reports only the
named navigation target's acceptance. Use the existing source auditor with
`--ndpi-readers --rendered-loop --gui-stack` to prepare the required full engine audit.

The earlier source459 was not qualified: its GUI matrix failed a standalone
project-startup case with a native stack-overflow status. Source794 moves large
session UI storage off the caller's stack and adds repeated startup on the
unchanged 1 MiB native executable stack. The replacement source must pass the
full matrix before any qualification trial. Workload, hardware, tolerances,
33 ms target and frozen viewer-script bytes remain unchanged.
The combined source757 also separates the four remaining mixed-type Darwin
declarations found by complete native compilation; it includes the source794
GUI stack change and its repeated-launch regression.

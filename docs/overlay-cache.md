# Canvas image cache

An unchanged canvas reuses its previously rendered base image, including native
image tiles, tissue/cell previews, body/nucleus outlines and annotation labels.
`GuiOptions.overlay_cache_budget` bounds retained RGBA GPU payload, defaulting
to 64 MiB; zero uses immediate rendering and values above 256 MiB are rejected.
Object geometry, picking, editing, selection and label visibility are unchanged.

After the base canvas commands execute, an ImGui renderer callback copies that
opaque framebuffer rectangle into a nearest-filtered texture. A matching later
frame draws those same pixels as a quad. This avoids repeated visibility lookup,
tessellation, triangle upload and dense text/outline overdraw. Vertex handles,
draft shapes, marquee, status and scale bars draw afterward and remain immediate.
The cache captures neither dialogs nor transient tools. Capturing after blending
preserves the framebuffer's original rounding; no transparent-layer approximation
or second alpha blend is introduced.

The key includes document/selection identity and revisions, primary object,
camera, canvas/clip bounds, font/atlas, drawing and tessellation settings,
image-tile uploads/generation/payload, tissue/cell job generations and result
identities, preview readiness/visibility and cell preview layer. Display changes
and workspace/image replacement clear the cache explicitly. Tile scheduling and
preview uploads continue on hits, and visible image tiles retain their LRU use.
Physical pixel bounds also invalidate on framebuffer/DPI changes.

The texture covers the clipped canvas, aligned outward to physical pixels and
clamped to the framebuffer. Its width/height must fit `GL_MAX_TEXTURE_SIZE` and
`width*height*4` must fit the budget before allocation. Replacement deletes the
old texture first. Bounds or allocation/copy failures use immediate rendering;
a failed resource is retried after a key change. No CPU framebuffer readback,
extra background worker or document/Python ownership is involved.

`GuiReport` exposes current/peak `overlay_cache_bytes`, successful
`overlay_cache_builds`, `overlay_cache_hits` and `overlay_cache_fallbacks`.
The bytes describe retained RGBA GPU payload; driver allocation overhead, normal
framebuffer buffers, image/mask caches and per-frame ImGui buffers are separate.
This is not a process RSS guarantee. Selection revisions advance on effective
membership/primary changes and deletion pruning; no-op selections preserve them.

The optimization covers unchanged base canvases. Invalidated views still render
all visible objects. Navigation through a dense scene, edits, first-frame work,
large dynamic masks, OS/DPI delivery and other hardware still need broader
validation. A frame-time recording does not measure GPU completion latency.

`tools/benchmark_overlays.py` runs paired immediate/cached views and requires
identical complete framebuffer bytes before recording timings. Native checks
cover physical bounds, fractional DPI and exact budget endpoints. Desktop checks
compare full captures through selection, drawing, camera/history changes,
image switching and forced budget fallback. Existing image, stain, tissue,
cell, project/recovery and interaction tests run with the cache enabled.

The [2560-by-1440 recording](benchmarks/overlays-100000-2560-windows.json)
keeps all 100000 objects visible and selected. Sampled CPU frame p95 changes from
99.72 to 16.91 ms, with equal full-frame hashes. The cache retains 12194160 GPU
payload bytes, records four captures and 176 hits in 180 frames, and stays within
the default 64 MiB budget. The [1440-by-900 recording](benchmarks/overlays-100000-1440-windows.json)
records 97.07/16.94 ms p95 with 3675040 bytes. These are paired warm synthetic
observations on Windows 11 and an NVIDIA RTX 4090, not broad latency guarantees.

Reproduce using the installed wheel's Python:

```powershell
python tools/benchmark_overlays.py --output build/overlays.json
python tools/benchmark_overlays.py --width 1440 --height 900 --output build/overlays-small.json
```

Both runs verify complete framebuffer equality before writing results. The
recording includes the native binary hash. `--zoomed` selects the existing narrow
view fixture; `--budget-mib` explores explicit resource limits. GUI replay tests
force a one-byte budget to prove the immediate fallback still draws all objects.

The final installed-wheel regression passes 802 Python cases with two unavailable
Windows symlink cases skipped. Fifteen new cases include selection revisions,
canvas equality, native preview completion and budget fallback. All 24 native
suites pass with GUI enabled, and all 23 core/minimal suites pass without it.

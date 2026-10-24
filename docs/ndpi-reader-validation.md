# Hamamatsu NDPI validation candidate

The [official CMU-1.ndpi fixture](https://openslide.cs.cmu.edu/download/openslide-testdata/Hamamatsu/index.yaml)
is CC0 and pinned by size and SHA-256 in `tests/fixtures/manifest.json`.
The 198,030,965-byte scan has nine natural levels, from 51,200 × 38,144 to
200 × 149, with anisotropic spacing about 0.456413 × 0.455063 µm.
`tools/fetch_reader_fixtures.py --extended` downloads and verifies it alongside
the full Aperio fixture. Set `QUPATHXX_HAMAMATSU_FIXTURE` to its local path.
The standard extended CI mode sets this automatically.

The 24-case corpus checks metadata and physical area, exact straight-alpha
pixels at every level, padding, retained buffer lifetime, nine native region
CLI invocations, complete overview averaging and source admission, scheduler
cache reuse and pending close, truncated input, tile-invariant LoG labels and
features, and saved/replayed cell records. The Python reference binding uses
the same pinned OpenSlide library through an independent adapter. These checks
validate adapter/workflow behavior rather than an independent codec or
biological segmentation ground truth.

This exposed a project API gap: `add_document()` with an omitted source tried
to embed a native slide as dense RGB. The C++ fix preserves an opened native
OpenSlide/TIFF source's locator and reader, including the current TIFF view.
Explicit conflicting readers reject before changing the project. Dense
in-memory images retain their existing embedding behavior. No storage schema
or archive format changes.

The [local receipt](benchmarks/ndpi-native-link-windows.json) binds source
`f5ca246`, 248 build inputs, the installed wheel, 45 native suites, 134 focused
Python passes, matching stubs/typing and the 1,812-pass full Windows headless
regression (405 optional skips and three unchanged legacy scientific xfails).
All 23 headless NDPI cases pass; its desktop case is explicitly skipped.
The native project suite separately executes NDPI LoG save/replay, and native
raw TIFF tests exercise default link capture with historical raw records.

The [desktop capture](images/ndpi-overview-baseline-windows.png) shows the whole
nine-level slide and overview with its distinct X/Y spacing. That renderer test
passes at the previously qualified source `8160ec2`; it does not qualify the
newer project-link fix on a GUI build. Rebuilt source-specific hosted desktop,
Windows/Linux, portable and macOS qualification remain pending. This one NDPI
brightfield scan does not cover VMS/VMU, multiple focal planes, fluorescence,
ICC/color accuracy, broader damage, biological accuracy or hardware budgets.


## Repeated region observations

The shared `tools/benchmark_vendor_reader.py` now handles both pinned full
Aperio and NDPI fixtures. Its Aperio coordinates and acceptance fields remain
compatible with prior engineering audits. Input SHA/size verification precedes
timing and can warm the filesystem cache. Each natural level has one first
handle read and nine repeated reads; all raw warm durations and pixel digests
are retained. A separate scheduler observation verifies one backend region read
and nine cache hits within the configured eight-MiB cache budget. It measures
backend-region calls rather than JPEG decompression counts.

[NDPI observations](benchmarks/vendor-reader-ndpi-windows.json) and the
[Aperio comparison workload](benchmarks/vendor-reader-aperio-windows.json)
record the compiled source, native binary, executed tool and nearby hardware
snapshot. They are distinct inputs, so their timings do not estimate a vendor
speedup. No OS cache flush, cold-storage, RSS, frame or hardware acceptance is
claimed. The standard extended CI driver now collects an NDPI observation on
future dispatches; the already dispatched source1b checks predate this tool
update. Its actual reader corpus remains independently gated.

# Extended vendor reader validation

The optional full Aperio fixture extends the small single-level smoke fixture.
It is `CMU-1.svs` from the [OpenSlide test-data publisher](https://openslide.cs.cmu.edu/download/openslide-testdata/Aperio/index.yaml),
listed there as CC0. The committed manifest pins its 177,552,579 bytes and SHA-256
`00a3d54482cd707abf254fe69dccc8d06b8ff757a1663f1290c23418c480eb30`.
The binary stays in ignored `build/fixtures` and is not distributed in the wheel.

Its three levels are 46,000 × 32,914, 11,500 × 8,228 and 2,875 × 2,057, with
0.499 µm level-zero calibration. The reported downsample factors are fractional,
rather than exact powers of four. This exercises coordinate and alpha edges
that the small fixture cannot cover.

Run with an installed OpenSlide-enabled wheel and the CI test dependencies:

```powershell
python -I tools/fetch_reader_fixtures.py --extended
$env:QUPATHXX_APERIO_MULTILEVEL_FIXTURE=(Resolve-Path build/fixtures/CMU-1.svs).Path
python -I -m pytest tests/test_openslide_extended.py -q
python -I tools/benchmark_vendor_reader.py --slide build/fixtures/CMU-1.svs --output build/vendor-reader.json
```

On Linux, export the same environment variable with an absolute path. The current
Linux development wheel also requires the matching GEOS SDK's `lib` directory
on `LD_LIBRARY_PATH`; see [distribution limitations](ci-distribution.md).
The CI
workflow's manual `extended_readers` input enables these checks and the benchmark
for the Windows/Linux installed headless jobs and Windows desktop job. Ordinary
pushes retain the small smoke download. An explicitly enabled suite fails if its
reader or reference dependency is missing.

Eleven local Windows cases pass against the installed native source fingerprint
`223d8b2a94105dc038fa2a1e09f5894d846509a81801990318fa4cf3ec8bb3f4`:

- Metadata, calibration and a calibrated ROI mean; oversized whole-level reads
  reject before a small valid read succeeds.
- Exact regions at all three levels, off-image padding, retained read-only buffers
  after close, and installed native CLI output.
- Scheduler cache reuse without another read, generation rejection while closing,
  bounded reservations, shutdown and a fresh handle's matching pixels.
- A complete alpha-weighted overview compared with the lowest-level global mean.
- Truncated container rejection and latched JPEG damage errors, while a separate
  healthy handle continues working. Damage is generated in a temporary copy.

The Windows software-OpenGL run passes these eleven checks plus a desktop case
(twelve total). Fit-to-window and zoomed viewport reads settle, stay within the
64 MiB GPU cache limit, and continue while the separately bounded overview reports
its unavailable state. This is a replay check, not a hardware frame-time gate.

The [local cross-platform receipt](benchmarks/extended-reader-local-windows-linux.json)
also records eleven passing Linux cases with the desktop-only case skipped.
The fresh hosted Linux wheel was installed in a separate WSL Python 3.12
environment with a locally built checksum-pinned GEOS 3.15.0 SDK. All three
benchmark region hashes and the complete overview hash match Windows exactly.
The [Linux timings](benchmarks/vendor-reader-linux.json) describe that local
environment, rather than the hosted runner's performance.

The independent OpenSlide Python binding uses the same decoder library. These
checks establish adapter consistency, not an independent decoder comparison.
The oracle reads OpenSlide's premultiplied C buffer directly and applies the
documented nearest-integer straight-alpha conversion. OpenSlide Python's ordinary
image conversion truncates partial-alpha RGB; the suite separately proves that
the difference is at most one and absent at opaque/transparent pixels.

The [local benchmark receipt](benchmarks/vendor-reader-windows.json) records exact
region/output hashes, native binary identity and repeated timings. For 257 × 193
level-one requests, nine scheduler cache hits required only one native read in
total; median cached completion was about 0.025 ms. The cache held 198,404 bytes
against an 8 MiB limit. A complete 250 × 150 overview took about 248 ms on this
machine. First reads may already benefit from OS/backend caches; these numbers
do not establish RSS, hardware frame budgets or long-run performance.

The lowest source level has 5,913,875 pixels, exceeding the default 4,194,304-pixel
overview budget. Native/Python thumbnail requests work with an explicit 8 Mi-pixel
budget; the current desktop overview retains its default limit and reports
unavailability. Configurable desktop overview budgets remain open, alongside
other vendors, ICC, physical DPI delivery and sustained navigation stress.
Extended hosted acceptance has not yet been recorded.

# Small-radius expansion frontier

This candidate preserves the strict-radius watershed recipe while replacing
its priority heap for radii greater than one and at most eight sampled pixels.
It uses 64 FIFO lists indexed by exact squared distance. A 64-bit occupancy mask
selects the smallest nonempty list, including when a newly admitted pixel has
a lower priority than the preceding pop. FIFO order within a list preserves the
original heap's distance/arrival ordering and collision lines.

For integer pixel-center coordinates, squared distances below 64 are integers.
Their float square roots are strictly increasing, and rounding the square of
each float root recovers the original integer. The native test checks every
key. Larger radii retain the heap. Radii up to and including one return the
validated input labels directly: the strict cutoff cannot reach a distinct
pixel center. Seed-domain checks and the existing memory admission remain in
effect before that return.

The small frontier uses one native link index per raster sample instead of
reserving a full distance/index/sequence heap entry per sample. This reduces
the frontier's allocated payload; process peak working set also depends on the
distance-transform buffers and on which heap pages are touched. No RSS reduction
is inferred from the allocation change alone. All memory remains native and
owned by the worker; no Python callbacks or deleters are introduced.

QCL4 restoration also reuses its validated historical domain when collecting
body statistics. Every opaque domain sample is checked against the ROI, and
every nonzero label must belong to that domain before statistics are collected.
The grid-bound check remains; older archive versions retain their predicate.
A signed malformed empty archive that moves an opaque bit outside the ROI while
preserving the sample count is rejected by the domain check.

The initial native checks compare all labels, queued-pixel counts and peak queue
sizes against the heap in 1,344 cases, covering narrow grids, seeded domain
holes, fractional cutoffs, float-distance boundaries and heap fallback. All
seven eligible pinned QuPath expansion cases also remain exact. These checks
do not accept broader cell detection or biology.

`tools/profile_cell_expansion.py --executable NATIVE_TEST_EXE
--output-directory NEW_DIRECTORY --report REPORT.json` alternates forced heap
and automatic buckets in fresh processes. All output pixels must match an
independent strict-radius dilation oracle. It retains every observation and
excludes one recorded warmup per mode/size from summaries. Kernel timing
excludes file I/O; process timing includes startup and protocol reads/writes.

An initial 36-observation Windows series (six warmups) at source `6feb81e`
records 1,024-pixel kernel medians of 26.49 ms for the heap and 24.82 ms for
buckets. Process medians are 146.20 and 151.52 ms respectively. The differing
directions demonstrate why process timing cannot stand in for kernel timing.
The host is uncontrolled; all raw observations remain under
`build/queue140-timed-frontier-profile`. Whole-workflow and hardware acceptance
remain open.

The full workflow profiler accepts `--baseline-python OLD_ENVIRONMENT` alongside
`--python CANDIDATE_ENVIRONMENT` to interleave both builds and both production
recipes. Each build must retain a stable native identity, the builds must have
distinct native hashes, and all fixture labels must match across every build,
recipe and trial. This comparison is required before drawing a conclusion
about production detection, archived-run loading or replay performance.

The [paired workflow series](benchmarks/expansion-workflow-paired-windows.json)
retains 60 observations with 12 warmups. At 1,024 pixels, sampled detection
medians are 272.53 ms for the preserved baseline and 263.41 ms for the candidate;
reopen validation is 178.61 versus 167.63 ms. Some measurements regress: legacy
replay medians are 258.05 versus 266.14 ms. All labels and saved replay checks
pass. This uncontrolled series suggests a modest default-radius benefit but
does not qualify hardware or establish a general performance guarantee.

The [one-pixel series](benchmarks/expansion-radius-one-windows.json) compares
the preserved pre-fast-path native executable with the candidate: 1,024-pixel
kernel medians are 21.72 versus 1.26 ms, and maximum child working sets are
25.23 versus 13.33 MiB. Every output equals the original seed map. The
[zero-radius control](benchmarks/expansion-radius-zero-control-windows.json)
retains its already-fast behavior without a measured improvement. Each series
retains 36 observations/six warmups. Eight installed Python cases independently
check labels, measurements and archived replay at zero, fractional, unit and
next-representable-above-unit radii for both body recipes.

The earlier [process-only pilot](benchmarks/expansion-frontier-initial-process-windows.json)
and [separate kernel timing series](benchmarks/expansion-frontier-kernel-windows.json)
remain available alongside the [pinned expansion comparison](benchmarks/expansion-frontier-qupath-windows.json).
No raw series or slower trial is discarded. Broader detection, platform,
full desktop regression and controlled hardware gates remain separate.

## Platform audit requirements

The Windows/Linux, portable-wheel and Mac native auditors accept
`--small-frontier`, which implies their sampled-production gates. They require
the native 64-key/1,344-comparison marker. Each Python builder, consumer or
desktop job must additionally pass the eight exact source-bound unit-radius
workflow names, without skips or expected failures. Mac native remains a
native-only gate. The contract is
`tests/fixtures/imagej-shape-1.54p/expansion-frontier-workflows.json`.

The shared verifier accepts the actual local native/Python evidence and rejects
missing, duplicate, skipped, failed and older native cases. The full
Windows/Linux and Mac command-line gates reject the otherwise-valid earlier
sampled candidate when the frontier flag is requested. These are engineering
checks; the flag does not assert a timing improvement or hardware acceptance.

Before publishing this candidate, additional replay probes exposed two gaps in
the earlier sampled recipe. A checksum-valid opacity-domain edit could preserve
the sample count and empty labels while passing pixel replay, because result
comparison omitted the packed domain. An ellipse could also cover a boundary
point outside the canonical sample grid, which the domain decoder did not
exclude. The initial module fails both new regressions while its unchanged
control passes. Verification now compares packed domains, and decoding checks
the canonical grid as well as geometric coverage. Existing archive versions
keep their earlier rules. The gate additionally requires all three original/
changed-domain Python cases and the native ellipse-grid regression.

The initial 2,370-pass full regression and performance profiles remain bound to
source `ee54cdb`, before these fixes; they are retained as initial evidence.
The domain-fix build passes 47 native suites and 156 focused Python cases.
Final clean-source validation and performance measurements must be rerun before
accepting or publishing the updated candidate.

That [post-fix clean validation](benchmarks/expansion-frontier-validation-windows.json)
now passes 2,373 full Python cases, 47 native suites and 156 focused cases;
116 additional comparison checks pass with three desktop skips and the three
retained scientific xfails. The final full suite retains 13 optional skips and
the same three xfails. All 36 real-slide compartment maps remain unchanged.
The post-fix timing series below are retained separately from the initial data.

The [domain-fixed unpinned series](benchmarks/expansion-domain-fixed-workflow-windows.json)
retains another 60 observations/12 warmups. At 1,024 pixels, sampled detection
medians are 252.67 ms baseline and 253.20 ms candidate, while legacy detection
is 259.46 versus 280.69 ms. Sampled reopen validation is 174.60 versus 171.26 ms.
All output/replay checks pass, but the mixed timings do not support a general
speedup. These results remain alongside the earlier series.

For a placement-controlled comparison, the workflow profiler accepts
`--cpu-affinity-mask 0x1` on Windows. Each worker applies and reads back its own
mask before importing NumPy or the native module; the parent process and other
applications retain their settings. Requested, original, system and verified
masks are recorded in every observation. Unsupported/empty masks fail the run.
The implementation follows Microsoft's
[SetProcessAffinityMask](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setprocessaffinitymask)
and [GetProcessAffinityMask](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getprocessaffinitymask)
contracts. This controls worker placement, not ambient load, and does not
replace the separately frozen GUI/hardware qualification protocol. The first
paired pinned run uses the predeclared mask `0x1`, four measured repetitions
per build/recipe/size and one retained/excluded warmup per combination.

The [fixed-affinity series](benchmarks/expansion-affinity-workflow-windows.json)
retains all 60 observations/12 warmups, with every worker mask verified. At
1,024 pixels, legacy detection medians are 296.69 ms baseline and 270.67 ms
candidate; sampled detection is 295.95 versus 272.19 ms. Sampled reopen is
191.89 versus 179.12 ms. Legacy replay is slower, 294.49 versus 305.00 ms,
and its process-wall median is also slightly slower. The direction differs
from some unpinned observations, so these diagnostic results do not establish
a general speedup or controlled hardware acceptance. All labels and saved
replay checks pass across builds and recipes.

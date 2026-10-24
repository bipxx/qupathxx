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

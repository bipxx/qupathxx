# Spatial native capture264

Native2b9c5d7 adds bounded owner capture, immutable deterministic distance results
and cancellable owner jobs over the existing index. No source-by-target matrix,
live worker Document, reader ownership or pixel IO is introduced. Snapshot/work
limits cover conservative payload accounting; they are not measured OS RSS.
Actual distance-comparison quota exhaustion fails without publishing partial rows.
Document identity/revision guards currentness; retained external result values
remain historical. Geometry-only requests may run after image-source closure.

Four affected suites pass each Windows GEOS-ON/OFF profile. Authentic pinned
QuPath reference files are byte-authenticated. A documented translation by(32,32)
fits fixtures into the native nonnegative document while preserving distances.
18 geometry cases ON/16 OFF and four explicit float-storage conversions pass
fixed tolerances. Four nonzero-plane cases are unsupported; two polygon cases
are unavailable without GEOS. The native result retains double precision.

Initial66c8cad compilation failed an explicit optional-to-bool test expression;
corrected2b9c5d7 passes. Initial tooling900/2 arose from a temporary Git fixture
missing production -text attributes. Corrected7bb8d78 passes902 controls; all395
compiler inputs match2b9c5d7 exactly, so the products were not rebuilt or relabelled.
Both initial failures and corrected outputs remain preserved. SDK trees match263.

This is native local groundwork, not the complete SPAT1 user workflow. Versioned
retention/archive/export, nanobind/CLI/ImGui/managed delivery and complete exact-source
qualification remain. Candidate262 engineering acceptance cannot transfer here.
No biological, comparative speed, representative process-memory, hardware or
supported-release acceptance follows from native test timings.

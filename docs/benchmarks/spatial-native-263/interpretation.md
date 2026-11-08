# Native SPAT1 geometry and query foundation

Source 3af9572 adds native ROI centroids and nearest-point queries to the existing
immutable geometry and spatial index. This is the foundation of the next spatial
workflow, not delivered capture/jobs/persistence/Python/CLI/ImGui functionality.
Candidate262 remains frozen at e2282b8 and qualifies independently.

Rectangles and ellipses use analytic centres. Multipoints use their point mean;
polylines use segment-length weights and skip zero-length segments. Offset-based
means keep intermediate moments within the finite geometry extent. Valid polygon
and multipart centroids reuse the existing serialized GEOS C context and its area
centroid operation, preserving holes and part weights. Polygon construction keeps
its existing GEOS requirement; other centroid kinds pass without GEOS. Shared
immutable polygon snapshots are queried from eight native workers in the tests.

The existing median-split SpatialIndex now supports point-only nearest queries
with finite positive per-axis scales and optional identity exclusion. Empty
eligible sets return no neighbour; coincident distinct IDs remain eligible, and
equal computed distances choose the smallest ID. Nodes retain their actual
coordinate maxima for conservative bounds rather than reconstructing origin plus
extent. L-infinity lower bounds prune Euclidean search without squaring large
values. A fixed address-width stack holds the traversal; no dense source-target
matrix or second index implementation is introduced. Nonpoint nearest queries,
invalid scales/coordinates, cancellation and unrepresentable final distances reject
explicitly. Region-candidate and picking APIs keep their previous meanings.

Independent analytic centroid checks include asymmetric triangles, holes,
multipart areas, concavity, unequal line segments, repeated points/segments and
near-limit coordinates. Nearest IDs/distances match 400 reordered and 200 concurrent
exhaustive squared-distance oracles, plus 100000-point scenes and explicit numeric/
tie/exclusion controls. Three affected native suites pass in each Windows GEOS-ON
and GEOS-OFF profile, with explicit capability markers. These repeat the same
requirements in two configurations; they are not six distinct workflow suites.
No all 26 upstream/native comparison or speed/hardware claim is made here.

Both compiler manifests bind 392 inputs (six changed), and both generated build
headers match the same source/fingerprint. All 490 raw source paths authenticate;
SDK trees are unchanged. Prior 55 candidate262 products and both new core/test
sets survive separately. The original build-command target spelling error is
retained; correcting the existing geometry_edits target on unchanged source passes.
Only the affected native targets were built; the earlier installed Python runtime
and qualified/frozen sources are not relabeled as this feature.

Next: capture selected source/target identities, centroids, calibration and budgets
on the owner; produce an immutable result with nullable distances/currentness;
then add bounded jobs, versioned retention/export and thin Python/CLI/UI delivery.
The committed 26-case independent QuPath reference and fixed geometry tolerances
remain the later integration oracle. Neighbourhoods, signed geometry distances,
density maps and tiled aggregation remain separate required spatial work.

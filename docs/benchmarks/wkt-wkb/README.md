# Native WKT/WKB annotation interoperability

Selected local Windows x64 provider `org.qupathxx.wkt-wkb`1.0.0, annotation ABI3,
contract `image-pixel-wkt-wkb/1`, transfers declared image-pixel XY geometry into
the existing native project workflow. Selecting this provider declares those
units; it does not infer geographic CRS, convert physical/geographic coordinates,
discard dimensions or assign a plane from coordinates. See the
[complete profile](../../../plugins/wkt_wkb/README.md).

Independent C++ parsing supports the seven basic2D types in WKT, raw WKB and
hex WKB. Both/mixed member byte orders, explicit polygon closure/holes/parts,
singleton multi-geometries, nested membership and literal binary64 coordinates
are retained. Multi-lines/heterogeneous collections become ordered native
objects with exact member-path IDs and enclosing-container metadata. Empty,
Z/M/SRID/extension, malformed/oversized and unrepresentable topology profiles
refuse; no partial row loss, clamp, automatic closure or geometry repair occurs.

Source rows/facts, QRO value encoding, fixed-arena ownership and ABI3 producer
callbacks now live in shared native plugin support used by WKT/WKB and ImageJ.
All scientific/application geometry, image frame/plane/class rules, identity,
preview/apply, undo and persistence remain the existing native authorities.
The native host/archive/CLI/SDK consumer were unchanged and retain exact prior
source/binary identities. No new kernel, binding, GUI, wheel or Java/Python
runtime is part of this delivery.

The pinned official OGC06-103r4/v1.2.1 schema and implementation terms are in the
[reference lock](../../../plugins/wkt_wkb/reference-lock.json). The specification
is referenced rather than redistributed; the independently authored parser and
shared helpers are AGPL-3.0-only, with no external reader/codec dependency. No
standards certification or patent grant is asserted. ImageJ retains its actual
NIH schema reference, AGPL code and private unchanged `Zlib` codec terms. Each
provider's exact source/binary identity and actual component obligations stay
separate. Existing admitted GEOS readers are a development syntax/type oracle;
they are not linked by the WKT/WKB module or a product fallback.

Evidence is under build/gui-aggregation/catalogue-closure/wkt-wkb/:

- `fixtures-02-μ-Δ/fixtures.json`:48 authored positive and79 refused profiles,
  independent expected QRO/IDs/metadata and exact input hashes. Existing admitted
  GEOS3.15.0/CAPI1.21.0 readers independently accept/type-check all48 positives.
- `validation-02/validation.json`: native preview/apply, exact source-ID maps,
  undo/redo, save/reopen/offline, binary64 signed zero/subnormal, Unicode filename,
  hierarchy metadata, source staleness, budgets and explicit Z/T choices on real
  Z2/T2 TIFF axes. Fourteen process checks include CLI/native exact preview,
  receipt and persisted-object comparisons and atomic refused outputs.
- The same native test exercises direct ABI3 minimum-budget open, bounded
  probe/fill/end-page ownership, long-path opening-budget refusal before I/O,
  actual parse-time cancellation and stable process file-handle counts across
  cancelled private opens.
- `imagej-regression-02/validation.json`: all26 positive/37 refused ImageJ cases
  and12 native/CLI checks pass through the shared producer. Its new module is a
  distinct source/binary candidate. The original ImageJ package/module/source
  receipt stays frozen; its identity is not transferred to changed source.

The earlier28 ABI3 and57 ABI1/2/native/CLI controls retain their unchanged host/
archive/source scope. Source-input equality, rather than another host rebuild,
establishes reuse. The earlier ImageJ receipt in the repository records
those frozen products; A03's source/package records own the shared producer's
new provider identities. All354 prior protected products and source-package
members remain exact. The original18 dirty snapshots and earlier QOI work are
preserved.

The194-member combined source/module/kit package retains exact previously
admitted native overlay/CLI/SDK/JSON/SQLite/zlib/NIH members and their original
material IDs/terms. It adds separate source/binary records for the new WKT and
shared-producer ImageJ candidates. `SOURCE.json` records both new providers and
unchanged host identities; `PACKAGE.json` records exact owned paths/digests.

`offline-runtime.json` records independent source builds of both standalone
modules, ten selected WKT positive/six refused profiles and fourteen native/CLI
checks, eight selected ImageJ positive/six refused profiles and twelve checks,
and the coexisting two-provider catalog. Both routes through the unchanged
public-only SDK consumer match exact previews/receipts/persisted objects. Root
configuration verifies the new provider/test wiring with Python OFF and no
Python/nanobind discovery; the base core/native host was not rebuilt.

`package-final-admission.json`, `package-zip-admission.json`, `removal-proof.json`
and `results.json` record the final package/runtime/owned-only removal outcome.
The deployment check preserves user images, annotation inputs, projects, reports
and notes, and reopens saved results after removing only verified package files.
Initial failed/successful runs remain under their actual source and runtime
identity; no scientific assertion was relaxed to repair a controller.

This is a selected native engineering profile, not full
geographic/QuPath equivalence, biological/speed/platform acceptance or closure of
the full qualified catalogue/application goal.

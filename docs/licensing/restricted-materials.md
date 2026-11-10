# Restricted-material references

Owner instruction: keep proprietary and noncommercial material separate from
the open feature implementation and from each other. This is a **metadata-only**
register, not a download list or approval to integrate restricted material.
The [expansion plan](../feature-expansion-plan.md#material-separation) owns handling;
the [reader policy](../reader-license-policy.md) still excludes closed reader/codecs.

No proprietary or NC artifact was downloaded for this planning task. Historical
archives are preserved; L01 will classify any previously unreviewed assets before
the next release. Do not infer that every historical file has already been audited.

## Proprietary material

| Record | Material | Status / handling |
| --- | --- | --- |
| PROP-CLASS-01 | Closed decoder/SDK binaries, NDA-only schemas and permission-restricted source | Excluded reference category; no specific package is admitted or downloaded by this plan |
| PROP-CLASS-02 | Weights/assets with explicit proprietary permission or redistribution restrictions | Record the actual version/terms; do not infer rights from an open model container or wrapper. Missing/unknown terms belong in the unreviewed register instead |

If later explicitly authorized for a separate local study and permitted by its
terms, keep actual artifacts outside this repository in a proprietary-only pool
such as `../qupathxx-restricted/proprietary/`. No source/build/package/CI search
path, automatic runtime download, open fixture or derivative-model pipeline points
there. This register does not select a proprietary adapter or relax open-format rules.

## Noncommercial material

License evidence checked2026-11-08; exact artifacts would still need their own
version/hash/terms record before any separately authorized use.

| Record | Material and primary evidence | Restriction | Open-product decision |
| --- | --- | --- | --- |
| NC-001 | [BBBC017v1 images/ground truth](https://bbbc.broadinstitute.org/BBBC017), Copyright section | CC BY-NC-SA3.0 | Exclude from bundled demos, training, CI and open-release acceptance fixtures; metadata reference only |
| NC-002 | [BBBC020v1 images/ground truth](https://bbbc.broadinstitute.org/BBBC020), Copyright section | CC BY-NC-SA3.0 | Same exclusion; do not substitute its converted/cropped images into the open fixture pool |

Keep any later authorized NC artifacts separately at a path such as
`../qupathxx-restricted/noncommercial/`; never combine that pool with proprietary
artifacts or an open default dependency. Research-only conditions are also
restricted even if download is free. Conversions, screenshots, crops, labels and
trained weights retain their source/terms lineage. No restricted pixels, masks,
weights or license text copies are needed in this register.

## Unreviewed material

Unknown code, datasets, filters or model weights remain unreviewed, not open or
proprietary by guess. Record URL, owner, resource kind, exact revision/digest,
license evidence, derivative sources, intended use and decision. A later authorized
local copy belongs in `../qupathxx-restricted/unreviewed/` until resolved; no
automated build/download should populate it during ordinary development.

Private user input is not automatically a project fixture or redistributable asset.
Keep it local under the user's permissions and out of public logs/packages.
The actual OME tubhiswt and 10x mouse-brain demonstration files have CC BY4.0
evidence in [the open demo receipt](../benchmarks/real-data-demo/README.md); they
are not proprietary/NC merely because an institution or commercial vendor owns them.

## Offline staging admission

Implementation handling for each future packet:

- Before introducing a resource, classify its code, format, codecs, data, weights
  and derivatives independently in the existing inventories. Record exact source,
  digest, terms evidence and parent IDs; unresolved entries stay unreviewed.
- Keep proprietary, NC and unreviewed references in their distinct sections above.
  Do not put their artifacts in the open fixture tree, build cache, dependency
  downloads, screenshots, training runs or release acceptance inputs. Separate
  external pools do not authorize downloading or integrating their contents.
- Use synthetic metadata/bytes to test rejection, including restricted ancestors
  and attempted pool inclusion. No restricted dataset is required for these tests.
- At L01/P02, run the existing guard on frozen source-kit and binary staging,
  including notices and generated assets. A failed admission stays a visible
  release blocker; it cannot be bypassed by conversion or a different wrapper.

This updates handling requirements only; it does not assert a completed audit of
historical assets or admit any new dependency, dataset, codec or model.

[check_material_staging.py](../../tools/ci/check_material_staging.py) reads
`material` annotations in the existing fixture/font/dependency inventories; it
does not download data or classify unknown licenses automatically. Each record
declares its ID, category, resource kind, exact hash, source reference, license
evidence, attribution and source/derivative IDs. Staged bytes additionally require
an exact byte size. Hash-only provenance parents can remain metadata references.
The license-ID set is explicit; review an unsupported license before extending it.
Mixed-license source bundles need their component records/lineage reviewed too.

Run against a frozen staging directory with each relevant existing inventory:

```text
python tools/ci/check_material_staging.py --stage STAGE --inventory INVENTORY.json
```

Repeat `--inventory` and `--exclude-root` as needed. Every staged file must match
an admitted record; missing/unreviewed/restricted bytes, restricted ancestors,
unknown/cyclic derivative sources and linked/reparse paths refuse staging. An
open derivative cannot reference an NC parent. This is development/release tooling,
not a Python product dependency or a runtime restriction on private user input.
The L01 native profile and its staging/collection entry points are described in
[native release material admission](../native-release-materials.md). Actual local
archive coverage is recorded in the feature-expansion receipt; P02 separately
owns final install/use/removal and publication. Historical products retain their
original evidence and are not retrospectively qualified by this inventory.

The cached upstream OpenCV carrier at commit
`31b0eeea0b44b370fd0712312df4214d4ae1b158`, SHA256
`bb15d5c85bab38c1377c0c7dd7ff201b078bcdb41cc4ff9f85ac61aa56b5f631`,
contains `samples/data/lena.jpg` and `doc/js_tutorials/js_assets/lena.jpg`.
The project has no reviewed independent open-image grant for those photographs.
Classify the mixed carrier as unreviewed for new collection/staging; preserve its
historical cache/receipts without using it as release membership evidence.
No new download is authorized by this metadata. Future unresolved material copies
belong in the external unreviewed pool above. The native OpenCV builder now uses
only the reviewed vendored core/ML subset; its original archive is never a fallback.
The exact selection manifest records retained code and excluded assets; an Apache
code license does not admit those omitted photographs or unrelated model files.

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

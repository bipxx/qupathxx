# Public NuCLS density pilot

Completed on2026-11-06 using unchanged installed Windows candidate4 and the
delivered standalone manifest generator/batch runner. Two different TCGA cases,
60 source annotations,2 density rows; original saved project bytes unchanged.

The authors' [corrected single-rater dataset](https://sites.google.com/view/nucls/single-rater)
is CC0-1.0. Their [format specification](https://sites.google.com/view/nucls/data-format)
states0.2 micrometers/pixel. Citation: Amgad et al., *NuCLS: A scalable crowdsourcing
approach and dataset for nucleus classification and segmentation in breast
cancer*, GigaScience2022, [doi:10.1093/gigascience/giac037](https://doi.org/10.1093/gigascience/giac037).
The seven source assets total396720 bytes. RGB/CSV/mask downloads came directly
from the authors' linked Google Drive folders; no full dataset or model download.
[sources.json](sources.json) pins URLs, exact sizes/SHA256 and selection criteria.

| Case | Source annotations | Centroids inside FOV | FOV area mm² | Annotated nuclei/mm² |
| --- | ---: | ---: | ---: | ---: |
| TCGA-A1-A0SP-DX1 | 32 | 31 | 0.00416024 | 7451.493183 |
| TCGA-A2-A0CM-DX1 | 28 | 27 | 0.00404496 | 6674.973300 |

These are annotation-density observations for two fields, not comparative
cohort statistics. All source classes are included, including AMBIGUOUS.
One centroid per case is outside its annotation FOV and is correctly uncounted.

The annotation support is the rectangle bounded by the author's FOV mask code253,
with every pixel inside checked nonzero and the rectangle contained in both
RGB/mask extents. It is supplied as both window and support to the existing
native density API; no exclusions are requested. This denominator is annotated
FOV area, **not validated tissue area**. Native CSV says cells/mm2; in this pilot
each native Cell represents an annotated nucleus centroid.

Original masks differ from RGB extents by one outer pixel:378x367 versus378x368,
and332x345 versus331x345. The selected FOVs lie inside both extents; no resampling
or image cropping is applied. One source bounding box extends beyond RGB by a
pixel. The first full-outline import was correctly rejected by native Document.
The final project explicitly uses1px square Cell/nucleus markers at original
annotation centroids, retaining source CSVs unchanged. Rectangle centroids are
box centers; polygon centroids use signed-area moments. These marker geometries
are not nuclear outlines or whole-cell bodies and must not be used as such for
shape measurements. No segmentation model or tissue detector was run.

| Selected pilot check | Status / evidence |
| --- | --- |
| Open dataset license, stated calibration, bounded download | Complete; sources.json,7 files verified |
| Two distinct case/field identities, original source retained | Complete; workflow.json |
| Preview class selections/calibration without analysis | Complete; preview.json, valid=true |
| Generate explicit manifest and run unchanged installed consumers | Complete; workflow.py,2 computed images/2 rows |
| Counts and calibrated areas match independent checks | Complete; workflow.json, relative tolerance1e-12 |
| Source project unchanged, failure retained, representation limits explicit | Complete; source SHA in workflow.json; limits above |

[workflow.json](workflow.json) records native/script hashes, source SHA, all
expected values and dimensions. [density.csv](density.csv) is the actual combined
native-export table with image context; its checkpoint paths name local artifacts.
[preview.json](preview.json) records the actual preview/explicit IDs. Raw initial
failure and final command logs, downloads, project and checkpoints remain under
main build/research-density-pilot. No test/native/package rebuild or reinstall
was needed; existing candidate4 numerical qualification is not repeated here.

To reproduce, download each of the seven URLs in sources.json to its specified
filename in a source directory. With candidate4, Pillow and numpy installed:

```powershell
python -I docs/benchmarks/research-density-pilot/workflow.py path/to/source-files path/to/new-output
```

The output directory must be new. The script verifies every input hash, saves
the project, invokes the actual isolated generator preview/publication and batch
CLI, compares the combined CSV to independent centroid/rectangle arithmetic,
and verifies source-project integrity. It does not fetch files or train models.
No clinical accuracy, complete research-cohort validation, speed superiority,
reader expansion or whole-cell/tissue segmentation acceptance is claimed.

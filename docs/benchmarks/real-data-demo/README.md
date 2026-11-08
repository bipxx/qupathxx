# Real experimental demonstration data

User requested real fluorescence and omics data in the native app. Both viewers
were launched and their rendered data inspected with the computer-use skill.
Inputs and generated projects are retained locally under main build/real-data-demo;
large datasets are not committed. [Download sizes/source hashes](downloads.json),
[preparation/rendering receipt](receipt.json), [native preparation source](prepare.cpp).

## Fluorescence

OME Consortium tubhiswt-2D companion OME-TIFFs,512×512 with two uint8 channels:
[source directory](https://downloads.openmicroscopy.org/images/OME-TIFF/2016-06/tubhiswt-2D/),
[CC BY4.0 statement](https://downloads.openmicroscopy.org/images/OME-TIFF/2016-06/tubhiswt-2D/readme.txt).
Native raw statistics report channel ranges7–48 and0–255. The prepared project
sets RGB projection0/1/disabled with display windows7–48 and0–255; original files
and sample values are unchanged. Optional Python only constructs native options,
queries the C++ statistics and saves the native project; no Python image processing.
The initially dark default preview was corrected through these presentation options.

## Spatial gene expression

10x Genomics V1_Adult_Mouse_Brain, Mouse Brain Section (Coronal), published2020-06-23.
[Dataset and CC BY4.0 attribution](https://www.10xgenomics.com/datasets/mouse-brain-section-coronal-1-standard-1-1-0).
Downloaded only processed Matrix Market/TSV counts and PNG/CSV/JSON spatial files;
no FASTQ, HDF5, proprietary container or vendor SDK. Archive extraction is lossless.

C++ preparation streams the32,285-feature/16,031,101-entry source matrix, selecting
Gad1, Slc17a7, Pcp4, Aqp4, Mbp and Plp1 without normalizing or changing counts.
All2,702 filtered tissue barcodes are joined explicitly to their position rows.
The14,550 selected nonzero entries are exported to the existing open profile;
feature IDs retain both the readable symbol and original Ensembl ID.

Positions use x=published full-resolution column×0.17011142 and y=published
full-resolution row×0.17011142, exactly the supplied tissue_hires_scalef. No
registration/axis/scale guessing. Native LodePNG decodes the1921×2000 tissue PNG;
its opaque RGB bytes are embedded as the project background. Existing C++ sparse
importer adds point detections and selected counts. These markers show positions,
not physical spot footprints or an expression heatmap. Inspect selected detections'
Imported properties (historical) for the individual gene values.

One temporary C++ recipe target was injected into the existing cache and then
removed from its configuration. The recipe links existing core/JSON/LodePNG code;
no product CMake, reader, kernel, schema or binding implementation changed. Keep
its source/binary/data receipts local and source-scoped; this is a demonstration,
not biological accuracy or vendor-platform compatibility qualification.

# DICOM WSI reader plugin

`org.qupathxx.dicom-wsi` uses the Windows x64 OpenSlide 4.0.1.2 SDK for
pyramid display and the existing ABI1 file-set extension for source tracking.
Select it explicitly for DICOM WSI projects that should guard their companions.
The provider reports straight RGBA8 display and physical XY calibration.
Raw samples are unavailable.

The selected same-directory profile follows Series Instance UID and the
OpenSlide image roles `VOLUME`, `LABEL`, `OVERVIEW`, and `THUMBNAIL`. It
excludes other series and refuses duplicate instance IDs, reparse entries and
more than 63 companions. The scan bounds entries visited and bytes read per
file. OpenSlide and libdicom manage their own internal decoder allocations.
The host records native file identity and version stamps for each selected
file. Its saved link checks those observations before reading. To use a
changed source, review and adopt it through the project source commands.

## Build and use

Use the matching Windows ReaderPluginKit and the exact SDK and corresponding
source archive from [source-lock.json](source-lock.json). Configure CMake with
`QUPATHXX_READER_PLUGIN_KIT_PREFIX`, `QUPATHXX_DICOM_WSI_OPENSLIDE_SDK`, and
`QUPATHXX_DICOM_WSI_SOURCE_ARCHIVE`, then build and install in x64 Release.
The build extracts only the listed code and notices. It requires no downloads.

Place `qpx_dicom_wsi_plugin.dll` and `dicom-wsi-reader-plugins.json` beside
the native executable and its matching `libopenslide-1.dll`. This lets the
plugin loader verify the same library file that the executable already loaded.
Keep the staged notices and source lock with the provider.

```powershell
qupathxx_format_import --reader-plugins C:/QPX/dicom-wsi-reader-plugins.json project-create SLIDE.dcm STUDY.qpx --reader org.qupathxx.dicom-wsi
qupathxx_region --reader-plugins C:/QPX/dicom-wsi-reader-plugins.json --project-region STUDY.qpx 1 0 0 0 128 128 CROP.pam --receipt
```

## Licensing and evidence

The module code is AGPL-3.0-only. The statically linked metadata parser is
libdicom 1.3.0 (MIT) with uthash 2.3.0 (BSD-1-Clause). The exact OpenSlide SDK
runtime and its dependencies retain their own terms, including OpenSlide's
LGPL-2.1-only license. The install copies the selected dependency notices.
The source lock identifies the corresponding archive and exact members.

Local Windows evidence covers the two-instance JPEG 2000 concatenation and
the Leica-4 sparse JPEG series. See the [verification notes](../../docs/current-task.md#verified-locally---optional-dicom-wsi-companion-guard).

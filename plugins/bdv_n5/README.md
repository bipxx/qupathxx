# BigDataViewer N5 UInt16 reader

Open a local project folder containing `dataset.xml` and the relative N5 store
it references. The profile reads one setup or co-registered setups declared as
distinct channels, with contiguous timepoints and matched UInt16 XYZ pyramids.
The channels are exposed in ascending setup-ID order. The reader preserves XML
voxel calibration and requires equal integer X and Y downsampling, unchanged Z,
and the declared voxel scale with zero translation for every view.

```powershell
$manifest = (Resolve-Path .\bin\bdv-n5-reader-plugins.json).Path
.\bin\qupathxx_region.exe --reader-plugins $manifest --reader org.qupathxx.bdv-n5 --raw PROJECT_FOLDER OUTPUT.npy --level 1 --time 0 --z 0
```

Keep the module, exact dependencies, CLI and manifest together. Save exports
outside the project directory. Reader support covers local N5 2.0 raw and
gzip/zlib blocks. Channels may share one declared tile, angle and
illumination. Explicit tile location and rotation must be zero. Reading
distinct tiles or angles and general affine mapping require separate
profiles. Use the BDV HDF5 plugin for HDF5 projects.

A project can declare missing setup/timepoint pairs in its XML. Channel
and timepoint indices retain their original positions. You can read a
present channel at a partially acquired timepoint with `--channel` and
`--channels 1`. A request that includes a missing view fails before export.

For an offline build, configure `source/plugins/bdv_n5` using Visual Studio
2022, `-A x64 -T version=14.44.35207`, Release /MD and these paths:

| Setting | Path |
| --- | --- |
| `qupathxx_DIR` | Matching FP04 SDK `lib/cmake/qupathxx` |
| `QUPATHXX_READER_PLUGIN_KIT_PREFIX` | `source/reader-plugin-kit` |
| `QUPATHXX_BDV_JSON_INCLUDE` | `source/third_party/native-overlay/json/include` |
| `QUPATHXX_BDV_ZLIB_INCLUDE` | `source/third_party/zlib/include` |
| `QUPATHXX_BDV_XML_INCLUDE` | `source/third_party/tinyxml2` |

See `SOURCE.json` and `FROZEN-SOURCE-AVAILABILITY.json` for the exact
prerequisites and corresponding-source references. Close the CLI and remove
only the files owned by this package as listed in `PACKAGE.json`.

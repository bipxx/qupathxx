# BigDataViewer HDF5 UInt16 reader

Open a local project folder containing `dataset.xml` and its relative HDF5
file or master and partition files. This profile reads one setup or co-registered setups declared as distinct
channels, with contiguous timepoints and matched UInt16 XYZ pyramids. The
channels are exposed in ascending setup-ID order. Pyramid levels require equal
integer X and Y downsampling and unchanged Z.
Each view must use the XML voxel scale and zero translation.

The reader preserves the unsigned meaning of legacy signed Int16 cells
when no setup datatype attribute is present. Explicit UInt16 uses native
unsigned storage and a fixed `uint16` datatype attribute. The reader accepts
local unfiltered or Deflate chunks and defined zero fill.

Partitioned projects require disjoint setup and timepoint maps that cover
every available view.
The corresponding master links must match the XML declarations. The reader
opens the declared local partition files directly and requires their setup
tables to agree with the master. Channels may share one declared tile, angle
and illumination. Explicit tile location and rotation must be zero. Distinct
tiles or angles, other datatypes, and general affine registration require
separate profiles.

A project can declare missing setup/timepoint pairs in its XML. Channel
and timepoint indices retain their original positions. You can read a
present channel at a partially acquired timepoint with `--channel` and
`--channels 1`. A request that includes a missing view fails before export.

```powershell
$manifest = (Resolve-Path .\bin\bdv-hdf5-reader-plugins.json).Path
.\bin\qupathxx_region.exe --reader-plugins $manifest --reader org.qupathxx.bdv-hdf5 --raw PROJECT_FOLDER OUTPUT.npy --level 1 --time 0 --z 0
```

Keep the module, manifest, exact dependencies and CLI together, and save
exports outside the project folder.

For an offline build, configure `source/plugins/bdv_hdf5` using Visual Studio
2022, `-A x64 -T version=14.44.35207`, Release /MD and these paths:

| Setting | Path |
| --- | --- |
| `qupathxx_DIR` | Matching FP04 SDK `lib/cmake/qupathxx` |
| `QUPATHXX_READER_PLUGIN_KIT_PREFIX` | `source/reader-plugin-kit` |
| `QUPATHXX_BDV_HDF5_INCLUDE` | `source/third_party/loom/hdf5/src` |
| `QUPATHXX_BDV_HDF5_GENERATED_INCLUDE` | `source/third_party/loom/hdf5/generated` |
| `QUPATHXX_BDV_ZLIB_INCLUDE` | `source/third_party/zlib/include` |
| `QUPATHXX_BDV_XML_INCLUDE` | `source/third_party/tinyxml2` |

`SOURCE.json` and `FROZEN-SOURCE-AVAILABILITY.json` identify the exact
prerequisites and corresponding sources. Close the CLI and remove only the
package-owned files in `PACKAGE.json` to uninstall.

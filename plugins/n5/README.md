# N5 TensorStore native image reader

Select the dataset folder containing its own `attributes.json`. The local reader
supports explicitly labeled UInt16 XY volumes with optional Z, channel and time
axes, TensorStore axis calibration, and raw or gzip/zlib-compressed blocks.
Select a separate dataset folder for each image.

```powershell
$manifest = (Resolve-Path .\bin\n5-reader-plugins.json).Path
.\bin\qupathxx_region.exe --reader-plugins $manifest --readers
.\bin\qupathxx_region.exe --reader-plugins $manifest --reader org.qupathxx.n5 --raw INPUT.n5\DATASET OUTPUT.npy --z 0 --time 0 --channel 0
```

The raw export destination must be outside the selected dataset. Keep the CLI,
module, manifest and exact declared DLLs together. Use the Windows x64 Visual
Studio 2022 generator, `-A x64 -T version=14.44.35207` and Release /MD. Build the
module from `source/plugins/n5` with these CMake settings:

| Setting | Path |
| --- | --- |
| `qupathxx_DIR` | Matching FP04 SDK `lib/cmake/qupathxx` |
| `QUPATHXX_READER_PLUGIN_KIT_PREFIX` | `source/reader-plugin-kit` |
| `QUPATHXX_N5_JSON_INCLUDE` | `source/third_party/native-overlay/json/include` |
| `QUPATHXX_N5_ZLIB_INCLUDE` | `source/third_party/zlib/include` |

The matching core SDK and store-aware region CLI retain their own corresponding
source and dependency terms. This profile is separate from BigDataViewer
projects, pyramids, affine registration and arbitrary N5 arrays. Close the CLI
and remove only package-owned files listed in `PACKAGE.json` to uninstall.

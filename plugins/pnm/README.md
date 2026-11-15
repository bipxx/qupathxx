# Native PBM/PGM/PPM/PAM reader plugin

`org.qupathxx.pnm` version1.0.0, contract `pnm-pam-uint-samples/1`, is a native
image C ABI reader for a declared single-image Netpbm profile. All runtime
parsing, sample interpretation, transfer conversion and validation are C++.
The independently built module uses the matching ReaderPluginKit and platform
runtime, with no external codec, application-core or Python dependency.

## Samples and display contract

| Encoding | Stored samples and raw channels |
| --- | --- |
| P1 / P4 PBM | Literal0=white,1=black codes unpacked to UInt8; `Blackness` |
| P2 / P5 PGM | Original unsigned code values; `Gray` |
| P3 / P6 PPM | Original unsigned code values; `Red`, `Green`, `Blue` |
| P7 PAM | Declared `BLACKANDWHITE`, `GRAYSCALE`, `RGB` or corresponding `_ALPHA` tuple; Gray/RGB and optional Alpha |

Maxval is1–65535. Values with maxval≤255 use UInt8, otherwise UInt16 in packed
native-endian HWC storage. Binary16-bit file samples are big endian; raw reads
preserve their original integer codes without normalization. PBM preserves its
1=black polarity, which differs from PAM `BLACKANDWHITE`0=black/1=white.
PBM row padding is ignored; arbitrary tuple meanings are unsupported. PAM
black-and-white tuples require maxval1 and the matching channel count.

Every in-image raw pixel is valid, including alpha0. Out-of-image raw samples
are zero/invalid; display padding is transparent black. Display RGBA8 uses
straight alpha and preserves hidden RGB at alpha0. Each color/gray code is
normalized by maxval for display only. Alpha is linear opacity normalized to8-bit,
without the color transfer function.

The standard PGM/PPM interpretation is BT.709-encoded Gray/RGB. Empty native
options use `bt709`; the generated manifest makes that default explicit. These
canonical options select known sample-meaning variants:

```json
{"transfer":"bt709"}
{"transfer":"srgb"}
{"transfer":"linear"}
```

For `bt709`, inverse BT.709 followed by the sRGB display transfer produces RGBA8.
For `linear`, only the sRGB display transfer applies. `srgb` uses normalized
encoded values directly. Quantization rounds to nearest8-bit with half values
upward. These choices do not change raw codes. Source files often cannot identify
their transfer variant; select the correct known interpretation explicitly.
The native source binding persists normalized options and their identity.
Changing manifest defaults preserves saved explicit choices; fresh attachments
use the new default. Other options refuse at the native boundary.

There is one level, `Z=0`, `T=0`. PNM/PAM supplies no physical calibration in
this profile; all calibration flags remain unknown. Comments do not supply scale,
orientation or registration. Typed raw access does not grant TIFF/Zarr views or
admit this provider into an otherwise reader-restricted scientific recipe.

## Strict input profile and resource bounds

P1/P2/P3 are plain ASCII encodings. P1 permits consecutive bit characters;
P2/P3 use unsigned decimal sample tokens. ASCII whitespace and comments are
recognized without locale-dependent parsing. Values above maxval, signed/float
tokens, missing pixels and malformed/overflowing dimensions refuse.

P4/P5/P6 consume exactly one final header whitespace delimiter. Raster-leading
whitespace and `#` are pixel bytes. A second byte of a CR/LF pair is not silently
discarded as another delimiter. P7 uses bounded LF/CRLF header lines ending at
`ENDHDR`; duplicate required fields or inconsistent tuple/depth/maxval refuse.
Missing/unsupported tuple semantics and unknown meaningful header fields refuse.
Multiple TUPLTYPE lines retain their defined concatenated meaning; an unsupported
result is not silently reduced to a recognized tuple.

The profile reads exactly one image. Binary trailing bytes/concatenated images
refuse. Plain images allow trailing whitespace/comments, with additional data
refused. PFM/other magic and XV P7 thumbnail interpretation are unsupported.
Header parsing is bounded to64KiB, and source/input, raw raster, RGBA, lookup table
and conservative wrapper/path allocations must fit a work allowance≤256MiB.
Large rasters exceeding that full-decode profile refuse. Open/decode and raw reads
honor synchronous cancellation. No input/cancel/host-buffer pointer is retained.

## Build and use

Selected profile: Windows x64, MSVC1944/compiler19.44.35225.0, Release `/MD`.
CMake verifies the matching kit/compiler and has no network or dependency fetch.
In a matching Visual Studio developer shell, using absolute paths:

```powershell
cmake -S plugins/pnm -B BUILD -G Ninja -DCMAKE_BUILD_TYPE=Release `
  "-DQUPATHXX_READER_PLUGIN_KIT_PREFIX=KIT"
cmake --build BUILD --config Release
cmake --install BUILD --config Release --prefix INSTALL
```

The complete source kit uses `source/plugins/pnm` and `source/reader-plugin-kit`.
Tests default OFF. `QUPATHXX_PNM_BUILD_TESTS=ON` additionally requires the matched
native SDK for the project test consumer; the module itself remains independent.
Keep the DLL, hash-bound manifest and licenses together and configure before
catalog/source use:

```text
qupathxx_region --reader-plugins ABS_MANIFEST --readers
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.pnm INPUT.pgm 0 0 0 512 512 OUTPUT.pam
```

Automatic suffix routing remains under its existing native policy. Saved-source
currentness, implementation review, exact result replay and offline metadata use
the existing host authorities. Restart the host to change configured modules.
Close module users before removing byte-verified package-owned files; preserve
images, projects and outputs.

## Licenses and reference definitions

This independent decoder, authored fixtures and source kit retain the repository's
[AGPL-3.0-only license](../../LICENSE). The profile references the published
[PBM](https://netpbm.sourceforge.net/doc/pbm.html),
[PGM](https://netpbm.sourceforge.net/doc/pgm.html),
[PPM](https://netpbm.sourceforge.net/doc/ppm.html) and
[PAM](https://netpbm.sourceforge.net/doc/pam.html) layouts. The Netpbm program/library
and its varied component licenses are not dependencies of this module; no upstream
sample asset or implementation is packaged. The adapter's license is not assigned
to the upstream format specifications.

The [source-scoped receipt](../../docs/benchmarks/pnm-reader-plugin/README.md)
records this packet's tested artifacts and remaining scope.
No additional format, new host/GUI/binding build, Java compatibility, platform,
biological/clinical or speed qualification follows from this reader profile.

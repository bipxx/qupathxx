# LodePNG source pin

Unmodified upstream files from [lvandeve/lodepng](https://github.com/lvandeve/lodepng/tree/7b2893e35891c0c85bee5595caafaf3c2044b409)
at revision `7b2893e35891c0c85bee5595caafaf3c2044b409`. The [Zlib license](LICENSE) applies to these
files; retain their original notices. This is vendored source, included in the
application source archive, not a system codec or a Python decoder.

- `LICENSE` SHA256 `bdafa318b8e637d600ca1b60545e894881977a16a16b74809c95e805701621e3`
- `lodepng.cpp` SHA256 `32cdd7a395b5476a3fe4615305a99066dc4d37aaea6988643cca8b4eccc352cc`
- `lodepng.h` SHA256 `a49b98b45ea603a05e2a77bd6b0320e7535a9c6e18aef8ce904692a3e02b87ab`

Only the native decoder is compiled. Encoder, disk convenience, C++ convenience
and ancillary metadata parsing are disabled in the NuCLS target. No gamma/ICC
conversion occurs. Per-allocation and inflated-output bounds supplement the
NuCLS file/header/pixel limits. RGB8 pixel data is retained exactly; palette,
gray, RGBA and 16-bit inputs are refused without conversion.

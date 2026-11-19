"""Write and independently read the ENVI baseline with MIT-licensed Spectral Python 0.24."""
import argparse
from pathlib import Path

import numpy as np
import spectral
from spectral.io import envi


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir()
    assert spectral.__version__ == "0.24"
    cube = np.arange(36, dtype="<u2").reshape(3, 4, 3) * 137
    names = ["Blue band", "Green band", "Red band"]
    for layout, order in (("bip", 0), ("bil", 1), ("bsq", 1)):
        header = args.output / (layout + ".hdr")
        envi.save_image(str(header), cube, interleave=layout, byteorder=order, metadata={"band names": names})
        raster = envi.open(str(header))
        assert raster.metadata["band names"] == names
        pixels = raster.open_memmap(interleave="bip")
        assert pixels.shape == cube.shape and pixels.astype("<u2").tobytes() == cube.tobytes()

    masked = np.array([[[-9999, 11], [21, -9999], [31, 32]]], dtype="<i2")
    header = args.output / "masked.hdr"
    envi.save_image(str(header), masked, interleave="bil", byteorder=1,
                    metadata={"data ignore value": -9999})
    raster = envi.open(str(header))
    assert int(raster.metadata["data ignore value"]) == -9999
    np.testing.assert_array_equal(raster.open_memmap(interleave="bip"), masked)

    for name, values, dtype, sentinel, layout, order, code in (
        ("signed64", [[-(2**63), 7], [-(2**53 + 1), 8], [-(2**53), 9], [2**63 - 1, -1]],
         "<i8", -(2**53 + 1), "bil", 1, "14"),
        ("unsigned64", [[0, 1], [2**64 - 1, 2], [2**64 - 2, 3], [2**53 + 1, 4]],
         "<u8", 2**64 - 1, "bsq", 0, "15"),
    ):
        samples = np.array([values], dtype=dtype)
        header = args.output / (name + ".hdr")
        envi.save_image(str(header), samples, interleave=layout, byteorder=order,
                        metadata={"data ignore value": sentinel})
        raster = envi.open(str(header))
        assert raster.metadata["data type"] == code
        assert int(raster.metadata["data ignore value"]) == sentinel
        np.testing.assert_array_equal(raster.open_memmap(interleave="bip"), samples)

    for extension, layout in (("raw", "bil"), ("bil", "bil"), ("bip", "bip"), ("bsq", "bsq")):
        header = args.output / (f"named-{extension}.hdr")
        envi.save_image(str(header), cube, ext=extension, interleave=layout, byteorder=1,
                        metadata={"band names": names})
        raster = envi.open(str(header))
        assert Path(raster.filename).resolve() == header.with_suffix("." + extension).resolve()
        np.testing.assert_array_equal(raster.open_memmap(interleave="bip"), cube)


if __name__ == "__main__":
    main()

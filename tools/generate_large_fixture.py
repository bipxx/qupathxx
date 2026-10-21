"""Generate a redistributable tiled RGB pyramid without a full-image allocation."""
import argparse
from pathlib import Path

import numpy as np
import tifffile


def generate(path):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    tile = np.empty((256, 256, 3), dtype="uint8")
    tile[:] = (40, 80, 160)
    with tifffile.TiffWriter(path) as writer:
        for level in range(9):
            width, height = 100352 >> level, 2048 >> level
            count = ((width + 255) // 256) * ((height + 255) // 256)
            writer.write(
                (tile for _ in range(count)), shape=(height, width, 3),
                dtype="uint8", photometric="rgb", tile=(256, 256),
                compression="deflate", metadata=None, subfiletype=0 if level == 0 else 1,
            )
    return path


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("output", nargs="?", default="build/fixtures/large-pyramid.tif")
    print(generate(parser.parse_args().output))

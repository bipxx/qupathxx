"""Measure native fixed-stain transforms; I/O and Python byte copies separately."""
import argparse
import json
from pathlib import Path
import platform
import statistics
import time

import numpy as np
import qupathxx as q


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--size", type=int, default=1024)
    parser.add_argument("--passes", type=int, default=12)
    parser.add_argument("--output", type=Path, default=Path("build/stains.json"))
    args = parser.parse_args()
    if not 16 <= args.size <= 4096 or not 2 <= args.passes <= 100:
        parser.error("require size 16..4096 and passes 2..100")
    rgb = np.random.default_rng(298).integers(0, 256, (args.size, args.size, 3), dtype=np.uint8)
    server = q.dense_image_server(q.Image(args.size, args.size, rgb.ravel().tolist()))
    pixels = server.read_region(q.RegionRequest(0, 0, 0, args.size, args.size))
    basis = [[.65, .70, .29], [.07, .99, .11]]
    start = time.perf_counter()
    transform = q.StainTransform(basis, [250, 245, 230])
    construction_ms = (time.perf_counter()-start)*1000
    result = dict(platform=platform.platform(), processor=platform.processor(),
                  pixels=args.size**2, passes=args.passes, seed=298,
                  stains=transform.stains, white=transform.white,
                  construction_ms=construction_ms, output_bytes=args.size**2*12)
    od_reference = np.maximum(0, np.log10(np.asarray(transform.white)/np.maximum(rgb.astype(float), 1)))
    for label, run in [("od", transform.optical_densities), ("deconvolve", transform.deconvolve)]:
        output = run(pixels)  # Warm allocation/kernel; no timing or copy.
        timings = []
        for _ in range(args.passes):
            del output  # Exclude release of the preceding output from each timed call.
            start = time.perf_counter()
            output = run(pixels)
            timings.append((time.perf_counter()-start)*1000)
        start = time.perf_counter()
        copied = output.data
        result[label+"_byte_copy_ms"] = (time.perf_counter()-start)*1000
        actual = np.frombuffer(copied, dtype=np.float32).reshape(rgb.shape)
        expected = od_reference if label == "od" else np.linalg.solve(
            np.asarray(transform.stains).T, od_reference.reshape(-1, 3).T).T.reshape(rgb.shape)
        np.testing.assert_allclose(actual, expected, rtol=1e-6, atol=2e-6)
        result[label+"_max_abs_error"] = float(np.max(np.abs(actual-expected)))
        result[label+"_median_ms"] = statistics.median(timings)
        result[label+"_max_ms"] = max(timings)
        result[label+"_mpixels_per_second"] = args.size**2/(statistics.median(timings)*1000)
        result[label+"_checksum"] = float(actual.sum(dtype=np.float64))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()

"""Measure the actual tiled viewer; report CPU frame work separately from vsync."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import time

import qupathxx as q


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=Path)
    parser.add_argument("--frames", type=int, default=240)
    parser.add_argument("--output", type=Path, default=Path("build/viewer-benchmark.json"))
    args = parser.parse_args()
    if args.frames < 30:
        parser.error("at least 30 frames required")
    server = q.open_slide(str(args.image))
    document = q.Document(server)
    runs = []
    try:
        for label in ["first-view", "repeat-view"]:
            start = time.perf_counter()
            report = q.show(document, frame_limit=args.frames)
            runs.append({
                "name": label, "elapsed_seconds": time.perf_counter() - start,
                "frames": report.frames, "cpu_frame_p95_ms": report.cpu_frame_p95_ms,
                "uploaded_tiles": report.uploaded_tiles, "peak_gpu_payload_bytes": report.peak_gpu_bytes,
                "pending_tiles_at_exit": report.pending_tiles,
            })
    finally:
        server.close()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.image.open("rb") as file:
        digest = hashlib.file_digest(file, "sha256").hexdigest()
    result = {
        "platform": platform.platform(), "processor": platform.processor(),
        "python": platform.python_version(), "fixture": args.image.name,
        "sha256": digest,
        "window_logical_size": [1440, 900], "runs": runs,
        "scope": "Static fit view. CPU timing excludes swap/vsync and capture; includes tile planning/upload. No navigation or GPU completion latency claim.",
    }
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()

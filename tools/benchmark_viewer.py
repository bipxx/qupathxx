"""Measure the actual tiled viewer; report CPU frame work separately from vsync."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import time

import qupathxx as q
from qupathxx import _native


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=Path)
    parser.add_argument("--frames", type=int, default=240)
    parser.add_argument("--width", type=int, default=2560)
    parser.add_argument("--height", type=int, default=1440)
    parser.add_argument("--navigate", action="store_true", help="Replay wheel zoom and overview clicks through ImGui")
    parser.add_argument("--docked", action="store_true", help="Use the split docked research workspace")
    parser.add_argument("--output", type=Path, default=Path("build/viewer-benchmark.json"))
    args = parser.parse_args()
    if args.frames < 30:
        parser.error("at least 30 frames required")
    server = q.open_slide(str(args.image))
    document = q.Document(server)
    options = q.GuiOptions()
    options.width, options.height = args.width, args.height
    options.docked_workspace = args.docked
    if args.navigate:
        if args.frames < 120:
            parser.error("navigation requires at least 120 frames")
        events = []
        for frame in range(5, 35):
            event = q.GuiInput()
            event.frame, event.kind, event.wheel = frame, q.GuiInputKind.WHEEL, 1
            events.append(event)
        for frame, kind in [(40, q.GuiInputKind.MOVE), (41, q.GuiInputKind.LEFT_DOWN), (42, q.GuiInputKind.LEFT_UP)]:
            event = q.GuiInput()
            event.frame, event.kind, event.target = frame, kind, q.GuiInputTarget.OVERVIEW
            event.x, event.y = .7, .5
            events.append(event)
        options.inputs = events
    runs = []
    try:
        for label in ["first-view", "repeat-view"]:
            start = time.perf_counter()
            report = q.show(document, frame_limit=args.frames, options=options)
            runs.append({
                "name": label, "elapsed_seconds": time.perf_counter() - start,
                "frames": report.frames, "cpu_frame_p95_ms": report.cpu_frame_p95_ms,
                "uploaded_tiles": report.uploaded_tiles, "peak_gpu_payload_bytes": report.peak_gpu_bytes,
                "pending_tiles_at_exit": report.pending_tiles,
                "framebuffer_size": [report.framebuffer_width, report.framebuffer_height],
                "generations": report.generations, "gl_renderer": report.gl_renderer,
                "docked_workspace": report.docked_workspace, "overview_state": report.overview_state,
                "overview_gpu_payload_bytes": report.overview_gpu_bytes,
                "gl_version": report.gl_version,
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
        "source_tree_sha256": q.build_environment().source_tree_sha256,
        "native_sha256": hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),
        "window_logical_size": [args.width, args.height], "runs": runs,
        "scope": ("ImGui zoom/overview replay" if args.navigate else "Static fit view") +
                 ". CPU timing excludes explicit swap/vsync and capture; includes planning/upload/draw submission. Driver calls may block. No GPU completion latency claim.",
    }
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()

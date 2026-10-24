"""Measure the actual tiled viewer; report CPU frame work separately from vsync."""
import argparse
import hashlib
import json
import os
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
    parser.add_argument("--ui-scale", type=float, default=1)
    parser.add_argument("--scale-cycle", action="store_true", help="Replay live UI multipliers 1.5, 1, 1.25 at frames 60, 90, 120")
    parser.add_argument("--output", type=Path, default=Path("build/viewer-benchmark.json"))
    parser.add_argument("--profile-directory", type=Path,
                        help="Retain native per-phase frame profiles for both views")
    args = parser.parse_args()
    if args.frames < 30:
        parser.error("at least 30 frames required")
    server = q.open_slide(str(args.image))
    document = q.Document(server)
    options = q.GuiOptions()
    options.width, options.height = args.width, args.height
    options.docked_workspace = args.docked
    options.ui_scale = args.ui_scale
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
    if args.scale_cycle:
        if args.frames < 150:
            parser.error("scale cycle requires at least 150 frames")
        events = list(options.inputs)
        for frame, multiplier in [(60, 1.5), (90, 1), (120, 1.25)]:
            event = q.GuiInput()
            event.frame, event.kind, event.x = frame, q.GuiInputKind.SET_UI_SCALE, multiplier
            events.append(event)
        options.inputs = events
    runs = []
    previous_profile = os.environ.get("QUPATHXX_GUI_PROFILE")
    if args.profile_directory is not None:
        args.profile_directory = args.profile_directory.resolve()
        args.profile_directory.mkdir(parents=True, exist_ok=True)
    try:
        for label in ["first-view", "repeat-view"]:
            profile_path = None
            if args.profile_directory is not None:
                profile_path = args.profile_directory / (label + ".json")
                os.environ["QUPATHXX_GUI_PROFILE"] = str(profile_path)
            start = time.perf_counter()
            report = q.show(document, frame_limit=args.frames, options=options)
            runs.append({
                "name": label, "elapsed_seconds": time.perf_counter() - start,
                "frames": report.frames, "cpu_frame_p95_ms": report.cpu_frame_p95_ms,
                "max_cpu_frame_ms": report.max_cpu_frame_ms,
                "rendered_frame_wall_p95_ms": report.rendered_frame_wall_p95_ms,
                "max_rendered_frame_wall_ms": report.max_rendered_frame_wall_ms,
                "rendered_frame_wall_samples": report.rendered_frame_wall_samples,
                "uploaded_tiles": report.uploaded_tiles, "peak_gpu_payload_bytes": report.peak_gpu_bytes,
                "pending_tiles_at_exit": report.pending_tiles,
                "framebuffer_size": [report.framebuffer_width, report.framebuffer_height],
                "generations": report.generations, "gl_renderer": report.gl_renderer,
                "docked_workspace": report.docked_workspace, "overview_state": report.overview_state,
                "overview_gpu_payload_bytes": report.overview_gpu_bytes,
                "gl_version": report.gl_version,
                "monitor_scale": report.monitor_scale, "framebuffer_scale": report.framebuffer_scale,
                "effective_ui_scale": report.ui_scale, "font_rebuilds": report.font_rebuilds,
                "ui_stacked_fields": report.ui_stacked_fields, "ui_wrapped_buttons": report.ui_wrapped_buttons,
                "ui_overflow_items": report.ui_overflow_items, "ui_max_overflow": report.ui_max_overflow,
                "ui_overflow_label": report.ui_overflow_label,
            })
            if profile_path is not None:
                profile_bytes = profile_path.read_bytes()
                profile = json.loads(profile_bytes)
                if (profile.get("schema") != "qpx.gui-profile/1" or
                        profile.get("source_tree_sha256") != q.build_environment().source_tree_sha256 or
                        len(profile.get("frames", [])) != min(report.frames, 4096)):
                    raise RuntimeError("native frame profile does not match the measured view")
                runs[-1]["native_phase_profile"] = {
                    "path": str(profile_path),
                    "sha256": hashlib.sha256(profile_bytes).hexdigest(),
                    "samples": len(profile["frames"]),
                    "scope": "CPU phases before swap/capture; bounded first 4096 frames, including warmup",
                }
    finally:
        server.close()
        if args.profile_directory is not None:
            if previous_profile is None:
                os.environ.pop("QUPATHXX_GUI_PROFILE", None)
            else:
                os.environ["QUPATHXX_GUI_PROFILE"] = previous_profile
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.image.open("rb") as file:
        digest = hashlib.file_digest(file, "sha256").hexdigest()
    result = {
        "schema": "qpx.viewer-benchmark/2",
        "build_git_revision": q.build_environment().git_revision,
        "tool_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "performance_accepted": False,
        "platform": platform.platform(), "processor": platform.processor(),
        "python": platform.python_version(), "fixture": args.image.name,
        "sha256": digest,
        "source_tree_sha256": q.build_environment().source_tree_sha256,
        "native_sha256": hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),
        "window_logical_size": [args.width, args.height], "ui_multiplier": args.ui_scale,
        "scale_cycle": args.scale_cycle, "runs": runs,
        "scope": ("ImGui zoom/overview replay" if args.navigate else "Static fit view") +
                 ". CPU timing excludes explicit swap/vsync and capture; includes planning/upload/draw submission. Driver calls may block. Rendered-loop wall metrics include swap/capture/post-paint startup dispatch; suspended iterations and final shutdown are excluded. Retains at most 4096 recent samples, excluding the first ten frames unless startup instrumentation is requested. No GPU completion or photon-latency claim.",
    }
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()

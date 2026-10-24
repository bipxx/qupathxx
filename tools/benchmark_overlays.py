"""Paired immediate/cached rendering with exact framebuffer comparisons."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import time

import qupathxx as q
import qupathxx._native as native


def main():
    if not __debug__:
        raise RuntimeError("framebuffer and cache-budget checks require Python assertions enabled")
    parser = argparse.ArgumentParser()
    parser.add_argument("--objects", type=int, default=100000)
    parser.add_argument("--frames", type=int, default=180)
    parser.add_argument("--budget-mib", type=int, default=64)
    parser.add_argument("--width", type=int, default=2560)
    parser.add_argument("--height", type=int, default=1440)
    parser.add_argument("--zoomed", action="store_true")
    parser.add_argument("--output", type=Path, default=Path("build/overlays.json"))
    args = parser.parse_args()
    if not 1 <= args.objects <= 100000 or not 30 <= args.frames <= 1000 or not 1 <= args.budget_mib <= 256:
        parser.error("objects 1..100000, frames 30..1000, budget 1..256 MiB")
    if not 640 <= args.width <= 8192 or not 480 <= args.height <= 8192:
        parser.error("width 640..8192; height 480..8192")
    doc = q.Document(q.Image(1024, 1024, [35, 60, 90]*(1024*1024)))
    doc.set_history_limits(0, 0)
    doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(i % 1024, (i//1024) % 1024, .5, .5))) for i in range(args.objects)])
    revision = doc.revision
    output = dict(schema="qpx.overlay-benchmark/2", performance_accepted=False, validation_passed=False,
                  build_git_revision=q.build_environment().git_revision,
                  source_tree_sha256=q.build_environment().source_tree_sha256,
                  tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                  platform=platform.platform(), processor=platform.processor(), objects=args.objects,
                  native_sha256=hashlib.sha256(Path(native.__file__).read_bytes()).hexdigest(),
                  zoomed=args.zoomed, window_logical_size=[args.width,args.height], runs=[])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    pixels = None
    for label, budget in [("immediate", 0), ("cached", args.budget_mib*1024*1024)]:
        options = q.GuiOptions()
        options.width, options.height = args.width, args.height
        options.overlay_cache_budget, options.settle_tiles = budget, True
        # Establish the canvas as the keyboard target before selecting. Keep
        # setup inside the excluded first ten frames, including cold windows.
        events = []
        for number, kind in [(2, q.GuiInputKind.MOVE), (3, q.GuiInputKind.LEFT_DOWN),
                             (4, q.GuiInputKind.LEFT_UP), (7, q.GuiInputKind.SELECT_ALL_KEY)]:
            event = q.GuiInput()
            event.frame, event.kind = number, kind
            event.x, event.y = .5, .5
            if number == 2:
                event.wait_for = q.GuiInputWait.WINDOW_READY
            events.append(event)
        options.inputs = events
        if args.zoomed:
            view = q.Viewport()
            view.zoom, view.origin = 32, q.Point(-8192, -min(32, args.objects//1024/2)*32)
            options.start_view = view
        capture = args.output.with_suffix(f".{label}.ppm")
        start = time.perf_counter()
        report = q.show(doc, frame_limit=args.frames, capture_path=str(capture), options=options)
        frame = capture.read_bytes()
        output["runs"].append(dict(name=label, budget=budget, elapsed_seconds=time.perf_counter()-start,
            validation_passed=False, selected=len(report.selected_ids),
            document_revision_before=revision,document_revision_after=doc.revision,
            framebuffer_matches_reference=None if pixels is None else frame == pixels,
            frames=report.frames, visible=report.visible_objects, cpu_frame_p95_ms=report.cpu_frame_p95_ms,
            max_cpu_frame_ms=report.max_cpu_frame_ms, cache_bytes=report.overlay_cache_bytes,
            rendered_frame_wall_p95_ms=report.rendered_frame_wall_p95_ms,
            max_rendered_frame_wall_ms=report.max_rendered_frame_wall_ms,
            rendered_frame_wall_samples=report.rendered_frame_wall_samples,
            framebuffer_size=[report.framebuffer_width,report.framebuffer_height],
            replay_complete=report.replay_complete,tiles_settled=report.tiles_settled,
            pending_tiles_at_exit=report.pending_tiles,ui_overflow_items=report.ui_overflow_items,
            cache_peak_bytes=report.overlay_cache_peak_bytes, builds=report.overlay_cache_builds,
            hits=report.overlay_cache_hits, fallbacks=report.overlay_cache_fallbacks,
            framebuffer_sha256=hashlib.sha256(frame).hexdigest(), gl_renderer=report.gl_renderer))
        # Preserve diagnostic data before checking a trial, including failures.
        args.output.write_text(json.dumps(output, indent=2)+"\n", encoding="utf-8")
        if pixels is None:
            pixels = frame
        else:
            assert frame == pixels, "cache changed framebuffer pixels"
        assert output["runs"][-1]["selected"] == args.objects, f"selected {output['runs'][-1]['selected']} of {args.objects}; replay_complete={report.replay_complete}"
        assert doc.revision == revision, f"document revision changed from {revision} to {doc.revision}"
        assert report.overlay_cache_peak_bytes <= budget and report.overlay_cache_bytes <= budget
        output["runs"][-1]["validation_passed"] = True
    output["validation_passed"] = True
    output["scope"] = "Warm synthetic static viewer with exact immediate/cached framebuffer comparison. CPU frame work includes driver submission, excludes explicit swap/vsync and capture. Rendered-loop wall metrics include swap/capture; both metrics exclude the first ten frames and final teardown. No GPU-completion or general hardware guarantee."
    args.output.write_text(json.dumps(output, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(output, indent=2))


if __name__ == "__main__":
    main()

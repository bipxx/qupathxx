"""Record real async modal waits and settled framebuffer publication."""
import argparse
from contextlib import contextmanager
import json
from pathlib import Path
import platform
import subprocess
import sys
import time
import uuid

import qupathxx as q


def event(frame, kind, target=q.GuiInputTarget.CANVAS, wait=q.GuiInputWait.IMMEDIATE):
    value = q.GuiInput()
    value.frame, value.kind, value.target, value.wait_for = frame, kind, target, wait
    return value


@contextmanager
def locked(path, ready):
    script = "import sqlite3,sys,time;from pathlib import Path;db=sqlite3.connect(sys.argv[1]);db.execute('BEGIN EXCLUSIVE');Path(sys.argv[2]).write_text('ready');time.sleep(.75);db.rollback();db.close()"
    child = subprocess.Popen([sys.executable, "-I", "-c", script, str(path), str(ready)])
    try:
        until = time.monotonic()+5
        while not ready.exists() and time.monotonic() < until:
            time.sleep(.01)
        assert ready.exists(), "lock process did not become ready"
        yield
    finally:
        child.wait(timeout=5)


def counters(report):
    assert report.replay_complete
    assert report.frames == report.replay_frame+report.replay_wait_frames+report.settle_frames
    return dict(frames=report.frames, logical_frames=report.replay_frame,
                wait_frames=report.replay_wait_frames, settle_frames=report.settle_frames,
                cpu_frame_p95_ms=report.cpu_frame_p95_ms, max_cpu_frame_ms=report.max_cpu_frame_ms)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rounds", type=int, default=3)
    parser.add_argument("--output", type=Path, default=Path("build/gui-replay.json"))
    args = parser.parse_args()
    if not 1 <= args.rounds <= 20:
        parser.error("rounds must be 1..20")
    directory = Path("build/gui-replay-benchmark")/uuid.uuid4().hex
    directory.mkdir(parents=True)
    recordings = []
    for iteration in range(args.rounds):
        project = q.Project()
        doc = q.Document(q.Image.demo())
        image = project.add_document(doc)
        obj = doc.add_rectangle(q.Rect(10, 10, 20, 20), "Original")
        view = q.ProjectView()
        view.center, view.zoom = q.Point(512, 384), 1
        project.set_view(image, view)
        path = directory/f"source-{iteration}.qpxproj"
        project.save(str(path))
        doc.rename_annotation(obj, "Captured")
        options = q.GuiOptions()
        options.autosave_interval_seconds = 0
        options.inputs = [event(3, q.GuiInputKind.SAVE_KEY), event(7, q.GuiInputKind.UNDO_KEY),
                          event(10, q.GuiInputKind.CLOSE_WINDOW),
                          event(11, q.GuiInputKind.LEFT_DOWN, q.GuiInputTarget.DISCARD_CLOSE, q.GuiInputWait.TARGET_READY),
                          event(12, q.GuiInputKind.LEFT_UP, q.GuiInputTarget.DISCARD_CLOSE)]
        with locked(path, directory/f"ready-{iteration}"):
            began = time.perf_counter()
            save = q.show(project, frame_limit=100, options=options)
            save_wall = (time.perf_counter()-began)*1000
        assert not save.project_error and save.saves_completed == 1 and save.replay_frame < 100
        assert save.replay_wait_frames > 0 and project.dirty and doc.annotation(obj).name == "Original"
        assert q.Project.open(str(path)).document(image).annotation(obj).name == "Captured"

        target = q.Project()
        options = q.GuiOptions()
        options.startup_kind, options.startup_path = "project", str(directory/"missing.qpxproj")
        options.project_open_path, options.autosave_interval_seconds = str(path), 0
        options.settle_tiles = True
        options.inputs = [event(2, q.GuiInputKind.OPEN_PROJECT_KEY, wait=q.GuiInputWait.STARTUP_FINISHED),
                          event(3, q.GuiInputKind.LEFT_DOWN, q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT, q.GuiInputWait.TARGET_READY),
                          event(4, q.GuiInputKind.LEFT_UP, q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT)]
        began = time.perf_counter()
        fallback = q.show(target, frame_limit=20, options=options)
        fallback_wall = (time.perf_counter()-began)*1000
        assert not fallback.startup_succeeded and not fallback.project_error and fallback.project_replacements == 1
        assert target.identity == project.identity and target.document(image).annotation(obj).name == "Captured"
        assert fallback.replay_wait_frames > 0 and fallback.tiles_settled and fallback.pending_tiles == 0

        pixels = q.Document(q.Image(512, 512, [30, 120, 210]*(512*512)))
        options = q.GuiOptions()
        options.settle_tiles = True
        capture = directory/f"settled-{iteration}.ppm"
        began = time.perf_counter()
        settled = q.show(pixels, frame_limit=1, capture_path=str(capture), options=options)
        settle_wall = (time.perf_counter()-began)*1000
        assert settled.tiles_settled and settled.pending_tiles == 0 and settled.uploaded_tiles == 4
        assert settled.settle_frames >= 1 and settled.replay_frame == 1
        header, dimensions, maximum, data = capture.read_bytes().split(b"\n", 3)
        width, height = map(int, dimensions.split())
        offset = (height//2*width+int(width*.65))*3
        assert header == b"P6" and maximum == b"255" and data[offset:offset+3] == bytes([30, 120, 210])
        recordings.append(dict(save=dict(**counters(save), wall_ms=save_wall, write_ms=save.save_write_ms),
                               fallback=dict(**counters(fallback), wall_ms=fallback_wall),
                               settlement=dict(**counters(settled), wall_ms=settle_wall, gpu_bytes=settled.peak_gpu_bytes)))
    output = dict(platform=platform.platform(), processor=platform.processor(), gl_renderer=settled.gl_renderer,
                  cpu_frame_sampling="Non-startup physical frames >=10; zero means no samples",
                  lock_seconds=.75, timeout_seconds=options.replay_timeout_seconds, recordings=recordings)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(output, indent=2))


if __name__ == "__main__":
    main()

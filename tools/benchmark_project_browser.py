"""Real desktop project switching with a large retained selection and catalog."""
import argparse
import json
from pathlib import Path
import platform

import qupathxx as q


def replay(frame, kind):
    result = q.GuiInput()
    result.frame, result.kind = frame, kind
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=Path("build/project-browser-benchmark.json"))
    args = parser.parse_args()
    project = q.Project()
    doc = q.Document(q.Image.demo())
    doc.set_history_limits(0, 0)
    doc.execute([q.ObjectEdit.create(q.Rect(i % 1000, i // 1000, 1, 1)) for i in range(100000)])
    first = project.add_document(doc, "Large document")
    project.add_document(q.Document(q.Image.demo()), "Second image")
    # Reuse an immutable dense server for the remaining metadata/list entries.
    server = q.dense_image_server(q.Image(8, 8, [40, 80, 160]*64))
    for i in range(998):
        project.add_document(q.Document(server), f"Catalog image {i}")
    options = q.GuiOptions()
    options.project = project
    options.width, options.height = 2560, 1440
    view = q.Viewport()
    view.zoom, view.origin = 64, q.Point(-32000, -3000)
    options.start_view = view
    inputs = [replay(4, q.GuiInputKind.SELECT_ALL_KEY)]
    for i in range(20):
        inputs.append(replay(20+i*8, q.GuiInputKind.NEXT_IMAGE_KEY if i % 2 == 0 else q.GuiInputKind.PREVIOUS_IMAGE_KEY))
    options.inputs = inputs
    report = q.show(doc, frame_limit=200, options=options)
    assert (report.active_image, project.active_image, report.image_switches) == (first, first, 20)
    assert len(report.selected_ids) == 100000 and 0 < report.visible_objects < 2000
    assert report.final_zoom == view.zoom and not report.project_error and report.pending_tiles == 0
    assert project.view(first).zoom == view.zoom and project.view(first).center.x == report.final_center.x
    assert report.peak_gpu_bytes <= 64*1024*1024
    result = dict(platform=platform.platform(), processor=platform.processor(), catalog_images=1000,
                  objects=100000, selected_objects=len(report.selected_ids), visible_objects=report.visible_objects,
                  switches=report.image_switches, max_switch_ms=report.max_image_switch_ms,
                  cpu_frame_p95_ms=report.cpu_frame_p95_ms, peak_gpu_payload_bytes=report.peak_gpu_bytes,
                  pending_tiles=report.pending_tiles, gl_renderer=report.gl_renderer,
                  scope="1000-image catalog (998 small documents sharing one dense server), 20 switches between two dense images. First image retains 100000 selected rectangles in a narrow view. One GPU cache, reloaded on switch. Native camera/display view publication is enabled. Synchronous switch timing includes scheduler shutdown; excludes disk/vendor slow-reader and autosave workloads. CPU frame timing includes switch frames but excludes presentation wait.")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()

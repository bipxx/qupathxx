"""Measure confirmed removal of a selected 100000-object image and its graph."""
import argparse
import json
from pathlib import Path
import platform
import weakref

import qupathxx as q


class TrackedDocument(q.Document):
    pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=Path('build/project-remove-benchmark.json'))
    args = parser.parse_args()
    project = q.Project()
    doc = TrackedDocument(q.Image.demo())
    doc.set_history_limits(0, 0)
    doc.execute([q.ObjectEdit.create(q.Rect(i % 1000, i // 1000, 1, 1), f'Object {i}') for i in range(100000)])
    removed = project.add_document(doc, 'Remove 100000 selected objects')
    reference = weakref.ref(doc)
    surviving = project.add_document(q.Document(q.Image.demo()), 'Surviving image')
    for index in range(998):
        project.add_document(q.Document(q.Image(1, 1, [0, 0, 0])), f'Image {index}')
    view = q.ProjectView()
    view.center, view.zoom = q.Point(512, 64), 64
    project.set_view(removed, view)
    path = Path('build').resolve() / f'project-remove-{project.identity}.qpxproj'
    project.save(str(path))
    del doc
    try:
        assert reference() is not None
        before = path.read_bytes()
        options = q.GuiOptions()
        options.width, options.height, options.autosave_interval_seconds = 2560, 1440, 0
        inputs = []
        for frame, kind, target in [
            (4, q.GuiInputKind.SELECT_ALL_KEY, q.GuiInputTarget.CANVAS),
            (25, q.GuiInputKind.REMOVE_IMAGE_KEY, q.GuiInputTarget.CANVAS),
            (40, q.GuiInputKind.LEFT_DOWN, q.GuiInputTarget.CONFIRM_REMOVE_IMAGE),
            (41, q.GuiInputKind.LEFT_UP, q.GuiInputTarget.CONFIRM_REMOVE_IMAGE),
        ]:
            item = q.GuiInput()
            item.frame, item.kind, item.target = frame, kind, target
            inputs.append(item)
        options.inputs = inputs
        report = q.show(project, frame_limit=120, options=options)
        assert not report.project_error and report.images_removed == 1 and report.active_image == surviving
        assert reference() is None and project.dirty and len(project.images) == 999
        assert report.selected_ids == [] and report.visible_objects == 0 and path.read_bytes() == before
        result = dict(platform=platform.platform(), processor=platform.processor(),
                      catalog_images_before=1000, catalog_images_after=999, removed_objects=100000,
                      removal_and_binding_ms=report.max_image_switch_ms,
                      cpu_frame_p95_ms=report.cpu_frame_p95_ms, max_cpu_frame_ms=report.max_cpu_frame_ms,
                      peak_gpu_payload_bytes=report.peak_gpu_bytes, python_document_released=reference() is None,
                      gl_renderer=report.gl_renderer,
                      scope='Local dense RGB, 100000 selected rectangles in removed image, 1000-entry catalog; no external document owner. Includes owner-thread model/selection/tile retirement and survivor binding; input construction and file creation excluded. No cold/network/vendor or prolonged-pressure claim.')
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2)+'\n')
        print(json.dumps(result, indent=2))
    finally:
        path.unlink(missing_ok=True)


if __name__ == '__main__':
    main()

"""Measure identity discovery and moved recovery with 1000 unrelated checkpoints."""
import argparse
import json
from pathlib import Path
import platform
import shutil
import time

import qupathxx as q


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--gui', action='store_true')
    parser.add_argument('--output', type=Path, default=Path('build/recovery-association-benchmark.json'))
    args = parser.parse_args()
    source = q.Project()
    root = Path('build').resolve()/f'association-{source.identity}'
    old, new = root/'old', root/'moved'
    old.mkdir(parents=True)
    doc = q.Document(q.Image.demo())
    doc.set_history_limits(0, 0)
    doc.execute([q.ObjectEdit.create(q.Rect(i % 1000, i // 1000, 1, 1), f'Object {i}') for i in range(100000)])
    image = source.add_document(doc)
    view = q.ProjectView()
    view.center, view.zoom = q.Point(512, 64), 64
    source.set_view(image, view)
    original = old/'study.qpxproj'
    source.save(str(original))
    doc.rename_annotation(1, 'Captured moved work')
    writer = q.ProjectAutosave()
    checkpoint = Path(writer.default_path(source))
    assert writer.request(source) and writer.wait().state == 'ready'
    assert old.parent == root and new.parent == root
    old.rename(new)
    primary = new/'renamed.qpxproj'
    (new/original.name).rename(primary)
    checkpoint = new/checkpoint.name
    del source, doc
    foreign = q.Project()
    foreign_primary = root/'foreign.qpxproj'
    foreign.save(str(foreign_primary))
    unused = foreign.add_document(q.Document(q.Image(1, 1, [0, 0, 0])))
    foreign.remove_image(unused)
    template = root/'foreign.qpxrecover'
    foreign_writer = q.ProjectAutosave()
    assert foreign_writer.request(foreign, str(template)) and foreign_writer.wait().state == 'ready'
    copies = [new/f'unrelated-{index:04}.qpxrecover' for index in range(1000)]
    try:
        for path in copies:
            shutil.copyfile(template, path)
        before = checkpoint.read_bytes()
        start = time.perf_counter()
        candidates = q.Project.find_recoveries(str(primary))
        elapsed = (time.perf_counter()-start)*1000
        assert len(candidates) == 1 and candidates[0].path == str(checkpoint)
        assert candidates[0].associated_base_path == str(primary) and candidates[0].base_status == 'relocated'
        result = dict(platform=platform.platform(), processor=platform.processor(), objects=100000,
                      directory_checkpoints=1001, unrelated_checkpoints=1000, discovery_ms=elapsed,
                      matching_checkpoints=len(candidates), result_limit=64)
        if args.gui:
            live = q.Project.open(str(primary))
            options = q.GuiOptions()
            options.autosave_interval_seconds, options.width, options.height = 0, 2560, 1440
            inputs = []
            for frame, kind in [(4, q.GuiInputKind.SELECT_ALL_KEY), (20, q.GuiInputKind.FIND_RECOVERIES_KEY),
                                (120, q.GuiInputKind.RECOVER_KEY), (155, q.GuiInputKind.SELECT_ALL_KEY),
                                (170, q.GuiInputKind.SAVE_KEY)]:
                item = q.GuiInput()
                item.frame, item.kind = frame, kind
                inputs.append(item)
            options.inputs = inputs
            report = q.show(live, frame_limit=240, options=options)
            assert not report.project_error and report.project_replacements == report.saves_completed == report.retired_workspaces == 1
            assert len(report.selected_ids) == 100000 and 0 < report.visible_objects < 2000
            assert live.path == str(primary) and live.generation == 2 and not live.dirty
            result.update(worker_recovery_load_ms=report.recovery_load_ms, cpu_frame_p95_ms=report.cpu_frame_p95_ms,
                          max_cpu_frame_ms=report.max_cpu_frame_ms, peak_gpu_payload_bytes=report.peak_gpu_bytes,
                          selected_objects=len(report.selected_ids), visible_objects=report.visible_objects,
                          retired_workspaces=report.retired_workspaces, worker_retirement_ms=report.max_retirement_ms,
                          gl_renderer=report.gl_renderer)
        else:
            start = time.perf_counter()
            restored = q.Project.recover(str(checkpoint), base_path=str(primary))
            result['associated_load_ms'] = (time.perf_counter()-start)*1000
            assert restored.path == str(primary) and restored.generation == 1 and restored.dirty
            assert restored.document(image).annotation(1).name == 'Captured moved work'
            restored.save(str(primary))
        assert not original.exists() and checkpoint.read_bytes() == before
        loaded = q.Project.open(str(primary))
        assert loaded.generation == 2 and loaded.document(image).annotation(1).name == 'Captured moved work'
        result['scope'] = 'Warm local SQLite: 100000-object moved/renamed project and checkpoint, 1000 unrelated empty checkpoints. Metadata-only identity discovery caches primary identity/generation, bounded result heap; graph/file construction excluded. Desktop includes background scan/load, native background graph retirement and explicit save; buffer-swap wait excluded. Cold/network/vendor and prolonged pressure unverified.'
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2)+'\n')
        print(json.dumps(result, indent=2))
    finally:
        # Known files only, followed by empty-directory removal; no recursion.
        for path in copies+[primary, checkpoint, Path(str(checkpoint)+'.lock'), foreign_primary, template, Path(str(template)+'.lock')]:
            path.unlink(missing_ok=True)
        new.rmdir()
        root.rmdir()


if __name__ == '__main__':
    main()

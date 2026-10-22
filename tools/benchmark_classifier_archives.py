"""Installed classifier provenance pipeline; synthetic fixture, no biological claim."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import tempfile
import time
import qupathxx as q
from qupathxx import _native


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--rows', type=int, default=10000)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    if not 500 <= args.rows <= 100000: ap.error('--rows must be 500..100000')
    if not q.has_project or not q.has_classifier_training: ap.error('requires project storage and classifier training')
    doc = q.Document(q.Image(64, 64, [80]*64*64*3))
    doc.set_history_limits(0, 0)
    started = time.perf_counter()
    ids = doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(1, 1, 2+i % 20, 2+i % 20)),
                f'Cell {i}', class_name=('small' if 2+i % 20 < 10 else 'large') if i < 400 else '',
                kind=q.ObjectKind.CELL) for i in range(args.rows)])
    prepare_ms = (time.perf_counter()-started)*1000
    doc.set_history_limits(128, 64*1024*1024)
    options = q.ObjectFeatureOptions(); options.snapshot_bytes = 128*1024*1024
    parameters = q.ForestParameters(); parameters.trees, parameters.max_depth = 32, 8
    job = q.ObjectClassifierJob()
    started = time.perf_counter()
    assert job.train(q.ObjectFeatureRequest(doc, ids[:400], options), parameters)
    status = job.wait(doc); assert status.state == 'ready', status.error
    training_job_ms = (time.perf_counter()-started)*1000
    started = time.perf_counter(); training_id = job.retain_training(doc)
    retain_training_ms = (time.perf_counter()-started)*1000
    model_sha256 = job.model.sha256
    started = time.perf_counter(); request = q.ObjectFeatureRequest(doc, ids[400:], options)
    capture_ms = (time.perf_counter()-started)*1000
    started = time.perf_counter(); assert job.predict(request, job.model)
    status = job.wait(doc); assert status.state == 'ready', status.error
    prediction_archive_ms = (time.perf_counter()-started)*1000
    table_sha256 = job.table.sha256
    started = time.perf_counter(); changed = job.apply(doc)
    publication_ms = (time.perf_counter()-started)*1000
    assert changed == ids[400:]
    prediction = doc.classifier_records[-1]
    archive_sha256 = prediction.archive.sha256
    history_bytes = doc.history_bytes
    project = q.Project(); image = project.add_document(doc)
    with tempfile.TemporaryDirectory(prefix='qpx-classifier-archive-') as directory:
        path = Path(directory)/'archive.qpxproj'; save = q.ProjectSaveJob()
        started = time.perf_counter(); assert save.request(project, str(path))
        save_capture_wall_ms = (time.perf_counter()-started)*1000
        status = save.wait(); assert status.state == 'saved', status.error
        save_metrics = dict(capture_wall_ms=save_capture_wall_ms, capture_native_ms=status.capture_ms,
                            write_ms=status.write_ms, snapshot_bytes=status.snapshot_bytes, file_bytes=path.stat().st_size)
        started = time.perf_counter(); reopened = q.Project.open(str(path)).document(image)
        reopen_ms = (time.perf_counter()-started)*1000
        assert reopened.classifier_record(training_id).archive.model.sha256 == model_sha256
        assert reopened.classifier_record(prediction.id).archive.sha256 == archive_sha256
        assert reopened.classifier_record(prediction.id).changed_objects == changed
    started = time.perf_counter(); assert doc.undo(); undo_ms = (time.perf_counter()-started)*1000
    assert all(doc.annotation(i).class_name == '' for i in ids[400:])
    assert doc.classifier_record_count == 2
    started = time.perf_counter(); assert doc.redo(); redo_ms = (time.perf_counter()-started)*1000
    assert all(doc.annotation(ids[i]).class_name == ('small' if 2+i % 20 < 10 else 'large') for i in range(400, args.rows))
    env = q.build_environment()
    report = dict(schema='qpx.classifier-archive-benchmark/1', rows=args.rows, prediction_rows=len(changed),
        prepare_ms=prepare_ms, training_job_ms=training_job_ms, retain_training_ms=retain_training_ms,
        capture_wall_ms=capture_ms, capture_native_ms=request.capture_ms,
        prediction_and_archive_ms=prediction_archive_ms, publication_ms=publication_ms,
        undo_ms=undo_ms, redo_ms=redo_ms, history_bytes=history_bytes,
        classifier_records=doc.classifier_record_count,
        archive_payload_bytes=sum(r.payload_bytes for r in doc.classifier_records),
        save=save_metrics, reopen_ms=reopen_ms, model_sha256=model_sha256, table_sha256=table_sha256,
        prediction_archive_sha256=archive_sha256,
        native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(), platform=platform.platform(),
        environment={name:getattr(env, name) for name in ['app_version','source_tree_sha256','git_revision','compiler','compiler_flags','configuration','platform']},
        scope='One fresh process; one call per phase. Overlapping synthetic rectangles with repeated sizes, 400 training labels and 32 trees/depth 8. Prediction includes feature extraction, inference and archive preparation on worker. Publication includes native class edit, immutable audit insertion and Python changed-ID conversion. Save capture shares immutable artifacts; writer persists objects and archives. Default 64 MiB history, no image reads or biological accuracy evidence. Shared workstation; no hardware/frame-budget acceptance.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2)+'\n', encoding='utf8')
    print(json.dumps({key:report[key] for key in ['rows','prediction_and_archive_ms','publication_ms','archive_payload_bytes']}))


if __name__ == '__main__': main()

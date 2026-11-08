"""Thin-control integration against the installed native workflow services."""
import csv
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import signal
import subprocess
import sys
import threading

import pytest
import qupathxx as q

SCRIPT = Path(__file__).resolve().parents[1] / 'examples/project_density.py'
spec = importlib.util.spec_from_file_location('project_density_runner', SCRIPT)
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


NATIVE_COLUMNS = (
    'window_id', 'window_revision', 'count', 'area_pixels', 'area_mm2',
    'density_per_mm2', 'status', 'units', 'cell_count', 'tissue_count',
    'exclusion_count', 'algorithm', 'geometry_policy', 'footprint_sha256',
    'input_sha256', 'result_sha256', 'pixel_width_um', 'pixel_height_um',
    'prefer_nucleus', 'polygonize_curves',
)
CONTEXT_COLUMNS = ('project_id', 'image_id', 'image_name', 'record_id', 'reuse',
                   'request_input_sha256', 'checkpoint')


def csv_line(values):
    stream = io.StringIO(newline='')
    csv.writer(stream, lineterminator='\n').writerow(values)
    return stream.getvalue().encode('utf-8')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_csv(report):
    with Path(report['csv_path']).open(encoding='utf-8', newline='') as stream:
        return list(csv.DictReader(stream))


def scene(root, count=2, retained=False):
    project = q.Project()
    selections = []
    for i in range(count):
        doc = q.Document(q.Image(24, 24, [80 + i] * 24 * 24 * 3))
        doc.set_calibration(2 if i == 0 else .5, .5 if i == 0 else .25)
        windows = [doc.add_rectangle(q.Rect(0, 0, 12, 12)), doc.add_rectangle(q.Rect(13, 13, 2, 2))]
        tissue = [doc.add_rectangle(q.Rect(0, 0, 10, 10))]
        excluded = [doc.add_rectangle(q.Rect(4, 0, 2, 10))]
        cells = [doc.add_cell(q.Roi.rectangle(q.Rect(x - 1, x - 1, 2, 2))) for x in (2, 5, 8)]
        image = project.add_document(doc, f'Case {i}, "quoted"\n肿瘤')
        project.set_image_metadata(image, {'case': str(i)})
        item = dict(image_id=image, windows=windows, tissue=tissue, excluded=excluded, cells=cells)
        if retained:
            request = q.SpatialDensityRequest(doc, windows, tissue, excluded, cells)
            item['record_id'] = doc.keep_density(request, q.measure_spatial_density(request))
        selections.append(item)
    source = root / 'source.qpxproj'
    project.save(str(source))
    manifest = root / 'selections.json'
    manifest.write_text(json.dumps(dict(schema='qpx.project-density/1', project=source.name,
                                        images=list(reversed(selections)))), encoding='utf-8')
    return project, source, manifest, selections


def change_manifest(path, fn):
    value = json.loads(path.read_text(encoding='utf-8'))
    fn(value)
    path.write_text(json.dumps(value), encoding='utf-8')


def no_job(monkeypatch):
    actual = q.ProjectDensityJob
    class ObservedJob:
        def __init__(self): self.native = actual()
        def poll(self):
            self.native.poll()
            assert self.native.progress.kernel_calls == 0, 'reuse invoked the native kernel'
        def __getattr__(self, key): return getattr(self.native, key)
    monkeypatch.setattr(q, 'ProjectDensityJob', ObservedJob)


def test_combined_rows_checkpoints_and_native_identity(tmp_path):
    project, source, manifest, selections = scene(tmp_path)
    before = sha(source), sha(manifest)
    report = runner.run(manifest, tmp_path / 'run')
    assert report['state'] == 'completed' and report['computed'] == 2 and report['reused'] == report['failed'] == 0
    rows = read_csv(report)
    assert len(rows) == report['rows'] == 4
    assert [int(r['image_id']) for r in rows] == [1, 1, 2, 2]
    assert [int(r['count']) for r in rows] == [2, 0, 2, 0]
    assert [r['status'] for r in rows] == ['valid', 'empty_area'] * 2
    assert rows[0]['image_name'] == 'Case 0, "quoted"\n肿瘤'
    assert float(rows[0]['area_mm2']) == pytest.approx(.00008)
    assert float(rows[2]['area_mm2']) == pytest.approx(.00001)
    assert rows[1]['density_per_mm2'] == ''
    assert (sha(source), sha(manifest)) == before and not project.dirty
    for selected, entry in zip(selections, report['images']):
        doc = project.document(selected['image_id'])
        expected = q.measure_spatial_density(q.SpatialDensityRequest(doc, selected['windows'], selected['tissue'], selected['excluded'], selected['cells']))
        assert entry['result_sha256'] == expected.sha256
        saved = q.Project.open(entry['checkpoint'])
        local = saved.document(entry['checkpoint_image_id'])
        assert saved.image_metadata(entry['checkpoint_image_id']) == {'case': str(selected['image_id'] - 1)}
        assert local.density_record(entry['record_id']).result.sha256 == expected.sha256
        replay = q.SpatialDensityRequest.replay(local, entry['record_id'])
        assert local.verify_density(entry['record_id'], replay, q.measure_spatial_density(replay))
        native = list(csv.DictReader(io.StringIO(q.spatial_density_csv(expected))))
        actual = [r for r in rows if int(r['image_id']) == selected['image_id']]
        assert [{k: r[k] for k in NATIVE_COLUMNS} for r in actual] == native


def test_resume_skips_kernel_and_does_not_copy_projects(tmp_path, monkeypatch):
    _, _, manifest, _ = scene(tmp_path)
    first = runner.run(manifest, tmp_path / 'first')
    no_job(monkeypatch)
    resumed = runner.run(manifest, tmp_path / 'second', resume_from=tmp_path / 'first')
    assert resumed['state'] == 'completed' and resumed['reused'] == 2 and resumed['computed'] == 0
    assert not list((tmp_path / 'second').glob('*.qpxproj'))
    assert [i['checkpoint'] for i in resumed['images']] == [i['checkpoint'] for i in first['images']]
    assert [r['result_sha256'] for r in read_csv(resumed)] == [r['result_sha256'] for r in read_csv(first)]


def test_unrelated_edit_preserves_reuse(tmp_path, monkeypatch):
    project, source, manifest, _ = scene(tmp_path)
    runner.run(manifest, tmp_path / 'first')
    project.document(1).add_rectangle(q.Rect(20, 20, 2, 2))
    project.save(str(source))
    no_job(monkeypatch)
    result = runner.run(manifest, tmp_path / 'second', resume_from=tmp_path / 'first')
    assert result['state'] == 'completed' and result['reused'] == 2


def test_changed_geometry_recomputes_only_affected_image(tmp_path):
    project, source, manifest, selections = scene(tmp_path)
    runner.run(manifest, tmp_path / 'first')
    project.document(1).update_rectangle(selections[0]['tissue'][0], q.Rect(0, 0, 8, 8))
    project.save(str(source))
    result = runner.run(manifest, tmp_path / 'second', resume_from=tmp_path / 'first')
    assert result['state'] == 'completed' and result['computed'] == result['reused'] == 1
    assert result['images'][0]['reuse'] == 'computed' and result['images'][1]['reuse'] == 'checkpoint'
    assert float(read_csv(result)[0]['area_pixels']) == 48


def test_existing_record_hint_reuses_native_replay(tmp_path, monkeypatch):
    _, _, manifest, _ = scene(tmp_path, retained=True)
    no_job(monkeypatch)
    report = runner.run(manifest, tmp_path / 'run')
    assert report['state'] == 'completed' and report['reused'] == 2
    assert all(i['reuse'] == 'source_record' for i in report['images'])
    assert all(i['request_input_sha256'] != i['result_input_sha256'] for i in report['images'])


@pytest.mark.parametrize('kind', ['recipe', 'engine', 'force', 'smaller_budget'])
def test_recompute_conditions(tmp_path, kind):
    _, _, manifest, _ = scene(tmp_path)
    runner.run(manifest, tmp_path / 'first')
    if kind == 'recipe':
        change_manifest(manifest, lambda v: v.update(options={'prefer_nucleus': False}))
    if kind == 'smaller_budget':
        change_manifest(manifest, lambda v: v.update(options={'work_bytes': 64 * runner.MIB}))
    if kind == 'engine':
        receipt = tmp_path / 'first/image-1.json'
        value = json.loads(receipt.read_text())
        value['engine']['source_tree_sha256'] = '0' * 64
        receipt.write_text(json.dumps(value))
    result = runner.run(manifest, tmp_path / 'second', resume_from=tmp_path / 'first', recompute=kind == 'force')
    assert result['state'] == 'completed'
    assert result['computed'] == (1 if kind == 'engine' else 2)


def test_corrupt_checkpoint_is_preserved_and_fails_explicitly(tmp_path):
    _, _, manifest, _ = scene(tmp_path)
    first = runner.run(manifest, tmp_path / 'first')
    checkpoint = Path(first['images'][0]['checkpoint'])
    checkpoint.write_bytes(checkpoint.read_bytes() + b'changed')
    damaged = checkpoint.read_bytes()
    result = runner.run(manifest, tmp_path / 'second', resume_from=tmp_path / 'first')
    assert result['state'] == 'partial' and result['failed'] == result['reused'] == 1
    assert 'checksum' in result['images'][0]['error'] and checkpoint.read_bytes() == damaged
    assert {r['image_id'] for r in read_csv(result)} == {'2'}


@pytest.mark.parametrize('stop_on_error', [False, True])
def test_per_image_errors_do_not_become_zero_rows(tmp_path, stop_on_error):
    _, _, manifest, selections = scene(tmp_path)
    def invalid(v):
        next(i for i in v['images'] if i['image_id'] == 1)['cells'] = selections[0]['windows']
    change_manifest(manifest, invalid)
    result = runner.run(manifest, tmp_path / 'run', stop_on_error=stop_on_error)
    assert result['state'] == 'partial' and result['failed'] == 1
    assert result['not_run'] == (1 if stop_on_error else 0)
    assert all(r['image_id'] == '2' for r in read_csv(result))
    assert not (tmp_path / 'run/density.csv').exists()


def test_cancel_after_image_and_resume(tmp_path):
    _, _, manifest, _ = scene(tmp_path)
    stop = threading.Event()
    report = runner.run(manifest, tmp_path / 'first', cancelled=stop, on_image=lambda _: stop.set())
    assert report['state'] == 'cancelled' and report['computed'] == 1 and report['not_run'] == 1
    assert len(read_csv(report)) == 2 and Path(report['csv_path']).name == 'density.partial.csv'
    resumed = runner.run(manifest, tmp_path / 'second', resume_from=tmp_path / 'first')
    assert resumed['state'] == 'completed' and resumed['computed'] == resumed['reused'] == 1


def test_active_native_job_receives_cancellation(tmp_path, monkeypatch):
    _, _, manifest, _ = scene(tmp_path)
    actual = q.ProjectDensityJob
    stop, calls = threading.Event(), []
    class Job:
        def __init__(self): self.native = actual()
        def cancel(self):
            calls.append('cancel')
            self.native.cancel()
        def __getattr__(self, key): return getattr(self.native, key)
    monkeypatch.setattr(q, 'ProjectDensityJob', Job)
    report = runner.run(manifest, tmp_path / 'run', cancelled=stop, on_image=lambda _: stop.set())
    assert calls and report['state'] == 'cancelled'
    assert report['computed'] == 1 and report['not_run'] == 1
    assert len(list((tmp_path / 'run').glob('*.qpxproj'))) == 1


def test_csv_budget_rolls_back_whole_image_and_keeps_reusable_checkpoint(tmp_path, monkeypatch):
    _, _, manifest, _ = scene(tmp_path)
    baseline = runner.run(manifest, tmp_path / 'baseline')
    rows = read_csv(baseline)
    future = tmp_path / 'limited'
    columns = CONTEXT_COLUMNS + NATIVE_COLUMNS
    for row in rows:
        row['checkpoint'] = str(future / Path(row['checkpoint']).name)
    limit = len(csv_line(list(columns)))
    limit += sum(len(csv_line([row[c] for c in columns])) for row in rows[:3]) + 10
    limited = runner.run(manifest, future, csv_bytes=limit)
    assert limited['state'] == 'partial' and limited['computed'] == limited['failed'] == 1
    assert len(read_csv(limited)) == 2 and (future / 'image-2.json').exists()
    no_job(monkeypatch)
    resumed = runner.run(manifest, tmp_path / 'resumed', resume_from=future)
    assert resumed['state'] == 'completed' and resumed['reused'] == 2


def test_input_change_refuses_complete_publication(tmp_path):
    _, _, manifest, _ = scene(tmp_path)
    def changed(_): manifest.write_text(manifest.read_text() + ' ')
    result = runner.run(manifest, tmp_path / 'run', on_image=changed)
    assert result['state'] == 'partial' and 'input changed' in result['error']
    assert not (tmp_path / 'run/density.csv').exists()


@pytest.mark.parametrize('error', ['duplicate_image', 'unknown', 'missing_cells', 'duplicate_ids', 'bool_id', 'bad_option'])
def test_manifest_refusals_are_setup_errors(tmp_path, error):
    _, _, manifest, _ = scene(tmp_path)
    def invalid(v):
        if error == 'duplicate_image': v['images'].append(v['images'][0])
        elif error == 'unknown': v['unexpected'] = True
        elif error == 'missing_cells': del v['images'][0]['cells']
        elif error == 'duplicate_ids': v['images'][0]['windows'] *= 2
        elif error == 'bool_id': v['images'][0]['image_id'] = True
        else: v['options'] = {'prefer_nucleus': 'yes'}
    change_manifest(manifest, invalid)
    with pytest.raises(ValueError): runner.run(manifest, tmp_path / 'run')
    assert not (tmp_path / 'run').exists()


def test_input_output_and_sidecar_protection(tmp_path):
    _, source, manifest, _ = scene(tmp_path)
    original = sha(source)
    with pytest.raises(FileExistsError): runner.run(manifest, tmp_path)
    with pytest.raises(runner.CsvBudget): runner.run(manifest, tmp_path / 'tiny', csv_bytes=1)
    assert not (tmp_path / 'tiny').exists()
    sidecar = Path(str(source) + '-journal')
    sidecar.write_bytes(b'preserve journal')
    with pytest.raises(ValueError, match='sidecar'): runner.run(manifest, tmp_path / 'run')
    assert sha(source) == original and sidecar.read_bytes() == b'preserve journal'


def test_cli_real_multi_image_run_and_resume(tmp_path):
    _, _, manifest, _ = scene(tmp_path)
    for name, extra, expected in [('first', [], 0), ('second', ['--resume-from', str(tmp_path / 'first')], 2)]:
        done = subprocess.run([sys.executable, '-I', str(SCRIPT), str(manifest), str(tmp_path / name), *extra], capture_output=True, text=True, timeout=30)
        assert done.returncode == 0, done.stdout + done.stderr
        result = json.loads(done.stdout)
        assert result['state'] == 'completed' and result['reused'] == expected and result['rows'] == 4


def test_cli_sigint_cancels_native_and_restores_handler(tmp_path, monkeypatch, capsys):
    _, _, manifest, _ = scene(tmp_path)
    actual = q.ProjectDensityJob
    previous = signal.getsignal(signal.SIGINT)
    class Job:
        def __init__(self): self.native = actual()
        def run(self, loaded, output, options):
            options.pause_after_image = True
            return self.native.run(loaded, output, options)
        def take_images(self):
            events = self.native.take_images()
            if events:
                signal.raise_signal(signal.SIGINT)
            return events
        def __getattr__(self, name): return getattr(self.native, name)
    monkeypatch.setattr(q, 'ProjectDensityJob', Job)
    assert runner.main([str(manifest), str(tmp_path / 'run')]) == 130
    assert json.loads(capsys.readouterr().out)['state'] == 'cancelled'
    assert signal.getsignal(signal.SIGINT) == previous


def test_resume_works_without_final_report(tmp_path, monkeypatch):
    _, _, manifest, _ = scene(tmp_path)
    runner.run(manifest, tmp_path / 'first')
    (tmp_path / 'first/report.json').unlink()  # Simulate interruption before final run publication.
    no_job(monkeypatch)
    result = runner.run(manifest, tmp_path / 'second', resume_from=tmp_path / 'first')
    assert result['state'] == 'completed' and result['reused'] == 2


def test_new_byte_limits_do_not_publish_fake_results(tmp_path):
    _, source, manifest, _ = scene(tmp_path)
    original = sha(source)
    with pytest.raises(ValueError, match='input exceeds'):
        runner.run(manifest, tmp_path / 'too-large', project_bytes=1)
    result = runner.run(manifest, tmp_path / 'no-checkpoint', checkpoint_bytes=1)
    assert result['state'] == 'partial' and result['failed'] == 2 and not read_csv(result)
    assert sha(source) == original and not list((tmp_path / 'no-checkpoint').glob('*.qpxproj'))


def test_missing_linked_image_stays_unopened(tmp_path):
    project = q.Project()
    doc = q.Document(q.Image.demo())
    doc.set_calibration(1, 1)
    roi = doc.add_rectangle(q.Rect(0, 0, 10, 10))
    cell = doc.add_cell(q.Roi.rectangle(q.Rect(1, 1, 2, 2)))
    absent = tmp_path / 'absent.ppm'
    image = project.add_document(doc, 'Missing linked source', str(absent), 'ppm')
    source = tmp_path / 'source.qpxproj'
    project.save(str(source))
    manifest = tmp_path / 'selections.json'
    manifest.write_text(json.dumps(dict(schema='qpx.project-density/1', project=source.name, images=[
        dict(image_id=image, windows=[roi], tissue=[roi], excluded=[], cells=[cell])])), encoding='utf-8')
    result = runner.run(manifest, tmp_path / 'run')
    assert result['state'] == 'completed' and not absent.exists()
    assert read_csv(result)[0]['count'] == '1'


def test_missing_linked_source_under_output_is_protected(tmp_path):
    project = q.Project()
    doc = q.Document(q.Image.demo())
    doc.set_calibration(1, 1)
    roi = doc.add_rectangle(q.Rect(0, 0, 10, 10))
    output = tmp_path / 'run'
    image = project.add_document(doc, 'Protected missing source', str(output / 'absent.ppm'), 'ppm')
    source = tmp_path / 'source.qpxproj'
    project.save(str(source))
    manifest = tmp_path / 'selections.json'
    manifest.write_text(json.dumps(dict(schema='qpx.project-density/1', project=source.name, images=[
        dict(image_id=image, windows=[roi], tissue=[roi], excluded=[], cells=[])])), encoding='utf-8')
    with pytest.raises(ValueError, match='protected'):
        runner.run(manifest, output)
    assert not output.exists()


@pytest.mark.parametrize('message', ['callback failed', ''])
def test_callback_failure_is_a_native_partial_run(tmp_path, message):
    _, _, manifest, _ = scene(tmp_path)
    def failed(_):
        raise RuntimeError(message)
    result = runner.run(manifest, tmp_path / 'run', on_image=failed)
    assert result['state'] == 'partial' and result['error']
    assert result['computed'] == 1 and result['not_run'] == 1
    assert len(read_csv(result)) == 2


def test_setup_cancellation_exit_status_restores_handler(tmp_path, monkeypatch, capsys):
    previous = signal.getsignal(signal.SIGINT)
    def cancelled(*args, **kwargs):
        raise q.WorkflowCancelled('cancelled during setup')
    monkeypatch.setattr(runner, 'run', cancelled)
    assert runner.main([str(tmp_path / 'manifest.json'), str(tmp_path / 'run')]) == 130
    assert json.loads(capsys.readouterr().out)['state'] == 'cancelled'
    assert signal.getsignal(signal.SIGINT) == previous

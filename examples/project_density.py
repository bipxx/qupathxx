"""Project-wide density reports using the installed qupathxx candidate4 API.

python -I project_density.py selections.json new-output [--resume-from prior-output]
No native rebuild, segmentation or UI extension is required.
"""
from __future__ import annotations

import argparse
import copy
import csv
import hashlib
import io
import json
import os
from pathlib import Path
import signal
import threading
from typing import Any, Callable

import qupathxx as q

SCHEMA = 'qpx.project-density/1'
REPORT_SCHEMA = 'qpx.project-density-report/1'
CHECKPOINT_SCHEMA = 'qpx.project-density-checkpoint/1'
ALGORITHM = 'qpx.roi-tissue-density/1'
POLICY = 'qpx.polygon512-closed-exclusion/1'
MIB = 1024 * 1024
NATIVE_COLUMNS = (
    'window_id', 'window_revision', 'count', 'area_pixels', 'area_mm2',
    'density_per_mm2', 'status', 'units', 'cell_count', 'tissue_count',
    'exclusion_count', 'algorithm', 'geometry_policy', 'footprint_sha256',
    'input_sha256', 'result_sha256', 'pixel_width_um', 'pixel_height_um',
    'prefer_nucleus', 'polygonize_curves',
)
CONTEXT_COLUMNS = ('project_id', 'image_id', 'image_name', 'record_id', 'reuse',
                   'request_input_sha256', 'checkpoint')


class Cancelled(Exception):
    pass


class CsvBudget(ValueError):
    pass


class InputChanged(RuntimeError):
    pass


def _unique(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f'duplicate JSON key: {key}')
        result[key] = value
    return result


def _integer(value: Any, name: str, maximum: int = (1 << 64) - 1) -> int:
    if type(value) is not int or not 0 < value <= maximum:
        raise ValueError(f'{name} requires a positive integer <= {maximum}')
    return value


class FileGuard:
    """Hash once; use cheap stat checks between images, rehash at final publish."""
    def __init__(self, path: Path, limit: int, *, project: bool = False):
        self.path = path.resolve(strict=True)
        self.project = project
        self.version = self._stat()
        if self.version[2] > limit:
            raise ValueError(f'input exceeds {limit} bytes: {self.path}')
        self.sha256 = self._hash()
        self.check()

    def _stat(self) -> tuple[int, ...]:
        if self.project and any(Path(str(self.path) + suffix).exists() for suffix in ('-journal', '-wal', '-shm')):
            raise ValueError(f'close/recover the project before running; SQLite sidecar present: {self.path}')
        s = self.path.stat()
        if not self.path.is_file():
            raise ValueError(f'input is not a file: {self.path}')
        return s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns, s.st_ctime_ns

    def _hash(self) -> str:
        with self.path.open('rb') as stream:
            return hashlib.file_digest(stream, 'sha256').hexdigest()

    def check(self, rehash: bool = False) -> None:
        try:
            if self._stat() != self.version or (rehash and self._hash() != self.sha256):
                raise InputChanged(f'input changed during run: {self.path}')
        except (OSError, ValueError) as error:
            raise InputChanged(f'input became unavailable during run: {self.path}') from error


def _read_json(path: Path, limit: int) -> dict[str, Any]:
    if path.stat().st_size > limit:
        raise ValueError(f'JSON exceeds {limit} bytes: {path}')
    with path.open('rb') as stream:
        data = stream.read(limit + 1)
    if len(data) > limit:
        raise ValueError('JSON exceeds byte limit')
    def invalid(value: str) -> None:
        raise ValueError(f'nonfinite JSON number: {value}')
    result = json.loads(data.decode('utf-8'), object_pairs_hook=_unique,
                        parse_constant=invalid)
    if not isinstance(result, dict):
        raise ValueError('JSON root must be an object')
    return result


def load_manifest(path: Path) -> tuple[FileGuard, Path, list[dict[str, Any]], Any]:
    guard = FileGuard(path, 16 * MIB)
    value = _read_json(guard.path, 16 * MIB)
    if set(value) - {'schema', 'project', 'images', 'options'} or value.get('schema') != SCHEMA:
        raise ValueError(f'expected {SCHEMA}, with schema/project/images/options fields')
    if not isinstance(value.get('project'), str) or not value['project']:
        raise ValueError('project must be a nonempty path relative to the manifest')
    images = value.get('images')
    if not isinstance(images, list) or not 1 <= len(images) <= 1000:
        raise ValueError('images requires 1..1000 explicit selections')
    selections = []
    seen = set()
    for item in images:
        if not isinstance(item, dict) or set(item) - {'image_id', 'windows', 'tissue', 'excluded', 'cells', 'record_id'}:
            raise ValueError('unknown image selection field')
        image = _integer(item.get('image_id'), 'image_id')
        if image in seen:
            raise ValueError('duplicate image_id')
        seen.add(image)
        selected: dict[str, Any] = {'image_id': image}
        for role in ('windows', 'tissue', 'excluded', 'cells'):
            ids = item.get(role)
            cap = 100000 if role == 'cells' else 10000
            if not isinstance(ids, list) or len(ids) > cap or (role in ('windows', 'tissue') and not ids):
                raise ValueError(f'{role} requires an explicit ID list, up to {cap}')
            canonical = sorted(_integer(i, role) for i in ids)
            if len(set(canonical)) != len(canonical):
                raise ValueError(f'duplicate {role} ID')
            selected[role] = canonical
        if 'record_id' in item:
            selected['record_id'] = _integer(item['record_id'], 'record_id')
        selections.append(selected)
    options = q.SpatialDensityOptions()
    supplied = value.get('options', {})
    if not isinstance(supplied, dict) or set(supplied) - {
        'prefer_nucleus', 'polygonize_curves', 'snapshot_bytes', 'work_bytes',
        'max_membership_tests', 'max_geometry_operations', 'max_geometry_vertices',
    }:
        raise ValueError('unknown density options')
    for key, entry in supplied.items():
        if key in ('prefer_nucleus', 'polygonize_curves'):
            if type(entry) is not bool:
                raise ValueError(f'{key} requires a boolean')
        else:
            _integer(entry, key)
        setattr(options, key, entry)
    options.validate()
    guard.check()
    return guard, (guard.path.parent / value['project']).resolve(), sorted(selections, key=lambda i: i['image_id']), options


def _engine(environment: Any) -> dict[str, str]:
    return {name: getattr(environment, name) for name in (
        'source_tree_sha256', 'compiler', 'compiler_flags', 'configuration',
        'platform', 'geos_version',
    )}


def _same_recipe(request: Any, result: Any) -> bool:
    """Compare authoritative captured values; do not recalculate any geometry."""
    if result.algorithm != ALGORITHM or result.geometry_policy != POLICY:
        return False
    for field in ('x_scale', 'y_scale'):
        if getattr(request, field) != getattr(result, field):
            return False
    for field in ('width', 'height'):
        if getattr(request.provenance.source, field) != getattr(result.provenance.source, field):
            return False
    for field in ('prefer_nucleus', 'polygonize_curves'):
        if getattr(request.options, field) != getattr(result.options, field):
            return False
    # A smaller limit needs fresh native admission, not a second Python budget engine.
    for field in ('snapshot_bytes', 'work_bytes', 'max_membership_tests',
                  'max_geometry_operations', 'max_geometry_vertices'):
        if getattr(request.options, field) < getattr(result.options, field):
            return False
    for role, count in [('window', 'window_count'), ('tissue', 'tissue_count'),
                        ('exclusion', 'exclusion_count'), ('cell', 'cell_count')]:
        if getattr(request, count) != getattr(result, count):
            return False
        for i in range(getattr(request, count)):
            a, b = getattr(request, role + '_object')(i), getattr(result, role + '_object')(i)
            if (a.id, a.revision, a.geometry_sha256) != (b.id, b.revision, b.geometry_sha256):
                return False
            if role == 'cell' and (a.kind, a.nucleus, a.centroid.x, a.centroid.y) != (b.kind, b.nucleus, b.centroid.x, b.centroid.y):
                return False
    return True


def _atomic_json(path: Path, value: dict[str, Any], limit: int = 16 * MIB) -> None:
    temporary = path.with_name('.' + path.name + '.part')
    created = False
    try:
        with temporary.open('xb') as stream:
            created = True
            written = 0
            for chunk in json.JSONEncoder(indent=2, allow_nan=False).iterencode(value):
                data = chunk.encode('utf-8')
                written += len(data)
                if written > limit:
                    raise ValueError('JSON output exceeds byte limit')
                stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.link(temporary, path)  # Atomic no-replace publication, like native exports.
    finally:
        if created:
            temporary.unlink(missing_ok=True)


def _measure(doc: Any, request: Any, cancelled: threading.Event) -> Any:
    job = q.SpatialDensityJob()
    try:
        job.request(request)
        while True:
            if cancelled.is_set():
                job.cancel()
            stats = job.poll(doc)
            if not job.busy:
                break
            cancelled.wait(.02)
        if cancelled.is_set() or stats.state == 'cancelled':
            raise Cancelled()
        if stats.state != 'ready' or job.result is None:
            raise RuntimeError(stats.error or f'native density ended in {stats.state}')
        return job.result
    finally:
        if job.busy:
            job.cancel()
            job.wait(doc)


def _resume(resume: Path | None, image: int, project_id: str, request: Any,
            engine: dict[str, str], checkpoint_bytes: int) -> tuple[Any, dict[str, Any]] | None:
    if resume is None:
        return None
    path = resume / f'image-{image}.json'
    if not path.exists():
        return None
    receipt = _read_json(path, MIB)
    required = {'schema', 'project_id', 'image_id', 'engine', 'checkpoint',
                'checkpoint_image_id', 'checkpoint_sha256', 'record_id',
                'result_sha256', 'result_input_sha256'}
    if not required <= receipt.keys():
        raise ValueError('resume checkpoint is incomplete')
    _integer(receipt['image_id'], 'checkpoint image_id')
    if receipt.get('schema') != CHECKPOINT_SCHEMA or receipt.get('image_id') != image or receipt.get('project_id') != project_id:
        raise ValueError('resume checkpoint identity/schema differs')
    if receipt.get('engine') != engine:
        return None
    checkpoint = Path(receipt['checkpoint'])
    observed = FileGuard(checkpoint, checkpoint_bytes, project=True)
    if observed.sha256 != receipt['checkpoint_sha256']:
        raise ValueError('resume checkpoint checksum differs; preserved without replacement')
    saved = q.Project.open_without_journal_recovery(str(observed.path))
    doc = saved.document(_integer(receipt['checkpoint_image_id'], 'checkpoint_image_id'))
    record = doc.density_record(_integer(receipt['record_id'], 'record_id'))
    q.SpatialDensityRequest.replay(doc, record.id)
    if record.result.sha256 != receipt['result_sha256'] or record.result.input_sha256 != receipt['result_input_sha256']:
        raise ValueError('resume result identity differs')
    observed.check()
    if _engine(record.result.provenance.environment) != engine or not _same_recipe(request, record.result):
        return None
    return record.result, receipt


def _saved_record(doc: Any, selection: dict[str, Any], request: Any,
                  engine: dict[str, str]) -> Any:
    if 'record_id' not in selection:
        return None
    try:
        record = doc.density_record(selection['record_id'])
        q.SpatialDensityRequest.replay(doc, record.id)
    except (ValueError, IndexError):  # A missing/stale hint falls back to fresh computation.
        return None
    if _engine(record.result.provenance.environment) == engine and _same_recipe(request, record.result):
        return record
    return None


def _checkpoint(project: Any, image: Any, doc: Any, path: Path,
                snapshot_bytes: int, file_bytes: int) -> int:
    single = q.Project()
    single.set_class_registry(project.classes, project.class_defaults)
    single.set_metadata(project.metadata)
    local = single.add_document(doc, image.name,
                                '' if image.embedded else image.source_path,
                                image.reader)
    single.set_image_metadata(local, image.metadata)
    single.set_image_details(local, image.description, image.tags)
    view = project.view(image.id)
    if view is not None:
        single.set_view(local, view)
    single.save_new(str(path), snapshot_bytes=snapshot_bytes, file_bytes=file_bytes)
    return local


def _csv_line(values: list[Any]) -> bytes:
    text = io.StringIO(newline='')
    csv.writer(text, lineterminator='\n').writerow(values)
    return text.getvalue().encode('utf-8')


def _append_csv(stream: Any, result: Any, context: list[Any], limit: int,
                cancelled: threading.Event) -> int:
    start = stream.tell()
    try:
        reader = csv.reader(io.StringIO(q.spatial_density_csv(result, byte_limit=64 * MIB), newline=''))
        if tuple(next(reader)) != NATIVE_COLUMNS:
            raise ValueError('unsupported native density CSV schema')
        rows = 0
        for fields in reader:
            if cancelled.is_set():
                raise Cancelled()
            data = _csv_line(context + fields)
            if len(data) > limit - stream.tell():
                raise CsvBudget('combined CSV byte budget exhausted')
            stream.write(data)
            rows += 1
        return rows
    except BaseException:
        stream.seek(start)
        stream.truncate()  # Retain complete earlier images, never a partial image table.
        raise


def run(manifest: Path, output: Path, *, resume_from: Path | None = None,
        cancelled: threading.Event | None = None,
        on_image: Callable[[dict[str, Any]], None] | None = None,
        recompute: bool = False, stop_on_error: bool = False,
        csv_bytes: int = 64 * MIB, project_bytes: int = 1024 * MIB,
        checkpoint_bytes: int = 1024 * MIB) -> dict[str, Any]:
    for name, value in [('csv_bytes', csv_bytes), ('project_bytes', project_bytes), ('checkpoint_bytes', checkpoint_bytes)]:
        _integer(value, name, 1024 * MIB)
    header = _csv_line(list(CONTEXT_COLUMNS + NATIVE_COLUMNS))
    if len(header) > csv_bytes:
        raise CsvBudget('CSV budget is smaller than its header')
    output = Path(output).resolve()
    if output.exists():
        raise FileExistsError('choose a new output directory; prior runs are immutable')
    stop = cancelled if cancelled is not None else threading.Event()
    manifest_guard, source, selections, options = load_manifest(Path(manifest))
    source_guard = FileGuard(source, project_bytes, project=True)
    project = q.Project.open_without_journal_recovery(str(source_guard.path))
    source_guard.check()
    catalog = {image.id: image for image in project.images}
    protected = [source_guard.path, manifest_guard.path]
    protected += [Path(i.source_path).resolve() for i in catalog.values() if not i.embedded and i.source_path]
    if any(p == output or p.is_relative_to(output) for p in protected):
        raise ValueError('output directory contains a protected project/manifest/image input')
    resume = Path(resume_from).resolve(strict=True) if resume_from is not None else None
    if resume is not None and (not resume.is_dir() or resume == output or resume.is_relative_to(output) or output.is_relative_to(resume)):
        raise ValueError('resume directory must be an existing separate prior output')
    output.mkdir(parents=True, exist_ok=False)
    engine = _engine(q.build_environment())
    report: dict[str, Any] = dict(schema=REPORT_SCHEMA, state='running', project_id=project.identity,
        project=str(source_guard.path), project_sha256=source_guard.sha256,
        manifest=str(manifest_guard.path), manifest_sha256=manifest_guard.sha256,
        engine=engine, selected=len(selections), images=[], rows=0, error='')
    staging = output / '.density.csv.part'
    with staging.open('xb') as stream:
        stream.write(header)
        try:
            for selected in selections:
                if stop.is_set():
                    break
                entry: dict[str, Any] = {'image_id': selected['image_id'], 'state': 'failed', 'rows': 0}
                try:
                    source_guard.check()
                    manifest_guard.check()
                    image = catalog.get(selected['image_id'])
                    if image is None:
                        raise ValueError(f'image {selected["image_id"]} does not exist in the project')
                    doc = project.document(image.id)
                    request = q.SpatialDensityRequest(doc, selected['windows'], selected['tissue'], selected['excluded'], selected['cells'], options)
                    cached = None if recompute else _resume(resume, image.id, project.identity, request, engine, checkpoint_bytes)
                    if cached:
                        result, receipt = cached
                        receipt = dict(receipt, reuse='checkpoint')
                    else:
                        saved = None if recompute else _saved_record(doc, selected, request, engine)
                        if saved:
                            result, record_id, reuse = saved.result, saved.id, 'source_record'
                        else:
                            result = _measure(doc, request, stop)
                            if stop.is_set():
                                raise Cancelled()
                            record_id = doc.keep_density(request, result)
                            reuse = 'computed'
                        checkpoint = output / f'image-{image.id}.qpxproj'
                        local = _checkpoint(project, image, doc, checkpoint, 512 * MIB, checkpoint_bytes)
                        observed = FileGuard(checkpoint, checkpoint_bytes, project=True)
                        receipt = dict(schema=CHECKPOINT_SCHEMA, project_id=project.identity, image_id=image.id,
                            checkpoint=str(checkpoint), checkpoint_image_id=local, checkpoint_sha256=observed.sha256,
                            record_id=record_id, result_sha256=result.sha256, result_input_sha256=result.input_sha256,
                            engine=engine, reuse=reuse)
                    source_guard.check()
                    manifest_guard.check()
                    receipt = dict(receipt, request_input_sha256=request.input_sha256)
                    _atomic_json(output / f'image-{image.id}.json', receipt, MIB)
                    context = [project.identity, image.id, image.name, receipt['record_id'], receipt['reuse'], request.input_sha256, receipt['checkpoint']]
                    rows = _append_csv(stream, result, context, csv_bytes, stop)
                    entry.update(receipt, state='complete', rows=rows, image_name=image.name)
                    report['rows'] += rows
                except Cancelled:
                    stop.set()
                    entry.update(state='cancelled', error='cancelled; no partial image CSV published')
                except Exception as error:
                    entry['error'] = str(error)[:2048]
                    if isinstance(error, (InputChanged, CsvBudget)):
                        report['error'] = entry['error']
                report['images'].append(entry)
                if on_image is not None:
                    on_image(copy.deepcopy(entry))
                if stop.is_set() or report['error'] or (entry['state'] == 'failed' and stop_on_error):
                    break
            source_guard.check(rehash=True)
            manifest_guard.check(rehash=True)
        except Exception as error:
            report['error'] = str(error)[:2048]
        report['computed'] = sum(i['state'] == 'complete' and i['reuse'] == 'computed' for i in report['images'])
        report['reused'] = sum(i['state'] == 'complete' and i['reuse'] != 'computed' for i in report['images'])
        report['failed'] = sum(i['state'] == 'failed' for i in report['images'])
        report['not_run'] = len(selections) - len(report['images'])
        report['state'] = 'cancelled' if stop.is_set() else 'partial' if report['failed'] or report['error'] else 'completed'
        stream.flush()
        os.fsync(stream.fileno())
    destination = output / ('density.csv' if report['state'] == 'completed' else 'density.partial.csv')
    os.link(staging, destination)
    staging.unlink()
    report.update(csv_path=str(destination), report_path=str(output / 'report.json'))
    _atomic_json(output / 'report.json', report)
    return report


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path)
    parser.add_argument('output', type=Path, help='new directory; previous runs are never overwritten')
    parser.add_argument('--resume-from', type=Path)
    parser.add_argument('--recompute', action='store_true')
    parser.add_argument('--stop-on-error', action='store_true')
    parser.add_argument('--csv-bytes', type=int, default=64 * MIB)
    parser.add_argument('--project-bytes', type=int, default=1024 * MIB)
    parser.add_argument('--checkpoint-bytes', type=int, default=1024 * MIB)
    args = parser.parse_args(argv)
    cancelled = threading.Event()
    previous = signal.signal(signal.SIGINT, lambda *_: cancelled.set())
    try:
        report = run(args.manifest, args.output, resume_from=args.resume_from, cancelled=cancelled,
                     recompute=args.recompute, stop_on_error=args.stop_on_error,
                     csv_bytes=args.csv_bytes, project_bytes=args.project_bytes, checkpoint_bytes=args.checkpoint_bytes)
    except Exception as error:
        print(json.dumps({'schema': REPORT_SCHEMA, 'state': 'failed', 'error': str(error)}))
        return 2
    finally:
        signal.signal(signal.SIGINT, previous)
    print(json.dumps({key: report[key] for key in ('schema', 'state', 'computed', 'reused', 'failed', 'not_run', 'rows', 'csv_path', 'report_path')}))
    return 130 if report['state'] == 'cancelled' else 0 if report['state'] == 'completed' else 1


if __name__ == '__main__':
    raise SystemExit(main())

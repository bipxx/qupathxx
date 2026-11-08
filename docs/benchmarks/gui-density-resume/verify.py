"""Verify actual compute/resume desktop runs without recalculating density.

Argument: main build/gui-density-resume. Writes new desktop.json and compiler
manifest; preserves all source/prior-run files. Uses installed candidate4 API.
"""
import csv
import hashlib
import json
from pathlib import Path
import sys

import qupathxx as q

ROOT = Path(__file__).resolve().parents[3]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    work = Path(sys.argv[1]).resolve(strict=True)
    first_dir, resumed_dir = work / 'desktop-first', work / 'desktop-reused'
    first = json.loads((first_dir / 'report.json').read_text(encoding='utf-8'))
    resumed = json.loads((resumed_dir / 'report.json').read_text(encoding='utf-8'))
    assert first['computed'] == first['completed'] == 2 and first['reused'] == 0
    assert resumed['reused'] == resumed['completed'] == 2 and resumed['computed'] == resumed['failed'] == 0
    assert not list(resumed_dir.glob('*.qpxproj'))
    before = json.loads((work / 'desktop-first-files.json').read_text(encoding='utf-8'))
    for name, digest in before.items():
        assert sha(first_dir / name) == digest
    assert sha(Path(resumed['source'])) == resumed['source_sha256']
    tables = []
    for directory in (first_dir, resumed_dir):
        with (directory / 'density.csv').open(encoding='utf-8', newline='') as stream:
            tables.append(list(csv.DictReader(stream)))
    assert [int(row['count']) for row in tables[1]] == [31, 27]
    native_columns = list(tables[0][0])[7:]
    assert [[row[name] for name in native_columns] for row in tables[0]] == [[row[name] for name in native_columns] for row in tables[1]]
    engine = None
    for a, b in zip(first['images'], resumed['images'], strict=True):
        assert a['checkpoint'] == b['checkpoint'] and a['result_sha256'] == b['result_sha256']
        receipt = json.loads((resumed_dir / f"image-{b['image_id']}.json").read_text(encoding='utf-8'))
        assert sha(Path(receipt['checkpoint'])) == receipt['checkpoint_sha256']
        saved = q.Project.open_without_journal_recovery(receipt['checkpoint'])
        doc = saved.document(receipt['checkpoint_image_id'])
        record = doc.density_record(receipt['record_id'])
        assert record.result.sha256 == receipt['result_sha256']
        q.SpatialDensityRequest.replay(doc, record.id)  # Capture validation only, no kernel.
        env = record.result.provenance.environment
        observed = {name: getattr(env, name) for name in receipt['engine']}
        assert observed == receipt['engine']
        if engine is None:
            engine = observed
        else:
            assert engine == observed
    inputs = sorted([p for directory, suffixes in [('cpp', ('.cpp', '.hpp')),
        ('cmake', ('.cmake', '.in')), ('src', ('.py',))]
        for p in (ROOT / directory).rglob('*') if p.is_file() and p.suffix in suffixes] +
        [ROOT / 'CMakeLists.txt', ROOT / 'pyproject.toml'], key=lambda p: p.relative_to(ROOT).as_posix())
    manifest = ''.join(p.relative_to(ROOT).as_posix() + '=' + sha(p) + '\n' for p in inputs).encode('utf-8')
    assert hashlib.sha256(manifest).hexdigest() == engine['source_tree_sha256']
    members = json.loads((work / 'application-files.json').read_text(encoding='utf-8'))
    for name, digest in members.items():
        assert sha(work / 'application' / name) == digest
    result = dict(engine=engine, compiler_input_count=len(inputs),
        application_sha256=members['qupathxx_app.exe'], bundle_members_verified=len(members),
        first_computed=2, resumed_computed=0, resumed_reused=2, new_qpx_copies=0,
        prior_run_files_unchanged=True, source_unchanged=True, native_csv_columns_exact=True,
        counts=[31, 27], cached_records_replay=True, no_observer_density_computation=True,
        source_project_sha256=resumed['source_sha256'],
        limits='Same-app local Windows x64 reuse, not numerical re-verification, speed superiority '
               'or a new release/platform/biological claim. GUI receipts and Python receipts differ; '
               'older or changed engines compute. Published candidate4 remains unchanged.')
    for name, data in [('source-inputs.sha256', manifest), ('desktop.json', json.dumps(result, indent=2).encode('utf-8'))]:
        with (work / name).open('xb') as stream:
            stream.write(data)
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()

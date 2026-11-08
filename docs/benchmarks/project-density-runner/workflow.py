"""Two-image CLI demonstration using the existing506-cell reference crop twice.

This tests image namespaces and orchestration; it is not a two-specimen study.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import subprocess
import sys

import qupathxx as q

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reference_project', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    project = q.Project()
    selections = []
    for i in range(2):
        loaded = q.Project.open_without_journal_recovery(str(args.reference_project.resolve()))
        doc = loaded.document(1)
        cells = sorted(obj.id for obj in doc.annotations if obj.kind == q.ObjectKind.CELL)
        assert len(cells) == 506
        image = project.add_document(doc, f'Reference crop copy {i + 1}')
        selections.append(dict(image_id=image, windows=[507, 508, 509, 510],
                               tissue=[511, 512], excluded=[513, 514], cells=cells,
                               record_id=17))
    source = output / 'study.qpxproj'
    project.save_new(str(source))
    source_sha = hashlib.sha256(source.read_bytes()).hexdigest()
    manifest = output / 'selections.json'
    manifest.write_text(json.dumps(dict(schema='qpx.project-density/1', project=source.name,
                                        images=selections), indent=2), encoding='utf-8')
    script = ROOT / 'examples/project_density.py'
    summaries = []
    for name, flags in [('first', ['--recompute']),
                         ('resumed', ['--resume-from', str(output / 'first')])]:
        completed = subprocess.run([sys.executable, '-I', str(script), str(manifest),
                                    str(output / name), *flags], capture_output=True,
                                   text=True, timeout=60)
        assert completed.returncode == 0, completed.stdout + completed.stderr
        summary = json.loads(completed.stdout)
        assert summary['state'] == 'completed' and summary['rows'] == 8
        assert summary['computed'] == (2 if name == 'first' else 0)
        assert summary['reused'] == (0 if name == 'first' else 2)
        with Path(summary['csv_path']).open(encoding='utf-8', newline='') as stream:
            rows = list(csv.DictReader(stream))
        assert [int(row['count']) for row in rows] == [289, 121, 168, 0] * 2
        assert [float(row['area_pixels']) for row in rows] == [40576, 17536, 23040, 0] * 2
        assert [row['image_id'] for row in rows] == ['1'] * 4 + ['2'] * 4
        assert rows[3]['status'] == rows[7]['status'] == 'empty_area'
        assert rows[3]['density_per_mm2'] == rows[7]['density_per_mm2'] == ''
        summaries.append(summary)
    assert not list((output / 'resumed').glob('*.qpxproj'))
    assert hashlib.sha256(source.read_bytes()).hexdigest() == source_sha
    first = json.loads((output / 'first/report.json').read_text(encoding='utf-8'))
    resumed = json.loads((output / 'resumed/report.json').read_text(encoding='utf-8'))
    assert [i['result_sha256'] for i in first['images']] == [i['result_sha256'] for i in resumed['images']]
    assert [i['checkpoint'] for i in first['images']] == [i['checkpoint'] for i in resumed['images']]
    for entry in first['images']:
        checkpoint = q.Project.open_without_journal_recovery(entry['checkpoint'])
        doc = checkpoint.document(entry['checkpoint_image_id'])
        assert doc.density_record(entry['record_id']).result.sha256 == entry['result_sha256']
        q.SpatialDensityRequest.replay(doc, entry['record_id'])
    receipt = dict(native_revision=q.build_environment().git_revision,
                   native_tree_sha256=q.build_environment().source_tree_sha256,
                   runner_sha256=hashlib.sha256(script.read_bytes()).hexdigest(),
                   original_reference_sha256=hashlib.sha256(args.reference_project.read_bytes()).hexdigest(),
                   project_sha256=source_sha, images=2, cells_per_image=506,
                   rows=8, computed_first_run=2, reused_second_run=2,
                   resumed_project_copies=0, source_unchanged=True,
                   expected_counts_per_image=[289, 121, 168, 0],
                   expected_area_pixels_per_image=[40576, 17536, 23040, 0],
                   checkpoints_reopened=True,
                   scope='Same existing reference crop duplicated to test image identity/orchestration, not independent specimens; candidate4 numeric oracles reused. No native build, new wheel/install, segmentation or UI campaign.')
    (output / 'workflow.json').write_text(json.dumps(receipt, indent=2), encoding='utf-8')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()

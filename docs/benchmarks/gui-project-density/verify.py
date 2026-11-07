"""Verify actual desktop outputs and the local app bundle; no GUI input/build.

Argument: main build/gui-project-density. Writes new desktop.json and source
manifest there. Requires the unchanged installed candidate4 Python API.
"""
import csv
from datetime import datetime, timezone
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
    output = work / 'desktop-final-run'
    report = json.loads((output / 'report.json').read_text(encoding='utf-8'))
    with (output / 'density.csv').open(encoding='utf-8', newline='') as stream:
        rows = list(csv.DictReader(stream))
    assert report['state'] == 'completed' and report['completed'] == 2 and report['failed'] == 0
    assert [int(row['count']) for row in rows] == [31, 27]
    assert [float(row['area_pixels']) for row in rows] == [104006, 101124]
    assert sha(Path(report['source'])) == report['source_sha256']
    original = json.loads((work / 'public-controller/report.json').read_text(encoding='utf-8'))
    assert sha(Path(original['source'])) == original['source_sha256']
    environments = []
    for entry in report['images']:
        saved = q.Project.open_without_journal_recovery(entry['checkpoint'])
        doc = saved.document(entry['checkpoint_image_id'])
        stored = doc.density_record(entry['record_id']).result
        assert stored.sha256 == entry['result_sha256']
        request = q.SpatialDensityRequest.replay(doc, entry['record_id'])
        assert doc.verify_density(entry['record_id'], request, q.measure_spatial_density(request))
        environment = stored.provenance.environment
        environments.append({name: getattr(environment, name) for name in (
            'git_revision', 'source_tree_sha256', 'compiler', 'compiler_flags', 'configuration', 'platform')})
    assert environments[0] == environments[1]
    inputs = sorted([p for directory, extensions in [('cpp', ('.cpp', '.hpp')),
        ('cmake', ('.cmake', '.in')), ('src', ('.py',))]
        for p in (ROOT / directory).rglob('*') if p.is_file() and p.suffix in extensions] +
        [ROOT / 'CMakeLists.txt', ROOT / 'pyproject.toml'],
        key=lambda p: p.relative_to(ROOT).as_posix())  # CMake sorts case-sensitive strings; WindowsPath does not.
    manifest = ''.join(p.relative_to(ROOT).as_posix() + '=' + sha(p) + '\n' for p in inputs).encode('utf-8')
    tree_sha = hashlib.sha256(manifest).hexdigest()
    assert tree_sha == environments[0]['source_tree_sha256']
    members = json.loads((work / 'application-files.json').read_text(encoding='utf-8'))
    for name, expected in members.items():
        assert sha(work / 'application' / name) == expected
    for name in ('desktop-preview.png', 'desktop-invalidated.png', 'desktop-completed.png'):
        assert (work / name).is_file()
    files = ['CMakeLists.txt', 'cpp/gui.cpp', 'cpp/gui_project_density.hpp',
             'cpp/project_density_panel.hpp', 'cpp/project_density_panel.cpp',
             'cpp/tests/project_density_panel_tests.cpp']
    receipt = dict(verified_utc=datetime.now(timezone.utc).isoformat(),
        application_environment=environments[0], compiler_input_count=len(inputs),
        compiler_tree_sha256=tree_sha, files_sha256={name: sha(ROOT / name) for name in files},
        application_sha256=members['qupathxx_app.exe'], bundle_files=len(members),
        bundle_files_verified=True, reader_dlls_match_candidate4=True,
        desktop_selected_cells=[32, 28], desktop_density_counts=[31, 27],
        desktop_area_pixels=[104006, 101124], saved_images=2, csv_rows=2, failed_images=0,
        source_project_sha256=report['source_sha256'], source_and_original_fixture_unchanged=True,
        checkpoint_stored_hashes_exact=True, candidate4_python_replay_verifies=True,
        selector_change_clears_preview_observed=True, short_id_rows_readable=True,
        results_folder_opener_observed=True, csv_opener_reached_windows_app_chooser=True,
        csv_association_changed=False, binding_or_wheel_rebuilt=False,
        limits='Local Windows x64 development app and public two-FOV annotated-nucleus pilot. '
               'Density kernels/codecs/CSV and published candidate4 are unchanged. '
               'No new release, fresh-host, additional platform, biology, tissue/whole-cell '
               'segmentation or speed claim. GUI panel has no resume control.')
    for name, data in [('source-inputs.sha256', manifest),
                       ('desktop.json', json.dumps(receipt, indent=2).encode('utf-8'))]:
        with (work / name).open('xb') as stream:
            stream.write(data)
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()

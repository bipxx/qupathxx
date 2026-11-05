"""Prepare a native archive from the frozen wheel, without compiling anything."""
import argparse
import hashlib
import json
import zipfile
from pathlib import Path

import qupathxx as q


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('wheel', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    receipt = json.loads((Path(__file__).parent.parent / 'stain-model-r7/shipping-integrity.json').read_text())
    assert hashlib.sha256(args.wheel.read_bytes()).hexdigest() == receipt['wheel_sha256']
    environment = q.build_environment()
    assert environment.git_revision == receipt['environment']['git_revision']
    assert environment.source_tree_sha256 == receipt['environment']['source_tree_sha256']
    args.output.mkdir(parents=True, exist_ok=True)
    archive = args.output / 'qupathxx-windows-x64-candidate.zip'
    native = []
    notices = []
    license_data = (Path(__file__).resolve().parents[3] / 'LICENSE').read_bytes()
    installed = Path(q.__file__).parent.parent
    with zipfile.ZipFile(args.wheel) as wheel:
        for name, digest in receipt['wheel_members'].items():
            data = wheel.read(name)
            assert hashlib.sha256(data).hexdigest() == digest, name
            assert hashlib.sha256((installed / name).read_bytes()).hexdigest() == digest, name
        with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED) as output:
            for name in sorted(wheel.namelist()):
                if not name.startswith('qupathxx/') or name.endswith('/'):
                    continue
                rel = name[len('qupathxx/'):]
                if rel.startswith(('fonts/', 'licenses/')) or Path(rel).suffix in {'.exe', '.dll'}:
                    output.writestr(rel, wheel.read(name))
                    if rel.startswith('licenses/'):
                        notices.append(rel)
                    if Path(rel).suffix in {'.exe', '.dll'}:
                        native.append(rel)
            output.writestr('LICENSE', license_data)
            output.writestr('licenses/qupathxx/LICENSE', license_data)
            notices.append('licenses/qupathxx/LICENSE')
            output.writestr('README.txt', (
                'qupathxx Windows x64 local engineering candidate\n'
                'Compiled source: 6219a08f1713d641633156e70f12b3fd70331d82\n'
                'Extract the whole archive. Keep DLLs, fonts and licenses beside the EXEs.\n'
                'Open: .\\qupathxx_app.exe C:\\data\\CMU-1.svs\n'
                'Reopen: .\\qupathxx_app.exe --project C:\\data\\study.qpxproj\n'
                'Windows x64, an OpenGL-capable desktop/driver and the matching Microsoft\n'
                'Visual C++ x64 runtime are external prerequisites. No Python is needed.\n'
                'Uninstall: close the apps and remove this extracted folder. Keep projects\n'
                'and source slides separately; deleting the app does not delete them.\n'
                'Project code: AGPL-3.0-only; see LICENSE. Dependencies keep their own\n'
                'licenses under licenses/. This is a local engineering candidate,\n'
                'not a public supported release.\n'
                'Scope: one bounded calibrated RGB8 Aperio ROI, tissue/statistics, CSV\n'
                'and saved replay. No general biology, platform or speed claim.\n'))
    summary = dict(source_revision=environment.git_revision,
                   source_tree_sha256=environment.source_tree_sha256,
                   wheel_sha256=receipt['wheel_sha256'],
                   original_wheel_and_installed_members_verified=len(receipt['wheel_members']),
                   native_archive=str(archive),
                   native_archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),
                   native_binaries=native, notice_files=notices,
                   native_rebuilds=0, wheel_rebuilds=0,
                   project_license='AGPL-3.0-only',
                   project_license_sha256=hashlib.sha256(license_data).hexdigest())
    (args.output / 'candidate.json').write_text(json.dumps(summary, indent=2))
    print(json.dumps({k: v for k, v in summary.items() if k != 'notice_files'}, indent=2))


if __name__ == '__main__':
    main()

"""Apply the owner's license decision to a retained wheel; reuse native bytes."""
import argparse
import base64
import csv
import hashlib
import io
import json
import zipfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('wheel', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    receipt = json.loads((here.parent / 'stain-model-r7/shipping-integrity.json').read_text())
    assert hashlib.sha256(args.wheel.read_bytes()).hexdigest() == receipt['wheel_sha256']
    license_data = (here.parents[2] / 'LICENSE').read_bytes()
    info = 'qupathxx-0.1.0.dist-info/'
    args.output.mkdir(parents=True, exist_ok=True)
    destination = args.output / 'qupathxx-0.1.0-1-cp312-cp312-win_amd64.whl'
    with zipfile.ZipFile(args.wheel) as original:
        files = {n: original.read(n) for n in original.namelist() if not n.endswith('/')}
    metadata = files[info + 'METADATA'].decode()
    header, body = metadata.split('\n\n', 1)
    header = header.replace('Metadata-Version: 2.2', 'Metadata-Version: 2.4')
    assert 'Metadata-Version: 2.4' in header
    header += '\nLicense-Expression: AGPL-3.0-only\nLicense-File: LICENSE'
    body = body.replace('Downloaded dependencies retain their upstream licenses. A project license has\nnot yet been selected.',
                        'Project code: AGPL-3.0-only; see the bundled LICENSE. Dependencies retain their separate upstream licenses.')
    files[info + 'METADATA'] = (header + '\n\n' + body).encode()
    files[info + 'WHEEL'] = files[info + 'WHEEL'].rstrip() + b'\nBuild: 1\n'
    files[info + 'licenses/LICENSE'] = license_data
    files['qupathxx/licenses/qupathxx/LICENSE'] = license_data
    files.pop(info + 'RECORD')
    record = io.StringIO(newline='')
    writer = csv.writer(record, lineterminator='\n')
    for name, data in sorted(files.items()):
        digest = base64.urlsafe_b64encode(hashlib.sha256(data).digest()).rstrip(b'=').decode()
        writer.writerow([name, 'sha256=' + digest, len(data)])
    writer.writerow([info + 'RECORD', '', ''])
    files[info + 'RECORD'] = record.getvalue().encode()
    with zipfile.ZipFile(destination, 'x', compression=zipfile.ZIP_DEFLATED) as wheel:
        for name, data in sorted(files.items()):
            wheel.writestr(name, data)
    unchanged = [n for n, digest in receipt['wheel_members'].items()
                 if n not in {info + 'METADATA', info + 'WHEEL'}]
    assert all(hashlib.sha256(files[n]).hexdigest() == receipt['wheel_members'][n] for n in unchanged)
    result = dict(wheel=str(destination), sha256=hashlib.sha256(destination.read_bytes()).hexdigest(),
                  seed_wheel_sha256=receipt['wheel_sha256'], native_source=receipt['environment'],
                  project_license='AGPL-3.0-only', preserved_original_members=len(unchanged),
                  changed_members=[info+'METADATA', info+'WHEEL', info+'RECORD'],
                  added_members=[info+'licenses/LICENSE', 'qupathxx/licenses/qupathxx/LICENSE'],
                  native_rebuilds=0)
    (args.output / 'wheel-license-overlay.json').write_text(json.dumps(result, indent=2))
    print(json.dumps({k:v for k,v in result.items() if k!='native_source'}, indent=2))


if __name__ == '__main__':
    main()

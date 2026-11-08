"""Collect original candidate257 remaining-role observations without redispatch or acceptance."""
from pathlib import Path, PurePosixPath
import datetime
import hashlib
import json
import re
import stat
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[2]
STATE = json.loads((ROOT / 'build/raw-link-qualification-257/campaign.json').read_text(encoding='utf-8'))
OUT = ROOT / 'build/q257-rest'
OUT.mkdir(parents=True, exist_ok=True)
STAMP = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
OBS = OUT / STAMP
OBS.mkdir(exist_ok=False)
NAMES = {
    'engine': {'Windows-gui-py3.12', 'Windows-headless-py3.11', 'Linux-headless-py3.12', 'Linux-minimal-py3.12', 'Linux-sanitize-py3.12'},
    'macos': {'macos-native-arm64', 'macos-native-x86_64'},
    'portable': {'manylinux-3.11', 'manylinux-3.12', 'manylinux-consumer-3.11', 'manylinux-consumer-3.12'},
}

def query(args):
    return json.loads(subprocess.check_output(['gh', *args], encoding='utf-8', timeout=45))

def save(path, data):
    path.write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

verified = []
for role in ['engine', 'portable']:
    attempt = STATE['roles'][role]
    run_id = attempt['run']['databaseId']
    run = query(['run', 'view', str(run_id), '--repo', 'bipxx/qupathxx', '--json', 'databaseId,headSha,status,conclusion,jobs,url'])
    save(OBS / (role + '-run.json'), run)
    assert run['databaseId'] == run_id and run['headSha'] == STATE['source']
    print(role, run['status'], run['conclusion'], flush=True)
    metadata = query(['api', f'repos/bipxx/qupathxx/actions/runs/{run_id}/artifacts', '--paginate'])
    save(OBS / (role + '-artifacts.json'), metadata)
    seen = set()
    for item in metadata['artifacts']:
        name = item['name']
        assert name in NAMES[role] and name not in seen
        seen.add(name)
        assert item['workflow_run']['id'] == run_id
        assert item['workflow_run']['head_sha'] == STATE['source'] and not item['expired']
        assert re.fullmatch(r'sha256:[0-9a-f]{64}', item['digest'])
        target = OUT / (name + '.zip')
        part = OUT / (name + '.zip.part')
        if not target.exists():
            assert not part.exists(), 'Inspect retained partial transfer before retrying'
            with part.open('xb') as stream, (OBS / (name + '-download.log')).open('xb') as error:
                result = subprocess.run(['gh', 'api', f"repos/bipxx/qupathxx/actions/artifacts/{item['id']}/zip"], stdout=stream, stderr=error, timeout=240)
            assert result.returncode == 0, 'Retained failed transfer'
            assert digest(part) == item['digest'].split(':')[1]
            assert part.stat().st_size == item['size_in_bytes']
            part.rename(target)
        assert digest(target) == item['digest'].split(':')[1]
        assert target.stat().st_size == item['size_in_bytes']
        dest = OUT / name
        if not dest.exists():
            with zipfile.ZipFile(target) as archive:
                members = archive.infolist()
                paths = set()
                for entry in members:
                    path = PurePosixPath(entry.filename)
                    assert not path.is_absolute() and '..' not in path.parts
                    assert '\\' not in entry.filename and ':' not in entry.filename
                    assert not stat.S_ISLNK(entry.external_attr >> 16)
                    key = entry.filename.rstrip('/').casefold()
                    assert key not in paths, 'Duplicate/case-colliding archive member'
                    paths.add(key)
                    assert (dest / entry.filename).resolve().is_relative_to(dest.resolve())
                dest.mkdir()
                archive.extractall(dest)
        save(OBS / (name + '-artifact.json'), item)
        verified.append({'name': name, 'artifact_id': item['id'], 'run_id': run_id, 'bytes': item['size_in_bytes'], 'digest': item['digest']})
        print('authenticated', name, item['size_in_bytes'], flush=True)
    for job in run['jobs']:
        if job['status'] == 'completed' and job['conclusion'] == 'failure':
            target = OUT / (str(job['databaseId']) + '-failed-job.log')
            if not target.exists():
                with target.open('xb') as stream:
                    subprocess.run(['gh', 'api', f"repos/bipxx/qupathxx/actions/jobs/{job['databaseId']}/logs"], stdout=stream, check=True, timeout=60)
save(OBS / 'verified.json', {'source': STATE['source'], 'files': verified, 'accepted': False})
print('Preserved', len(verified), 'archives; partial observation, not acceptance.', OBS, flush=True)

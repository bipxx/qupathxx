"""One exact-source successor campaign after terminal258 and local260 gates."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import subprocess
import time
import xml.etree.ElementTree as ET

ws = Path(__file__).resolve().parents[2]
source = ws / 'build/ci/root-locator-qualification-260-audit'
out = ws / 'build/root-locator-fix-260'
path = out / 'campaign.json'
commit = 'f9ad28f5d88fb6706163f8a6c2e0ffe7a96e4ec3'
ref = 'feature/root-locator-qualification-260'


def require(value, message):
    if not value:
        raise ValueError(message)


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def query():
    return json.loads(subprocess.check_output(['gh', 'run', 'list', '--repo',
        'bipxx/qupathxx', '--workflow', 'ci.yml', '--branch', ref, '--event',
        'workflow_dispatch', '--limit', '20', '--json',
        'databaseId,headSha,status,conclusion,url,createdAt'], text=True))


require(not path.exists(), 'Campaign exists; inspect without redispatch')
require(subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=source, text=True).strip() == commit, 'checkout source')
require(not subprocess.check_output(['git', 'status', '--porcelain'], cwd=source), 'checkout dirty')
require(subprocess.check_output(['git', 'ls-remote', 'origin', 'refs/heads/' + ref], cwd=source, text=True).split()[0] == commit, 'remote ref')
require(not query(), 'Existing dispatches on this ref; inspect them')
for name, count, skips in [('native-tests.xml', 51, 0), ('preflight.xml', 880, 0), ('python-tests.xml', 392, 6)]:
    cases = list(ET.parse(out / name).getroot().iter('testcase'))
    require(len(cases) == count and not any(c.find(k) is not None for c in cases
        for k in ['failure', 'error']), name + ' cohort')
    require(sum(c.find('skipped') is not None for c in cases) == skips, name + ' skips')
installed = read(out / 'installed-proof.json')
inputs = read(out / 'compiler-inputs.json')
require(installed['source'] == inputs['source'] == commit, 'installed source')
require(installed['fingerprint'] == inputs['fingerprint'], 'installed fingerprint')
frozen = read(out / 'frozen-audit-source-proof.json')
require(frozen['source'] == commit and frozen['clean'] and frozen['raw_source_paths'] == 490, 'frozen source proof')
originals = []
for run_id in [40440028126, 40440036308, 40440044976]:
    run = json.loads(subprocess.check_output(['gh', 'run', 'view', str(run_id),
        '--repo', 'bipxx/qupathxx', '--json', 'databaseId,status,conclusion,headSha,url'], text=True))
    require(run['status'] == 'completed' and run['headSha'] == 'a5b5394fd6a5bdfd8bfb45e239dcbc81b65ae027', 'original campaign still live or wrong source')
    originals.append(run)
helper = source / 'tools/ci/campaign_inputs.py'
spec = importlib.util.spec_from_file_location('q258_profiles', helper)
profiles = importlib.util.module_from_spec(spec)
exec(compile(helper.read_bytes(), str(helper), 'exec', dont_inherit=True), profiles.__dict__)
state = dict(schema='qpx.qualification-campaign/1', source=commit, ref=ref,
    original_campaign=originals, roles={}, overall_engineering_accepted=False,
    release_ready=False, scope='All roles at the repaired source; no transfer of candidate258 acceptance')


def save():
    path.write_text(json.dumps(state, indent=2) + '\n', encoding='utf-8')


save()
for role in ['engine', 'macos', 'portable']:
    previous = {r['databaseId'] for r in query()}
    semantic = profiles.full_inputs(role)
    wire = profiles.gh_inputs(role, semantic)
    record = dict(state='dispatch_started', semantic_inputs=semantic, gh_inputs=wire,
        at_utc=datetime.now(timezone.utc).isoformat(), known_runs_before=sorted(previous),
        input_helper_sha256=hashlib.sha256(helper.read_bytes()).hexdigest())
    state['roles'][role] = record
    save()
    try:
        result = subprocess.run(['gh', 'workflow', 'run', 'ci.yml', '--repo',
            'bipxx/qupathxx', '--ref', ref, '--json'], input=json.dumps(wire),
            capture_output=True, text=True, timeout=30)
        record.update(returncode=result.returncode, stdout=result.stdout, stderr=result.stderr,
            state='dispatch_acknowledged' if result.returncode == 0 else 'dispatch_uncertain')
        save()
        require(result.returncode == 0, 'Dispatch uncertain; inspect without retry')
    except BaseException:
        if record['state'] == 'dispatch_started':
            record['state'] = 'dispatch_uncertain'
            save()
        raise
    for _ in range(15):
        found = [r for r in query() if r['databaseId'] not in previous and r['headSha'] == commit]
        require(len(found) <= 1, 'Ambiguous run; inspect without retry')
        if found:
            record.update(state='run_identified', run=found[0])
            save()
            print(role, found[0]['databaseId'], found[0]['url'], flush=True)
            break
        time.sleep(1)
    else:
        raise RuntimeError('Acknowledged run not yet identified; poll without redispatch')

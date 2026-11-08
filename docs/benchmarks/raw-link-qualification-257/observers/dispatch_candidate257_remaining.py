"""Dispatch candidate257 remaining roles only after its strict Mac audit."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import subprocess
import time

workspace = Path(__file__).resolve().parents[2]
source = workspace / 'build/ci/raw-link-qualification-257-audit'
out = workspace / 'build/raw-link-qualification-257'
path = out / 'campaign.json'
state = json.loads(path.read_text(encoding='utf-8'))
commit = 'd57df0166888eb335d989d404bdae4d1b63c66a4'
ref = 'feature/raw-link-qualification-257'


def require(value, message):
    if not value:
        raise ValueError(message)


def save():
    path.write_text(json.dumps(state, indent=2) + '\n', encoding='utf-8')


def query():
    return json.loads(subprocess.check_output([
        'gh', 'run', 'list', '--repo', 'bipxx/qupathxx', '--workflow', 'ci.yml',
        '--branch', ref, '--event', 'workflow_dispatch', '--limit', '20',
        '--json', 'databaseId,headSha,status,conclusion,url,createdAt'], text=True))


require(state['source'] == commit and state['ref'] == ref, 'campaign identity')
require(all(state['roles'][r]['state'] == 'not_dispatched'
            for r in ['engine', 'portable']), 'Prior dispatch exists; inspect it without retry')
require(subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=source, text=True).strip() == commit, 'frozen checkout source')
require(not subprocess.check_output(['git', 'status', '--porcelain'], cwd=source), 'frozen checkout dirty')
remote = subprocess.check_output(['git', 'ls-remote', 'origin', 'refs/heads/' + ref], cwd=source, text=True)
require(remote.split()[0] == commit, 'remote source')
audit = json.loads((out / 'macos-acceptance.json').read_text(encoding='utf-8'))
require(audit['source'] == commit and audit['native_engineering_accepted'], 'Mac acceptance')
require(audit['macho_native_inventory_required'] and audit['macos_relocation_required'], 'Mac inventory/relocation gates')
require({j['architecture'] for j in audit['jobs']} == {'arm64', 'x86_64'}, 'Mac architectures')
run = json.loads(subprocess.check_output(['gh', 'run', 'view', '40422850278', '--repo', 'bipxx/qupathxx', '--json', 'headSha,status,conclusion'], text=True))
require(run['headSha'] == commit and run['status'] == 'completed' and run['conclusion'] == 'success', 'Mac terminal run')
state['roles']['macos'].update(state='engineering_accepted', terminal_run=run,
    acceptance='macos-acceptance.json', acceptance_sha256=hashlib.sha256((out / 'macos-acceptance.json').read_bytes()).hexdigest())
helper = source / 'tools/ci/campaign_inputs.py'
spec = importlib.util.spec_from_file_location('candidate257_inputs', helper)
profiles = importlib.util.module_from_spec(spec)
exec(compile(helper.read_bytes(), str(helper), 'exec', dont_inherit=True), profiles.__dict__)
save()
for role in ['engine', 'portable']:
    previous = {r['databaseId'] for r in query()}
    semantic = profiles.full_inputs(role)
    wire = profiles.gh_inputs(role, semantic)
    record = dict(state='dispatch_started', semantic_inputs=semantic, gh_inputs=wire,
        at_utc=datetime.now(timezone.utc).isoformat(), known_runs_before=sorted(previous),
        input_helper_sha256=hashlib.sha256(helper.read_bytes()).hexdigest())
    state['roles'][role] = record
    save()
    try:
        result = subprocess.run(['gh', 'workflow', 'run', 'ci.yml', '--repo', 'bipxx/qupathxx', '--ref', ref, '--json'], input=json.dumps(wire), capture_output=True, text=True, timeout=30)
        record.update(returncode=result.returncode, stdout=result.stdout, stderr=result.stderr,
            state='dispatch_acknowledged' if result.returncode == 0 else 'dispatch_uncertain')
        save()
        require(result.returncode == 0, 'Dispatch uncertain; inspect remote state without retry')
    except BaseException:
        if record['state'] == 'dispatch_started':
            record['state'] = 'dispatch_uncertain'
            save()
        raise
    for _ in range(15):
        found = [r for r in query() if r['databaseId'] not in previous and r['headSha'] == commit]
        require(len(found) <= 1, 'Ambiguous runs; inspect without retry')
        if found:
            record.update(state='run_identified', run=found[0])
            save()
            print(role, found[0]['databaseId'], found[0]['url'], flush=True)
            break
        time.sleep(1)
    else:
        raise RuntimeError('Acknowledged attempt not yet identified; poll without redispatch')

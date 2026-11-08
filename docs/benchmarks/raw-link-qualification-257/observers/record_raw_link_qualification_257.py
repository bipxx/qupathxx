from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import shutil
import subprocess

workspace = Path(__file__).resolve().parents[2]
root = workspace / 'build/ci/scientific-grid-159'
out = workspace / 'build/raw-link-qualification-257'
dest = root / 'docs/benchmarks/raw-link-qualification-257'
dest.mkdir(exist_ok=False)

run = json.loads(subprocess.check_output([
    'gh', 'run', 'view', '40422850278', '--repo', 'bipxx/qupathxx',
    '--json', 'status,conclusion,jobs,headSha,url'], text=True))
if run['headSha'] != 'd57df0166888eb335d989d404bdae4d1b63c66a4':
    raise ValueError('wrong Mac source')
(out / 'macos-run-observation.json').write_text(json.dumps(dict(
    observed_at_utc=datetime.now(timezone.utc).isoformat(), run=run), indent=2)
    + '\n', encoding='utf-8')

for source in out.iterdir():
    if source.is_file() and source.suffix in ['.json', '.xml', '.log', '.txt']:
        shutil.copyfile(source, dest / source.name)
for folder in ['history-shutdown', 'active-shutdown']:
    for source in (out / folder).rglob('*'):
        if source.is_file() and source.suffix in ['.json', '.log']:
            target = dest / folder / source.relative_to(out / folder)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
for source in (out / 'history-captures').glob('*.png'):
    target = dest / 'captures' / source.name
    target.parent.mkdir(exist_ok=True)
    shutil.copyfile(source, target)
for name in ['raw_link_qualification_257_backend.py',
             'raw_link_qualification_257_proof.py']:
    target = dest / 'observers' / name
    target.parent.mkdir(exist_ok=True)
    shutil.copyfile(workspace / 'build/ci' / name, target)

readiness = json.loads((out / 'local-readiness.json').read_text())
proof = json.loads((out / 'installed-proof.json').read_text())
campaign = json.loads((out / 'campaign.json').read_text())
summary = dict(
    schema='qpx.staged-qualification-local/1', source=readiness['source'],
    fingerprint=readiness['fingerprint'], frozen_ref=campaign['ref'],
    state='macos_role_running_or_queued', native_suites=51,
    preflight_controls=880,
    installed_workflow=dict(unique_cases=304, passed=301, declared_skips=3,
                            new_history_cases=26),
    typing=dict(positive_files=5, negative_files=4, intended_errors=20),
    stubs_match=True, wheel=dict(bytes=proof['wheel_bytes'],
                                sha256=proof['wheel_sha256']),
    installed_members=len(proof['wheel_members']), source_paths=490,
    previous_products_preserved=64, previous_products_with_authenticated_source=13,
    new_active_products_preserved=55, sdks_unchanged=True,
    captures=dict(count=4, decoded_pixels_identical_to_ppm=True, review=(
        'Historical and retained actions fit. Unverified apply disabled; successful '
        'apply reports 2 annotations and disables repeat apply. Chinese filename '
        'uses fallback glyphs; stored UTF8 name is correct.')),
    macos_run=40422850278, mac_runtime_repair_verified=False,
    engine_role='not_dispatched_until_strict_mac_acceptance',
    portable_role='not_dispatched_until_strict_mac_acceptance',
    dispatch_note=('Initial HTTP422 before branch push completed created no run. '
                   'Ref publication verified, rejection preserved, then one '
                   'accepted Mac-role run identified.'),
    limitations=readiness['limitations'] + [
        'Broader Unicode glyph coverage remains open in M2/UI',
        'All M0-M7 remain Partial; no main promotion or release'])
summary['evidence'] = [dict(
    path=p.relative_to(dest).as_posix(), bytes=p.stat().st_size,
    sha256=hashlib.sha256(p.read_bytes()).hexdigest())
    for p in sorted(dest.rglob('*')) if p.is_file()]
(dest / 'receipt.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
print('Recorded local candidate evidence and original Mac run handle.')

from pathlib import Path
import hashlib
import json
import shutil
import subprocess

root = Path(__file__).resolve().parents[2]
work = root / 'build/ci/scientific-grid-159'
state = root / 'build/stream-header-qualification-281'
observation = root / 'build/q281-all/20261028T173709Z'
public = work / 'docs/benchmarks/stream-header-qualification-281/closeout283'
source = '3f15c2be8722ec6559d8657ecd69a839e1f200a5'
verified = json.loads((observation / 'verified.json').read_text())
assert verified['source'] == source and len(verified['files']) == 11
for entry in verified['files']:
    archive = root / 'build/q281-all' / (entry['name'] + '.zip')
    assert archive.stat().st_size == entry['bytes']
    assert 'sha256:' + hashlib.sha256(archive.read_bytes()).hexdigest() == entry['digest']
for role in ('engine', 'macos', 'portable'):
    result = json.loads((state / (role + '-audit-exit.json')).read_text())
    assert result['returncode'] == 0 and source in result['argv']
frozen = root / 'build/ci/stream-header-qualification-281-audit'
assert subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=frozen, text=True).strip() == source
assert not subprocess.check_output(['git', 'status', '--porcelain'], cwd=frozen)
public.mkdir(exist_ok=False)
files = []
def copy(src, name):
    target = public / name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(src, target)
    assert target.read_bytes() == src.read_bytes()
    files.append(dict(path=name, bytes=target.stat().st_size,
                      sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
for p in state.iterdir():
    if p.is_file() and p.suffix in ('.json', '.log'):
        copy(p, 'audits/' + p.name)
for p in observation.iterdir():
    if p.is_file() and p.suffix == '.json':
        copy(p, 'observation/' + p.name)
for name in ('gui-overview.png', 'gui-overview-benchmark.json', 'gui-overview-benchmark.log'):
    copy(root / 'build/q281-all/Windows-gui-py3.12' / name, 'gui/' + name)
copy(Path(__file__), 'tools/close_candidate281.py')
receipt = dict(schema='qpx.qualification-closeout/1', source=source,
    state='complete-engine-macos-native-portable-engineering-accepted',
    role_runs=dict(engine=40726273936, macos=40726289400, portable=40726306463),
    authenticated_archives=verified['files'], complete_original_auditors=True,
    frozen_checkout_clean=True, gui_overview_visually_inspected=True,
    gui_inspection='Source-specific whole-slide canvas and overview display tissue; toolbar and calibration controls are visible.',
    later_font_source_qualified=False, public_main_promoted=False,
    milestones='M0-M7 remain Partial', scientific_accepted=False,
    general_speed_claim=False, release_ready=False, files=files)
(public / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')

link = '[closeout283](benchmarks/stream-header-qualification-281/closeout283/receipt.json)'
status = f'''**Current qualified feature baseline: candidate281 at `3f15c2b`.**
The {link} authenticates all11 archives and successful complete original-source
engine, Mac-native and portable audits. Its source-specific GUI overview has
been inspected. This supersedes older campaign observations below; public main
is unchanged. The later font source `7231d95` is locally built and passes55 native
suites and five targeted desktop font cases; broader installed/desktop/audit
regressions are running. It inherits no281 platform acceptance. Three strict
scientific expected failures, representative speed/hardware, broader biology,
clean-machine support, licensing/release decisions and all M0-M7 exits remain open.
'''
for name in ('project-plan.md', 'feature-matrix.md', 'api-delivery-plan.md'):
    path = work / 'docs' / name
    text = path.read_text(encoding='utf-8')
    title, rest = text.split('\n', 1)
    path.write_text(title + '\n\n' + status + '\n' + rest.lstrip('\n'), encoding='utf-8')
path = work / 'docs/current-task.md'
text = path.read_text(encoding='utf-8')
start = text.index('**Candidate281 is frozen')
end = text.index('**Font pilot282', start)
text = text[:start] + status + '''
**Font delivery283 local validation is in progress.** The pinned ImGui1.93
core/backends, module-relative OFL CJK/Emoji assets and recoverable atlas cap
are built in native7231d95. All55 native suites and five actual GUI font tests
pass, including real glyph rendering, cached pixels across scale changes,
working-directory independence and missing/corrupt asset repair. The rendered
Unicode capture has been inspected. Full installed cohorts, broader desktop
regressions and1053 expected audit controls are running. These local tests share
test dependencies; no clean-machine, real GPU peak-memory, shaping, bidi or
own-source platform qualification is claimed. Preserve the initial stale-source
configure failure and the later native compatibility compile failure separately.

''' + text[end:]
path.write_text(text, encoding='utf-8')
note = root.parent / 'compilexx/docs/notes/qupathxx-build-inefficiencies.md'
prefix = note.read_bytes()
addition = '''

## Qualification281 closeout and font283 validation (2026-10-28)

All11 existing281 archives authenticate again; complete original frozen-source
engine, Mac-native and portable auditors returned0. Inspect the source-specific
overview and publish one combined closeout instead of rerunning completed roles.
Keep the older initial/failed observations intact. The active plan must lead with
the latest qualified source so historical packet prose cannot obscure next work.
Font283 is a separate changed source:55 native suites and five targeted real-GUI
cases pass, but broader regressions and platform acceptance remain separate.
An inherited FetchContent source override defeated the declared ImGui pin; the
draw-source hash guard caught it before compilation. Set and authenticate the
actual source override before building. A subsequent obsolete ImGui field compile
failure required a one-line adaptation; preserve both failed attempts and final
compiler-input identities rather than claiming a single clean build.
Scientific, representative speed/hardware, support and release gates remain open.
'''
note.write_bytes(prefix + addition.encode('utf-8'))
assert note.read_bytes()[:len(prefix)] == prefix
(state / 'closeout283-compilexx-append-proof.json').write_text(json.dumps(dict(
    prefix_bytes=len(prefix), prefix_sha256=hashlib.sha256(prefix).hexdigest(),
    prefix_unchanged=True), indent=2) + '\n')
print('Published combined281 closeout:', len(files), 'evidence files; preserved all11 archive identities')

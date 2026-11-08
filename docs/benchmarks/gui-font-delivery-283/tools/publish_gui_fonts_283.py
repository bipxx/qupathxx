from pathlib import Path
import hashlib
import json
import shutil
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[2]
work = root / 'build/ci/scientific-grid-159'
out = root / 'build/gui-font-delivery-283'
public = work / 'docs/benchmarks/gui-font-delivery-283'
def counts(name):
    cases = ET.parse(out / (name + '.xml')).findall('.//testcase')
    assert not any(c.find('failure') is not None or c.find('error') is not None for c in cases), name
    return dict(passed=sum(c.find('skipped') is None for c in cases), skipped=sum(c.find('skipped') is not None for c in cases))
results = {n: counts(n) for n in ('native', 'font-gui', 'gui-final-cohort', 'nonrendering-final-cohort', 'desktop-regression', 'preflight')}
assert results == {
    'native': dict(passed=55, skipped=0), 'font-gui': dict(passed=5, skipped=0),
    'gui-final-cohort': dict(passed=395, skipped=1),
    'nonrendering-final-cohort': dict(passed=348, skipped=48),
    'desktop-regression': dict(passed=115, skipped=1), 'preflight': dict(passed=1053, skipped=0)}
installed = json.loads((out / 'installed-proof.json').read_text())
assert installed['source'] == '7231d95c25fbc9515d937036986e8c92b19da261'
assert len(installed['wheel_members']) == 39
assert len(json.loads((out / 'final-products.json').read_text())) == 71
assert json.loads((out / 'typing-admission.json').read_text())['expected_errors'] == 28
public.mkdir(exist_ok=False)
files = []
def copy(src, name):
    target = public / name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(src, target)
    assert target.read_bytes() == src.read_bytes()
    files.append(dict(path=name, bytes=target.stat().st_size,
                      sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
for p in out.iterdir():
    if p.is_file() and p.suffix in ('.json', '.log', '.xml', '.png'):
        copy(p, p.name)
for name in ('validate_gui_fonts_283.py', 'preflight_gui_fonts_283.py', 'retain_gui_fonts_283.py', 'gui_fonts_283_proof.py', 'publish_gui_fonts_283.py'):
    copy(root / 'build/ci' / name, 'tools/' + name)
receipt = dict(schema='qpx.gui-font-delivery/1', native_source=installed['source'],
    source_proof_observer='aa2f565', local_windows_gui_accepted=True,
    counts=results, cohort_counts_overlap=True,
    package_members=39, wheel_record_entries=43, compiler_inputs=415,
    retained_prior_products=69, retained_final_products=71,
    font_assets_and_notices=6, typing_positive_files=7, typing_negative_files=5,
    expected_typing_errors=28, source_paths=len(json.loads((out / 'source-proof.json').read_text())),
    actual_unicode_capture_visually_inspected=True,
    failed_attempts=['build.log: stale FetchContent source override rejected before compilation',
                     'build-source-override.log: obsolete ImGui shared-data fields rejected by compiler'],
    final_build='build-compatibility.log', test_dependencies_shared=True,
    real_gl_peak_storage_measured=False, shaping_or_bidi_accepted=False,
    physical_monitor_dpi_accepted=False, own_source_platform_qualified=False,
    scientific_accepted=False, general_speed_claim=False, release_ready=False, files=files)
(public / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
attribute = work / '.gitattributes'
with attribute.open('a', encoding='utf-8') as stream:
    stream.write('\ndocs/benchmarks/gui-font-delivery-283/** -text -whitespace\n')

for name in ('current-task.md', 'project-plan.md', 'feature-matrix.md', 'api-delivery-plan.md'):
    path = work / 'docs' / name
    text = path.read_text(encoding='utf-8')
    text = text.replace('suites and five targeted desktop font cases; broader installed/desktop/audit\nregressions are running.',
        'suites, five targeted font cases,395 installed GUI passes,348 nonrendering\npasses,115 additional desktop passes and1053 audit controls. See the\n[local font receipt](benchmarks/gui-font-delivery-283/receipt.json). Counts overlap.')
    if name == 'current-task.md':
        start = text.index('**Font delivery283 local validation is in progress.**')
        end = text.index('**Font pilot282', start)
        text = text[:start] + '''**Font delivery283 is locally verified at native7231d95.**
The [receipt](benchmarks/gui-font-delivery-283/receipt.json) binds55 native suites,
five targeted font cases,395 installed GUI passes/one skip,348 nonrendering
passes/48 skips,115 broader desktop passes/one skip and1053 audit controls.
These cohorts overlap. All39 package members,43 wheel records, six new font
assets/notices,415 compiler inputs,71 retained final products and7/5 typing
files with28 expected diagnostics verify. Stubs are unchanged. The Unicode
capture was inspected. Initial configure/compile failures remain preserved.
The orchestration's mixed path-separator filter omitted preflight inputs before
execution; the separate corrected observer ran all20 original files successfully.
This Windows GUI-capable runtime shares test dependencies. Actual GL peak
storage/RSS, physical-monitor DPI, shaping/bidi, actual editor interaction on
this new font runtime and own-source platform qualification remain open.

''' + text[end:]
    if name == 'feature-matrix.md':
        start = text.index('The latest qualified feature baseline is [candidate273]')
        end = text.index('\nBaseline: [QuPath', start)
        text = text[:start] + '''Candidate281 supersedes qualified273 for the declared engineering roles.
Original276 remains terminal/unaccepted; its failure and independent Mac/portable
results are preserved. Candidate281 includes the Unicode-input/wait/header repairs.
Font283 is a later local delivery with the separate acceptance limits above.
''' + text[end:]
    if name == 'project-plan.md':
        start = text.index('Planning update: 2026-10-28 UTC, through engineering-qualified candidate273')
        end = text.index('All **M0–M7 remain Partial**.', start)
        text = text[:start] + '''Planning update: 2026-10-28 UTC, through engineering-qualified candidate281
at3f15c2b. Its combined closeout above accepts the complete original engine,
Mac-native and portable roles and preserves all11 archives. Original276 remains
terminal/unaccepted, while candidate273 remains a historical qualified source.
Later font283 has local Windows acceptance and requires its own platform gate.
Public main is unchanged. Do not repeat completed281 roles for documentation.
''' + text[end:]
    if name == 'api-delivery-plan.md':
        text = text.replace('Candidate281 freezes3f15c2b for complete own-source qualification. Observe engine40726273936, Mac-native40726289400 and portable40726306463; authenticate11 archives and run every complete frozen-source auditor.',
            'Candidate281 at3f15c2b now passes all complete original engine/Mac-native/portable auditors with11 authenticated archives; see closeout283 above. Preserve those completed runs and their source-bound acceptance.')
        text = text.replace('application still has224 default glyphs', 'application delivery283 now has the pinned dynamic fonts and local Windows acceptance above')
        text = text.replace('Qualify the repaired source independently; preserve276 failure', 'Candidate281 qualifies this repair in its complete engineering roles; preserve276 failure')
    path.write_text(text, encoding='utf-8')
path = work / 'docs/gui-font-pipeline.md'
text = path.read_text(encoding='utf-8').replace('The application implementation is now prepared as delivery283, with installed\nvalidation still pending.',
    'The application delivery283 now has [local installed acceptance](benchmarks/gui-font-delivery-283/receipt.json):\n55 native suites, five targeted font cases,395 GUI-mode installed passes,348\nnonrendering passes,115 additional desktop passes and1053 audit controls.\nPackage/font integrity and exact typing controls pass. These counts overlap;\nnew-source platform and remaining acceptance below are still open.')
path.write_text(text, encoding='utf-8')
print('Published local font283 evidence:', len(files), 'files; platform/GL-memory/shaping/release gates remain open', flush=True)

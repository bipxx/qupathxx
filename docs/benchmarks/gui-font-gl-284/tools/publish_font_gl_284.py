from pathlib import Path
import hashlib
import json
import platform
import shutil

root = Path(__file__).resolve().parents[2]
work = root / 'build/ci/scientific-grid-159'
out = root / 'build/gui-font-gl-284'
public = work / 'docs/benchmarks/gui-font-gl-284'
local = json.loads((out / 'receipt-local.json').read_text())
rows = local['sessions']
assert len(rows) == 6 and local['native_source'] == '7231d95c25fbc9515d937036986e8c92b19da261'
assert all(v['live_after_shutdown'] == 0 and v['readbacks'] > 0 for v in rows)
assert all(v['creates'] - v['retired_during_frames'] == v['live_before_shutdown'] for v in rows)
assert max(v['queried_gl_storage_peak_bytes'] for v in rows) == 33030144
assert json.loads((out / 'run-exit.json').read_text())['returncode'] == 0
assert json.loads((out / 'build-exit-attempt2.json').read_text())['returncode'] == 0
identity = json.loads((out / 'input-identity-attempt2.json').read_text())
for e in identity['files']:
    assert hashlib.sha256(Path(e['path']).read_bytes()).hexdigest() == e['sha256'], e['path']
public.mkdir(exist_ok=False)
files = []
def copy(src, name):
    target = public / name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(src, target)
    assert src.read_bytes() == target.read_bytes()
    files.append(dict(path=name, bytes=target.stat().st_size,
                      sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
for p in out.iterdir():
    if p.is_file() and p.suffix in ('.cpp', '.txt', '.json', '.jsonl', '.log'):
        copy(p, p.name)
for p in (out / 'attempt1').iterdir():
    copy(p, 'attempt1/' + p.name)
for name in ('run_font_gl_284.py', 'publish_font_gl_284.py'):
    copy(root / 'build/ci' / name, 'tools/' + name)
receipt = dict(schema='qpx.gui-font-real-gl/1', native_source=local['native_source'],
    state='six-real-opengl-probe-sessions-accepted',
    scope='Retained production GUI/core libraries and production GLFW/OpenGL backends, driven by a standalone resource probe; not a whole-application benchmark.',
    system=platform.platform(), sessions=rows,
    total_exact_texture_readbacks=sum(v['readbacks'] for v in rows),
    total_readback_bytes=sum(v['readback_bytes'] for v in rows),
    maximum_queried_live_texture_storage_bytes=33030144,
    individual_texture_cap_bytes=16777216,
    actual_framebuffer_scales=[dict(name=v['name'],x=v['framebuffer_width']/640,y=v['framebuffer_height']/480) for v in rows],
    window_height_clipped_by_os='maximal: requested1920, observed1901; actual y density1901/480, not4',
    exact_gpu_cpu_pixels=True, every_observed_atlas_deleted=True,
    rss_scope='Point samples of whole-process working set including driver, window buffers and measurement readbacks; neither font-only attribution nor a continuous peak measurement.',
    physical_vram_measured=False, physical_monitor_dpi_accepted=False,
    new_runtime_name_editor_accepted=False,
    name_editor_observation='Computer Use list_windows failed: native pipe unavailable, os error2; same failure after retry and fresh kernel initialization. No app input performed.',
    first_link_failure_preserved=True, production_source_changed=False,
    own_source_platform_qualified=False, scientific_accepted=False,
    general_speed_claim=False, release_ready=False, files=files)
(public / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
attribute = work / '.gitattributes'
with attribute.open('a', encoding='utf-8') as stream:
    stream.write('\ndocs/benchmarks/gui-font-gl-284/** -text -whitespace\n')

path = work / 'docs/current-task.md'
text = path.read_text(encoding='utf-8')
text = text.replace('This Windows GUI-capable runtime shares test dependencies. Actual GL peak\nstorage/RSS, physical-monitor DPI, shaping/bidi, actual editor interaction on\nthis new font runtime and own-source platform qualification remain open.',
    'This Windows GUI-capable runtime shares test dependencies. Real GL atlas\nmeasurements now follow below; physical-monitor DPI, shaping/bidi, actual editor\ninteraction on this new font runtime and own-source platform qualification remain open.')
marker = '**Font pilot282 identifies the next M2 delivery path.**'
addition = '''**Real OpenGL font measurements284 pass on retained native7231d95.**
The [receipt](benchmarks/gui-font-gl-284/receipt.json) drives the production font
loader/backends through six standalone GL sessions on NVIDIA RTX4090/driver610.88.
All25 texture readbacks match CPU atlas pixels, and every observed atlas object
is absent after shutdown. Queried live atlas storage peaks at33,030,144 bytes
(31.5MiB) during quota growth; individual textures stay at or below16MiB.
The1,600-supported-glyph stress case loads1,429 and recovers with fallback.
Normal/repeated-normal storage is262,144 bytes. These are scope-specific resource
measurements, not a whole-app speed or physical VRAM claim. Whole-process working
set is sampled separately and includes driver/readback buffers; immutable font
assets persist across windows. Synthetic densities do not qualify physical DPI;
the OS clipped the maximal window to2560x1901 rather than2560x1920.
An initial probe link omitted core/SHA dependencies; the retained core library
and bcrypt resolve it without rebuilding any product. Actual Name-editor input
could not run because Computer Use's native pipe remained unavailable after
retry/reset; no app input occurred. Keep that gate open and continue other work.

'''
assert marker in text
path.write_text(text.replace(marker, addition + marker, 1), encoding='utf-8')

path = work / 'docs/gui-font-pipeline.md'
text = path.read_text(encoding='utf-8')
text = text.replace('The application preserves supplementary UTF8 input but its224-glyph default font\nstill displays placeholders.',
    'The old application preserved supplementary UTF8 input but its224-glyph font\ndisplayed placeholders. Delivery283 now packages dynamic international fonts\nand has local Windows acceptance; [real GL measurements284](benchmarks/gui-font-gl-284/receipt.json)\nadd exact texture readback and cleanup evidence on the retained production runtime.')
text = text.replace('Measure real GL allocations,\ntransient behavior and RSS; do not infer them from a CPU protocol simulation.',
    'Real GL probe284 now queries33,030,144 bytes of peak live atlas storage in its\nquota case, authenticates25 complete texture readbacks and checks cleanup across\nsix sessions. Whole-process working set is sampled with driver/readback overhead.\nThis measures GL channel storage, not physical VRAM or font-only RSS; the probe\nuses synthetic densities and its maximal window height is OS-clipped. Broader\nhardware and actual monitor delivery remain open.')
path.write_text(text, encoding='utf-8')

for name in ('project-plan.md','feature-matrix.md','api-delivery-plan.md'):
    path = work / 'docs' / name
    text = path.read_text(encoding='utf-8')
    marker = 'clean-machine support, licensing/release decisions and all M0-M7 exits remain open.'
    assert marker in text
    text = text.replace(marker, marker + '''
Real-GL [probe284](benchmarks/gui-font-gl-284/receipt.json) now verifies25 exact
texture readbacks and cleanup across six sessions of retained font source7231d95.
Peak queried live atlas storage is31.5MiB during quota growth; this is one-system
texture storage evidence. Physical DPI, new-runtime Name editing and source-bound
platform acceptance remain open.
''', 1)
    if name == 'api-delivery-plan.md':
        text = text.replace('Actual GL texture/update/budget/lifecycle evidence, cache/immediate pixel identity, repeated DPI/window delivery, resource corruption/missing-file controls and actual editor/save/reopen.',
            'Local283 verifies cache/immediate pixels and missing/corrupt resources; real-GL284 verifies texture updates/readback/growth/cleanup on one system. Next: actual editor/save/reopen when the Windows helper is available, physical monitor/DPI delivery and one complete own-source platform qualification.')
    path.write_text(text, encoding='utf-8')

note = root.parent / 'compilexx/docs/notes/qupathxx-build-inefficiencies.md'
prefix = note.read_bytes()
addition = '''

## Font GL284: retain binaries, measure actual protocol, preserve dependency closure (2026-10-28)

Link a standalone resource probe against retained7231d95 GUI/core libraries and
the existing production GLFW/OpenGL backend. No product rebuild or relabeling.
The first link omitted core/SHA and bcrypt; preserve the failure and carry the
real transitive static-library dependencies instead of treating a .lib path as
a CMake target with inherited dependencies. Six real-GL sessions pass,25 complete
readbacks equal CPU pixels and all observed atlas names are deleted at shutdown.
Peak queried live atlas storage is31.5MiB during quota growth; per texture<=16MiB.
Do not equate queried channel storage with physical VRAM. RSS samples include
driver/window/readback costs and persistent font bytes, not font-only memory.
Record delivered dimensions: the OS clipped a requested1920-high window to1901.
Synthetic density controls do not prove physical monitor transitions. Computer
Use's native pipe failed again after retry/reset; no app inputs were performed.
Keep actual Name-editor and physical-DPI checks open, while continuing independent
platform/scientific work instead of repeating successful resource measurements.
'''
note.write_bytes(prefix + addition.encode('utf-8'))
assert note.read_bytes()[:len(prefix)] == prefix
(out / 'compilexx-append-proof.json').write_text(json.dumps(dict(prefix_bytes=len(prefix),
    prefix_sha256=hashlib.sha256(prefix).hexdigest(), prefix_unchanged=True), indent=2) + '\n')
print('Published real-GL284:',len(files),'evidence files;',receipt['total_exact_texture_readbacks'],'readbacks; no product rebuild',flush=True)

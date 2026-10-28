"""UI scaling exercises real font textures, cached overlays and routed input."""
import os
from pathlib import Path
import shutil
import subprocess
import sys

import pytest
import qupathxx as q
from test_gui_docking import config, workspace
from test_gui_tiles import replay

pytestmark = pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI') != '1', reason='opt-in native DPI scaling')


def unicode_document(label):
    doc=q.Document(q.Image(256,160,[35,60,90]*256*160))
    doc.add_rectangle(q.Rect(40,40,160,80),label)
    return doc


def unicode_options(budget=64*1024*1024):
    options=q.GuiOptions();options.width,options.height=900,700
    options.docked_workspace=False;options.settle_tiles=True;options.autosave_interval_seconds=0
    options.overlay_cache_budget=budget;options.replay_timeout_seconds=15
    options.inputs=[replay(3,q.GuiInputKind.SELECT_ALL_KEY)]
    return options


def test_unicode_labels_render_actual_glyphs_instead_of_question_marks(tmp_path):
    frames=[]
    for index,label in enumerate(['A_\u96ea\U0002000b\U0001f52c','A_???']):
        doc=unicode_document(label);capture=tmp_path/f'name-{index}.ppm'
        report=q.show(doc,frame_limit=16,capture_path=str(capture),options=unicode_options())
        assert report.tiles_settled and report.canvas_visible and report.primary_selection==1
        assert doc.annotation(1).name==label
        frames.append(capture.read_bytes())
    assert frames[0]!=frames[1], 'Unicode labels still render as fallback question marks'


def test_unicode_dynamic_fonts_preserve_cached_pixels_across_scale_changes(tmp_path):
    frames=[];reports=[]
    for budget in [0,64*1024*1024]:
        doc=unicode_document('A_\u03b1\u539f\u59cb\u96ea_\U0002000b\U0001f52c\U0001f469')
        options=unicode_options(budget)
        options.inputs += [replay(6,q.GuiInputKind.SET_UI_SCALE,2),replay(11,q.GuiInputKind.SET_UI_SCALE,1.25)]
        capture=tmp_path/f'cached-{budget}.ppm'
        report=q.show(doc,frame_limit=24,capture_path=str(capture),options=options)
        assert report.replay_complete and report.tiles_settled and report.font_rebuilds==3
        frames.append(capture.read_bytes());reports.append(report)
    assert frames[0]==frames[1]
    assert reports[1].overlay_cache_hits>0 and reports[1].overlay_cache_builds>=3


def test_packaged_fonts_are_independent_of_working_directory(tmp_path):
    decoy=tmp_path/'fonts';decoy.mkdir()
    for name in ['NotoSansCJKsc-Regular.otf','NotoEmoji.ttf']:(decoy/name).write_bytes(b'not a font')
    code="""import qupathxx as q
doc=q.Document(q.Image.demo());doc.add_rectangle(q.Rect(1,1,20,20),'A_\u96ea\U0002000b\U0001f52c')
options=q.GuiOptions();options.width,options.height=640,480;options.docked_workspace=False;options.settle_tiles=True
report=q.show(doc,frame_limit=8,options=options)
assert report.tiles_settled and report.canvas_visible
print('module-relative fonts verified')
"""
    result=subprocess.run([sys.executable,'-I','-X','utf8','-c',code],cwd=tmp_path,capture_output=True,text=True,encoding='utf-8',timeout=45)
    assert result.returncode==0 and 'module-relative fonts verified' in result.stdout,result.stdout+result.stderr


@pytest.mark.parametrize('fault',['missing','corrupt'])
def test_packaged_fonts_reject_corrupt_assets_and_recover(tmp_path,fault):
    source=Path(q.__file__).parent;package=tmp_path/'isolated'/'qupathxx';package.mkdir(parents=True)
    for path in source.iterdir():
        if path.is_file() and (path.suffix in {'.py','.pyd','.dll'} or '.so' in path.name):shutil.copy2(path,package/path.name)
    shutil.copytree(source/'fonts',package/'fonts')
    target=package/'fonts'/'NotoSansCJKsc-Regular.otf'
    if fault=='missing':target.unlink()
    else:
        with target.open('r+b') as stream:
            first=stream.read(1);stream.seek(0);stream.write(bytes([first[0]^64]))
    code="""import sys,shutil
from pathlib import Path
assert 'qupathxx' not in sys.modules
sys.path.insert(0,sys.argv[1])
import qupathxx as q
assert Path(q.__file__).parent==Path(sys.argv[1])/'qupathxx'
doc=q.Document(q.Image.demo());obj=doc.add_rectangle(q.Rect(1,1,20,20),'Preserved_\u96ea')
revision=doc.revision;options=q.GuiOptions();options.width,options.height=640,480;options.docked_workspace=False;options.settle_tiles=True
try:q.show(doc,frame_limit=8,options=options)
except RuntimeError as error:
    assert ('Missing GUI font asset' if sys.argv[3]=='missing' else 'GUI font asset checksum mismatch') in str(error),str(error)
else:raise AssertionError('invalid packaged font was accepted')
assert doc.revision==revision and doc.annotation(obj).name=='Preserved_\u96ea'
shutil.copy2(sys.argv[2],Path(q.__file__).parent/'fonts'/'NotoSansCJKsc-Regular.otf')
assert q.show(doc,frame_limit=8,options=options).tiles_settled
assert doc.revision==revision and doc.annotation(obj).name=='Preserved_\u96ea'
print('packaged font rejected and recovered')
"""
    result=subprocess.run([sys.executable,'-I','-X','utf8','-c',code,str(package.parent),str(source/'fonts'/'NotoSansCJKsc-Regular.otf'),fault],
        cwd=tmp_path,capture_output=True,text=True,encoding='utf-8',timeout=45)
    assert result.returncode==0 and 'packaged font rejected and recovered' in result.stdout,result.stdout+result.stderr


@pytest.mark.parametrize('multiplier', [.75, 1., 1.5, 2.])
def test_scaled_workspace_keeps_camera_and_routes_image_input(tmp_path, multiplier):
    project, doc = workspace(tmp_path)
    generation = project.generation
    options = config()
    options.ui_scale = multiplier
    options.inputs = [replay(4, q.GuiInputKind.MOVE, 440, 330, target=q.GuiInputTarget.IMAGE),
                      replay(5, q.GuiInputKind.LEFT_DOWN, 440, 330, target=q.GuiInputTarget.IMAGE),
                      replay(6, q.GuiInputKind.LEFT_UP, 440, 330, target=q.GuiInputTarget.IMAGE)]
    report = q.show(project, frame_limit=14, options=options)
    assert report.tiles_settled and report.replay_complete and report.canvas_visible
    assert report.ui_scale == pytest.approx(report.monitor_scale/report.framebuffer_scale*multiplier)
    assert report.font_rebuilds == 1
    assert report.primary_selection == 1 and report.selected_ids == [1]
    assert report.final_center.x == pytest.approx(440) and report.final_center.y == pytest.approx(330)
    assert report.final_zoom == 1.5
    assert project.generation == generation and not project.dirty and doc.object_count == 1


def test_live_scale_changes_invalidate_cache_without_camera_drift(tmp_path):
    project, _ = workspace(tmp_path)
    generation = project.generation
    captures, reports = [], []
    for budget in [0, 64*1024*1024]:
        options = config()
        options.overlay_cache_budget = budget
        options.inputs = [replay(3, q.GuiInputKind.SELECT_ALL_KEY),
                          replay(6, q.GuiInputKind.SET_UI_SCALE, 1.5),
                          replay(10, q.GuiInputKind.SET_UI_SCALE, 1.5),
                          replay(13, q.GuiInputKind.SET_UI_SCALE, 1),
                          replay(17, q.GuiInputKind.SET_UI_SCALE, 1.25)]
        capture = tmp_path/f'cache-{budget}.ppm'
        report = q.show(project, frame_limit=27, capture_path=str(capture), options=options)
        assert report.font_rebuilds == 4  # Repeating the current scale does no GL work.
        assert report.final_center.x == pytest.approx(440) and report.final_center.y == pytest.approx(330)
        assert report.final_zoom == 1.5 and report.tiles_settled and report.replay_complete
        reports.append(report)
        captures.append(capture.read_bytes())
    assert captures[0] == captures[1]
    assert reports[1].overlay_cache_hits > 0 and reports[1].overlay_cache_builds >= 4
    assert project.generation == generation and not project.dirty


def test_small_successive_changes_accumulate_against_built_font_scale(tmp_path):
    project, _ = workspace(tmp_path)
    baseline = q.show(project, frame_limit=4, options=config()).ui_scale
    options = config()
    options.inputs = [replay(frame, q.GuiInputKind.SET_UI_SCALE, 1+delta/baseline)
                      for frame, delta in [(3, .0006), (5, .0012), (7, .0018), (9, .0024)]]
    report = q.show(project, frame_limit=14, options=options)
    assert report.font_rebuilds == 3
    assert report.ui_scale == pytest.approx(baseline+.0024, abs=1e-6)


def test_compact_scaled_resize_and_live_scale_preserve_camera(tmp_path):
    project, _ = workspace(tmp_path)
    generation = project.generation
    options = config()
    options.docked_workspace = False
    options.ui_scale = 2
    options.inputs = [replay(3, q.GuiInputKind.RESIZE_WINDOW, 1100, 700),
                      replay(7, q.GuiInputKind.SET_UI_SCALE, 1.25),
                      replay(11, q.GuiInputKind.MOVE, 440, 330, target=q.GuiInputTarget.IMAGE),
                      replay(12, q.GuiInputKind.LEFT_DOWN, 440, 330, target=q.GuiInputTarget.IMAGE),
                      replay(13, q.GuiInputKind.LEFT_UP, 440, 330, target=q.GuiInputTarget.IMAGE)]
    report = q.show(project, frame_limit=20, options=options)
    assert not report.docked_workspace and report.font_rebuilds == 2
    assert report.tiles_settled and report.replay_complete and report.primary_selection == 1
    assert report.final_center.x == pytest.approx(440) and report.final_center.y == pytest.approx(330)
    assert report.final_zoom == 1.5 and project.generation == generation and not project.dirty


@pytest.mark.parametrize('value', [0, .49, 3.01, float('inf'), float('nan')])
def test_invalid_scale_rejected_before_opening_window(value):
    options = q.GuiOptions()
    options.ui_scale = value
    with pytest.raises(ValueError, match='UI scale'):
        q.show(q.Document(q.Image.demo()), frame_limit=2, options=options)
    options.ui_scale = 1
    options.inputs = [replay(0, q.GuiInputKind.SET_UI_SCALE, value)]
    with pytest.raises(ValueError, match='UI scale'):
        q.show(q.Document(q.Image.demo()), frame_limit=2, options=options)


@pytest.mark.parametrize('native', [False, True])
def test_launchers_accept_scaled_empty_workspace(native):
    if native and sys.platform != 'win32':
        pytest.skip('Windows standalone GUI')
    command = [str(Path(q.__file__).parent/'qupathxx_app.exe')] if native else [sys.executable, '-I', '-m', 'qupathxx']
    subprocess.run(command+['--empty', '--smoke', '--ui-scale', '1.5'], check=True, capture_output=True, timeout=45)

"""Activity selection, hidden dock chrome and a slide-owned toolbar on real GL."""
import os
import re

import pytest
import qupathxx as q
from test_gui_docking import config, workspace
from test_gui_tiles import replay

pytestmark = pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI') != '1', reason='opt-in native activity workspace')


@pytest.mark.parametrize('index,key', [(0,'project'), (1,'image'), (2,'annotations'), (3,'workflow')])
@pytest.mark.parametrize('scale', [1,2])
def test_activity_click_changes_and_persists_without_touching_research_state(tmp_path,index,key,scale):
    project,doc = workspace(tmp_path)
    before = (tmp_path/'source.qpxproj').read_bytes()
    generation = project.generation
    options = config(tmp_path/'activity.ini')
    options.width, options.height, options.ui_scale = 1100, 800, scale
    # The rail uses fixed logical targets below the menu, independent of panel
    # captions, translated text lengths or dock split positions.
    x, y = 24*scale, (28+48*index+24)*scale
    options.inputs = [replay(4,q.GuiInputKind.LEFT_DOWN,x/options.width,y/options.height,target=q.GuiInputTarget.WINDOW),
                      replay(5,q.GuiInputKind.LEFT_UP,x/options.width,y/options.height,target=q.GuiInputTarget.WINDOW)]
    report = q.show(project,frame_limit=16,options=options)
    assert report.replay_complete and report.canvas_visible and report.tiles_settled
    text = (tmp_path/'activity.ini').read_text(encoding='utf8')
    assert f'; qupathxx sidebar={key}\n' in text
    # The default leaf nodes contain primary panes. The root must allow tool
    # tabs/close buttons on additional docks instead of hiding them globally.
    nodes = [line for line in text.splitlines() if re.match(r'\s*DockNode\s+ID=',line)]
    assert nodes and all('NoTabBar=1' in line and 'NoWindowMenuButton=1' in line for line in nodes)
    assert report.ui_overflow_items == 0, report.ui_overflow_label
    assert report.final_center.x == pytest.approx(440) and report.final_center.y == pytest.approx(330) and report.final_zoom == 1.5
    assert project.generation == generation and not project.dirty and doc.object_count == 1
    assert (tmp_path/'source.qpxproj').read_bytes() == before
    options.inputs = []
    q.show(project,frame_limit=10,options=options)
    assert f'; qupathxx sidebar={key}\n' in (tmp_path/'activity.ini').read_text(encoding='utf8')


def test_toolbar_is_inside_slide_and_stays_there_after_resize(tmp_path):
    project,_ = workspace(tmp_path)
    options = config(tmp_path/'toolbar.ini')
    options.inputs = [replay(4,q.GuiInputKind.RESIZE_WINDOW,1100,700)]
    report = q.show(project,frame_limit=16,options=options)
    assert report.replay_complete and report.canvas_visible and report.tiles_settled
    # A child toolbar belongs to the slide; it cannot stay over the sidebar when
    # the splitter or the main window changes size.
    text = (tmp_path/'toolbar.ini').read_text(encoding='utf8')
    slide = re.search(r'\[Window\]\[Slide\]\s+Pos=(\d+),(\d+)\s+Size=(\d+),(\d+)',text)
    assert slide
    x,y,width,height = map(int,slide.groups())
    assert x >= report.browser_bounds.x+report.browser_bounds.width
    assert y <= report.browser_bounds.y+1 and height >= report.browser_bounds.height-1
    assert width > 400 and report.ui_overflow_items == 0

"""Unavailable GUI entry points keep the same typed argument/return contract."""
import pytest
import qupathxx as q


@pytest.mark.skipif(q.has_gui, reason='headless wheel contract')
def test_all_show_overloads_raise_gui_unavailable_without_starting_work(tmp_path):
    doc = q.Document(q.Image(4, 4, [32]*4*4*3))
    project = q.Project()
    session = q.ViewerSession(str(tmp_path/'not-opened.qpxproj'))
    for target in [doc, project, session]:
        with pytest.raises(RuntimeError, match='QUPATHXX_BUILD_GUI=ON'):
            q.show(target, frame_limit=2, capture_path='', options=q.GuiOptions())
    assert session.stats.state == 'created' and doc.object_count == 0

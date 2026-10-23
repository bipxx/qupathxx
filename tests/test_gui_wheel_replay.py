"""OS wheel messages must not change a deterministic replay's camera."""
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import pytest
import qupathxx as q
from test_gui_window import workspace,window
from test_gui_tiles import replay

pytestmark=pytest.mark.skipif(os.name!='nt' or not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='Windows GUI wheel-message regression')


def test_native_wheel_messages_are_isolated_but_scripted_zoom_is_preserved(tmp_path):
    project,_,_,_=workspace(tmp_path);ready=tmp_path/'sender-ready';sent=tmp_path/'sender-sent.json'
    sender=subprocess.Popen([sys.executable,'-I',str(Path(__file__).with_name('gui_wheel_sender.py')),str(os.getpid()),'1040','660',str(ready),str(sent)],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    try:
        deadline=time.monotonic()+5
        while not ready.exists() and sender.poll() is None and time.monotonic()<deadline:time.sleep(.005)
        assert ready.exists(),'native wheel sender did not start'
        options=q.GuiOptions();options.autosave_interval_seconds=0
        move=replay(6,q.GuiInputKind.MOVE,.5,.5);move.wait_for=q.GuiInputWait.WINDOW_READY
        wheel=replay(10,q.GuiInputKind.WHEEL,.5,.5,wheel=1)
        options.inputs=[replay(2,q.GuiInputKind.MOVE,.5,.5),window(4,q.GuiInputKind.RESIZE_WINDOW,1040,660),move,wheel]
        report=q.show(project,frame_limit=1000,options=options)
        out,error=sender.communicate(timeout=25)
        assert sender.returncode==0,(out,error)
        assert json.loads(sent.read_text(encoding='utf-8'))['messages']==3
        assert report.replay_complete and report.final_zoom==pytest.approx(2.4)
        assert report.replay_physical_wheel_events_ignored>=3
    finally:
        if sender.poll() is None:sender.terminate()
        sender.communicate(timeout=5)

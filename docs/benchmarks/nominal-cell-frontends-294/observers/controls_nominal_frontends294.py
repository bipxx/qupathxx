from pathlib import Path
import hashlib
import json
import sys
from PIL import Image
import qupathxx as q

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/nominal-cell-frontends-294'
sys.path.insert(0,str(work/'tests'))
from test_nominal_cell_python import scene
from test_nominal_cell_frontends import sampling,pinned
from test_gui_replay import event
phase=sys.argv[1];assert phase in ('probe','probe2','toggle','toggle2','toggle3')
doc,obj,_,p=scene();options=q.GuiOptions();options.cell_nominal=sampling();options.grid_cell_parameters=p
options.width=1100;options.height=800;options.docked_workspace=False;options.settle_tiles=True;options.autosave_interval_seconds=0
scroll=event(8,q.GuiInputKind.WHEEL,q.GuiInputTarget.WINDOW);scroll.x=.24;scroll.y=.5;scroll.wheel=-26
events=[event(4,q.GuiInputKind.SELECT_ALL_KEY),scroll]
if phase.startswith('toggle'):
    contours=p.contours;contours.work_bytes=16*1024*1024;p.contours=contours;options.grid_cell_parameters=p
    for frame,y in [(42,float(sys.argv[2])),(46,float(sys.argv[3]))]:
        for offset,kind in [(0,q.GuiInputKind.LEFT_DOWN),(1,q.GuiInputKind.LEFT_UP)]:
            click=event(frame+offset,kind,q.GuiInputTarget.WINDOW);click.x=.025;click.y=y/800;events.append(click)
    events.extend([event(50,q.GuiInputKind.DETECT_CELLS_KEY),event(52,q.GuiInputKind.MOVE,wait=q.GuiInputWait.CELLS_READY)])
options.inputs=events
path=out/('nominal-controls-'+phase+'.ppm');assert not path.exists()
report=q.show(doc,frame_limit=80,capture_path=str(path),options=options)
(out/('controls-'+phase+'-telemetry.json')).write_text(json.dumps(dict(cell_state=report.cell_state,cell_error=report.cell_error,selected_ids=report.selected_ids,replay_complete=report.replay_complete,replay_frame=report.replay_frame,frames=report.frames),indent=2)+'\n',encoding='utf-8')
if phase.startswith('toggle'):
    assert report.cell_state=='ready' and not report.cell_error
    pinned(report.cell_result)
    assert report.cell_result.nominal_sampling.context_x==report.cell_result.nominal_sampling.context_y==32
png=path.with_suffix('.png')
with Image.open(path) as original:
    original.save(png)
    with Image.open(png) as encoded:assert original.size==encoded.size and original.tobytes()==encoded.tobytes()
(out/('controls-'+phase+'-proof.json')).write_text(json.dumps(dict(native_source=q.build_environment().git_revision,phase=phase,png=png.name,
    ppm_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),png_sha256=hashlib.sha256(png.read_bytes()).hexdigest(),
    original_document=True,pinned_detection_after_mode_switch=phase.startswith('toggle'),lossless=True),indent=2)+'\n',encoding='utf-8')
print('Nominal controls',phase,'captured',flush=True)

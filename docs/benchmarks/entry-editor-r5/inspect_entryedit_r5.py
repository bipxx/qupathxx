from pathlib import Path
import json
import qupathxx as q
r=Path(r'C:\Users\HEQ\Projects\qupathxx');o=r/'build/entry-edit-r5'
source=json.loads((o/'compiler-inputs.json').read_bytes())['source'];assert q.build_environment().git_revision==source
projects=list((o/'installed-temp').rglob('desktop.qpxproj'));assert len(projects)==1
project=q.Project.open(str(projects[0]));image=project.images[0]
options=q.GuiOptions();options.width=1500;options.height=1400;options.project_image_path=image.source_path
inputs=[]
for frame,kind in [(20,q.GuiInputKind.LEFT_DOWN),(21,q.GuiInputKind.LEFT_UP)]:
 e=q.GuiInput();e.frame=frame;e.kind=kind;e.target=q.GuiInputTarget.WINDOW;e.x=140/1500;e.y=565/1400;inputs.append(e)
options.inputs=inputs
report=q.show(project,frame_limit=45,capture_path=str(o/'installed-editor.ppm'),options=options)
assert report.replay_complete
(o/'editor-replay.json').write_text(json.dumps(dict(source=source,frames=report.frames,replay_complete=report.replay_complete,click=dict(x=140,y=565,width=1500,height=1400),purpose='Open editor from observed installed sidebar; visual capture, no field editing'),indent=2))

from pathlib import Path
import json
import qupathxx as q
r=Path(r'C:\Users\HEQ\Projects\qupathxx');o=r/'build/entry-edit-r5'
source=json.loads((o/'compiler-inputs.json').read_bytes())['source'];assert q.build_environment().git_revision==source
projects=list((o/'installed-temp').rglob('desktop.qpxproj'));assert len(projects)==1
project=q.Project.open(str(projects[0]));image=project.images[0]
options=q.GuiOptions();options.width=1500;options.height=1400;options.project_image_path=image.source_path
before=dict(image.metadata);description=image.description;tags=image.tags;assert '' in before
inputs=[]
for frame,x,y in [(20,140,565),(30,590,630),(38,490,819),(46,645,424)]:
 for tick,kind in [(frame,q.GuiInputKind.LEFT_DOWN),(frame+1,q.GuiInputKind.LEFT_UP)]:
  e=q.GuiInput();e.frame=tick;e.kind=kind;e.target=q.GuiInputTarget.WINDOW;e.x=x/1500;e.y=y/1400;inputs.append(e)
e=q.GuiInput();e.frame=55;e.kind=q.GuiInputKind.SAVE_KEY;inputs.append(e)
options.inputs=inputs
report=q.show(project,frame_limit=70,capture_path=str(o/'installed-edited.ppm'),options=options)
assert report.replay_complete
expected=dict(before);del expected['']
restored=q.Project.open(str(projects[0]));after=restored.images[0]
assert after.metadata==expected,(after.metadata,expected)
assert after.description==description and after.tags==tags
print('Installed real mouse removal/Apply/close/Ctrl+S/reopen retained exact map and independent details.')
(o/'editor-authoring.json').write_text(json.dumps(dict(source=source,frames=report.frames,replay_complete=report.replay_complete,click=dict(x=140,y=565,width=1500,height=1400),purpose='Observed-coordinate editor removal/Apply/close/save/reopen; exact metadata and independent details verified'),indent=2))

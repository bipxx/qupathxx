from pathlib import Path
import json,qupathxx as q
from PIL import Image
out=Path(__file__).resolve().parent
source='8722d96d788a550945e15b7aa3e32e29423b9a54'
assert q.build_environment().git_revision==source
path=out/'installed-class-editor.qpxproj';project=q.Project.open(str(path))
assert project.images==[] and project.class_defaults==['Parent','Parent: Positive','Parent: Positive']
classes={c.name:(c.parent,c.color_argb,c.color) for c in project.classes}
options=q.GuiOptions();options.width=1500;options.height=1400;events=[]
# Coordinates observed from the installed panel, including its selected-class row.
for frame,x,y in [(20,100,456),(30,100,479),(38,60,501),(46,65,755),(54,72,882)]:
 for tick,kind in [(frame-3,q.GuiInputKind.MOVE),(frame,q.GuiInputKind.LEFT_DOWN),(frame+1,q.GuiInputKind.LEFT_UP)]:
  e=q.GuiInput();e.frame=tick;e.kind=kind;e.target=q.GuiInputTarget.WINDOW;e.x=x/1500;e.y=y/1400;events.append(e)
e=q.GuiInput();e.frame=64;e.kind=q.GuiInputKind.SAVE_KEY;events.append(e);options.inputs=events
report=q.show(project,frame_limit=90,capture_path=str(out/'installed-class-editor.ppm'),options=options)
expected=['Parent','Parent: Positive','Parent','Parent: Positive']
observed=project.class_defaults
print('Observed defaults:',observed,flush=True)
assert report.replay_complete and observed==expected
restored=q.Project.open(str(path));assert restored.images==[] and restored.class_defaults==expected
assert {c.name:(c.parent,c.color_argb,c.color) for c in restored.classes}==classes
Image.open(out/'installed-class-editor.ppm').save(out/'installed-class-editor.png')
(out/'editor-authoring.json').write_text(json.dumps(dict(source=source,replay_complete=report.replay_complete,frames=report.frames,images=0,defaults=expected,classes=classes,purpose='Installed mouse selection, append duplicate default, reorder, Ctrl+S and reopen; exact palette and raw class state verified'),indent=2)+'\n',encoding='utf-8')
print('Installed class defaults mouse edit/save/reopen passed')

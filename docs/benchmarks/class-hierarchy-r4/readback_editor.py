from pathlib import Path
import json,qupathxx as q
from PIL import Image
out=Path(__file__).resolve().parent
assert q.build_environment().git_revision=='8722d96d788a550945e15b7aa3e32e29423b9a54'
project=q.Project.open(str(out/'installed-class-editor.qpxproj'))
assert project.images==[] and project.class_defaults==['Parent','Parent: Positive','Parent','Parent: Positive']
assert {c.name:c.color_argb for c in project.classes}=={'Parent':0x80123456,'Parent: Positive':None}
options=q.GuiOptions();options.width=1500;options.height=1400;events=[]
for frame,x,y in [(20,100,456),(30,100,479),(38,60,501)]:
 for tick,kind in [(frame-3,q.GuiInputKind.MOVE),(frame,q.GuiInputKind.LEFT_DOWN),(frame+1,q.GuiInputKind.LEFT_UP)]:
  e=q.GuiInput();e.frame=tick;e.kind=kind;e.target=q.GuiInputTarget.WINDOW;e.x=x/1500;e.y=y/1400;events.append(e)
options.inputs=events
report=q.show(project,frame_limit=48,capture_path=str(out/'installed-class-editor-readback.ppm'),options=options)
assert report.replay_complete and not project.dirty
Image.open(out/'installed-class-editor-readback.ppm').save(out/'installed-class-editor-readback.png')
(out/'editor-readback.json').write_text(json.dumps(dict(source=q.build_environment().git_revision,replay_complete=report.replay_complete,dirty=project.dirty,defaults=project.class_defaults,images=0,purpose='Fresh-process installed project readback and rendered registry/default inspection after mouse edit/save'),indent=2)+'\n',encoding='utf-8')
print('Fresh-process installed class readback: exact defaults/raw colors, clean project')

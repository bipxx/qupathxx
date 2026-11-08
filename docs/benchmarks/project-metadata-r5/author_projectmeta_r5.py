from pathlib import Path
import qupathxx as q,json
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/project-metadata-r5';source=json.loads((o/'compiler-inputs.json').read_bytes())['source'];assert q.build_environment().git_revision==source
reference=json.loads((w/'tests/fixtures/qupath-project-metadata-0.7.0/reference.json').read_bytes());rich=next(row['metadata'] for row in reference if row['name']=='rich')
target=o/'installed-authoring.qpxproj';project=q.Project.open(str(target));assert project.metadata==rich and project.images==[];options=q.GuiOptions();options.width=1500;options.height=1400
options.inputs=[];events=[]
for frame,x,y in [(20,100,110),(30,590,630),(38,490,819),(46,660,424)]:
 for tick,kind in [(frame-3,q.GuiInputKind.MOVE),(frame,q.GuiInputKind.LEFT_DOWN),(frame+1,q.GuiInputKind.LEFT_UP)]:
  e=q.GuiInput();e.frame=tick;e.kind=kind;e.target=q.GuiInputTarget.WINDOW;e.x=x/1500;e.y=y/1400;events.append(e)
e=q.GuiInput();e.frame=55;e.kind=q.GuiInputKind.SAVE_KEY;events.append(e)
options.inputs=events
report=q.show(project,frame_limit=70,capture_path=str(o/'installed-project-edited.ppm'),options=options);assert report.replay_complete
expected=dict(rich);del expected['']
restored=q.Project.open(str(target));assert project.metadata==restored.metadata==expected and restored.images==[]
import hashlib
(o/'editor-authoring.json').write_text(json.dumps(dict(source=source,replay_complete=report.replay_complete,frames=report.frames,images=len(project.images),removed_keys=[''],remaining_pairs=len(restored.metadata),saved_metadata_sha256=hashlib.sha256(json.dumps(restored.metadata,sort_keys=True,ensure_ascii=False).encode()).hexdigest(),purpose='Installed actual mouse remove/Apply/close/Ctrl+S/reopen on zero-image project; exact map verified'),indent=2))

print('Installed actual mouse authoring/save/reopen: exact zero-image global map verified.')

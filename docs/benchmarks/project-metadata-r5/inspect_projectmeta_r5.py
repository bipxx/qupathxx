from pathlib import Path
import qupathxx as q,json
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/project-metadata-r5';source=json.loads((o/'compiler-inputs.json').read_bytes())['source'];assert q.build_environment().git_revision==source
reference=json.loads((w/'tests/fixtures/qupath-project-metadata-0.7.0/reference.json').read_bytes());rich=next(row['metadata'] for row in reference if row['name']=='rich')
project=q.Project();project.set_metadata(rich);target=o/'installed-authoring.qpxproj';project.save(str(target));options=q.GuiOptions();options.width=1500;options.height=1400
options.inputs=[];events=[]
for frame,kind in [(16,q.GuiInputKind.MOVE),(20,q.GuiInputKind.LEFT_DOWN),(21,q.GuiInputKind.LEFT_UP)]:
 e=q.GuiInput();e.frame=frame;e.kind=kind;e.target=q.GuiInputTarget.WINDOW;e.x=100/1500;e.y=110/1400;events.append(e)
options.inputs=events
report=q.show(project,frame_limit=45,capture_path=str(o/'installed-project-editor.ppm'),options=options);assert report.replay_complete and project.metadata==rich and project.images==[]
(o/'editor-replay.json').write_text(json.dumps(dict(source=source,replay_complete=report.replay_complete,frames=report.frames,images=len(project.images),purpose='Installed zero-image project sidebar button opens shared project-mode editor; visual inspection required'),indent=2))

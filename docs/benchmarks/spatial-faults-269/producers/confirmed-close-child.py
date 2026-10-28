
import json,sys
import qupathxx as q
doc=q.Document(q.Image(64,64,[80]*64*64*3));roi=q.Roi.points([q.Point(32,32)])
doc.execute([q.ObjectEdit.create_geometry(roi) for _ in range(20000)])
ids=[a.id for a in doc.annotations];project=q.Project();project.add_document(doc,'Close during analysis');project.save(sys.argv[1])
o=q.GuiOptions();o.width=1280;o.height=1000;o.autosave_interval_seconds=0
spatial=q.GuiSpatialOptions();spatial.open=spatial.run=True;spatial.sources=spatial.targets=ids
analysis=spatial.analysis;analysis.max_distance_tests=1<<40;spatial.analysis=analysis;o.spatial=spatial
e=q.GuiInput();e.frame=8;e.kind=q.GuiInputKind.CLOSE_WINDOW;down=q.GuiInput();down.frame=10;down.kind=q.GuiInputKind.LEFT_DOWN;down.target=q.GuiInputTarget.DISCARD_CLOSE;down.wait_for=q.GuiInputWait.TARGET_READY
up=q.GuiInput();up.frame=11;up.kind=q.GuiInputKind.LEFT_UP;up.target=q.GuiInputTarget.DISCARD_CLOSE
o.inputs=[e,down,up]
report=q.show(project,frame_limit=60,options=o)
print(json.dumps(dict(replay_complete=report.replay_complete,state=report.spatial.state,busy=report.spatial.busy,rows=report.spatial.rows,frames=report.frames)))
assert report.replay_complete and report.spatial.state=='cancelled' and not report.spatial.busy
assert report.spatial.result is None and doc.spatial_record_count==0
print(json.dumps(dict(state=report.spatial.state,distance_tests=report.spatial.analysis.distance_tests)))

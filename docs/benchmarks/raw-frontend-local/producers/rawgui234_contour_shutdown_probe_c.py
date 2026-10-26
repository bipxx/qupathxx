from pathlib import Path
import hashlib,json,sys,time
import qupathxx as q
r=Path(__file__).resolve().parents[2];s=r/'build/ci/scientific-grid-159';out=r/'build/rawgui234-final/contour-shutdown-c';out.mkdir(exist_ok=False);sys.path.insert(0,str(s/'tests'))
from test_gui_raw_pixel import scene,model_for
from test_viewer_session import run,completed
results=[]
for action in ('cancel','close'):
 directory=out/action;directory.mkdir();p,image,doc,ids,root,settings=scene(directory);model=model_for(doc,ids,settings);admitted=q.RawPixelPredictionRequest(doc,model,q.ScientificGrid(8,6,q.ScientificGridAxis(0,128,112,2),q.ScientificGridAxis(0,96,80,2),64*1024**2),root,work_bytes=64*1024**2);del admitted;path=directory/'source.qpx';p.save(str(path));doc.server.close();original=hashlib.sha256(path.read_bytes()).hexdigest()
 session=q.ViewerSession(str(path));seen={}
 def producer():
  grid=q.ScientificGrid(8,6,q.ScientificGridAxis(0,128,112,2),q.ScientificGridAxis(0,96,80,2),64*1024**2)
  ticket=session.contour_raw_pixels(image,model,grid,object_id=root,apply=True,work_bytes=64*1024**2)
  deadline=time.monotonic()+15
  while session.poll(ticket).state=='queued':
   assert time.monotonic()<deadline;time.sleep(.0002)
  assert session.poll(ticket).state=='running';seen['observed_running']=True
  if action=='cancel':assert session.cancel(ticket)
  else:closing=session.close(True)
  event=session.wait(ticket,20);(directory/'terminal.json').write_text(json.dumps(dict(state=event.state,error=event.error,committed=event.committed),indent=2),encoding='utf-8');assert event.state=='cancelled' and not event.committed and event.raw_pixel_contours is None,(event.state,event.error,event.committed);session.take(ticket)
  if action=='cancel':completed(session,session.close(True))
  else:completed(session,closing)
  assert session.stats.retained_bytes==0 and session.stats.objects==3
 report=run(session,producer,frames=2000)
 assert seen['observed_running']
 assert hashlib.sha256(path.read_bytes()).hexdigest()==original
 source=directory/'输入.tif';moved=directory/'released.tif';source.rename(moved);assert moved.is_file()
 results.append(dict(action=action,observed_running=True,state='cancelled',committed=False,objects=3,source_project_unchanged=True,reader_released=True,frames=report.frames))
(out/'proof.json').write_text(json.dumps(dict(source=q.build_environment().git_revision,fingerprint=q.build_environment().source_tree_sha256,cases=results),indent=2)+'\n',encoding='utf-8');print('Managed contour cancellation and active close: 2 cases passed; no mutation, retained results, or borrowed reader after shutdown')

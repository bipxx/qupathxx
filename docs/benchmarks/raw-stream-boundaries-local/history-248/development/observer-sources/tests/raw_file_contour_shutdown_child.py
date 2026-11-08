"""Isolated actual running historical/replay job shutdown observation."""
import gc,json,sys
from pathlib import Path
import qupathxx as q
mode,project_path,file_path,receipt=sys.argv[1:]
limits=q.RawPixelStreamFileLimits();limits.work_bytes=2*1024**2
file=q.RawPixelStreamFile(file_path,limits)
options=q.RawPixelStreamContourOptions();options.work_bytes=64*1024**2;options.boundary_work_bytes=4*1024**2;contour=options.contours;contour.work_bytes=4*1024**2;options.contours=contour
project=None;doc=None
if mode.startswith('replay'):
    file.verify();project=q.Project.open(project_path);doc=project.document(project.images[0].id);request=q.capture_raw_pixel_stream_replay(doc,file);job=q.RawPixelStreamContourJob();assert job.request_replay(request,file,options);stats=job.poll(doc)
else:
    job=q.RawPixelStreamFileReadJob();assert job.trace(file,options);stats=job.poll()
assert job.busy and stats.state=='running'
observed={'mode':mode,'observed_busy':True,'observed_state':stats.state,'runtime_source':q.build_environment().git_revision}
if mode.endswith('cancel'):
    job.cancel();status=job.wait(doc) if doc is not None else job.wait();assert status.state=='cancelled'
else:
    del job;gc.collect()
if doc is not None:doc.server.close()
file=None;project=None;doc=None;gc.collect();observed['shutdown_completed']=True
Path(receipt).write_text(json.dumps(observed,indent=2)+'\n',encoding='utf-8')

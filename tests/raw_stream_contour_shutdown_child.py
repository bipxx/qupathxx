"""Real active-worker shutdown probe; invoked only by its parent regression."""
from pathlib import Path
import gc
import json
import sys
import qupathxx as q

mode,image,model_path=sys.argv[1:]
model=q.load_raw_pixel_classifier(model_path)
server=q.open_tiff(image);doc=q.Document(server)
lattice=q.ScientificRawLattice(1024,768,q.ScientificGridAxis(0,1024,896,2),q.ScientificGridAxis(0,768,640,2))
stream=q.RawPixelStreamOptions();stream.tile_width=stream.tile_height=16;stream.work_bytes=16*1024**2
capture=q.RawPixelTiledPredictionRequest(doc,model,lattice,stream)
options=q.RawPixelStreamContourOptions();options.work_bytes=32*1024**2;options.boundary_work_bytes=2*1024**2
contour=q.TissueContourOptions();contour.work_bytes=2*1024**2;options.contours=contour
job=q.RawPixelStreamContourJob()
started=job.request(capture,options)
if not started:raise RuntimeError('worker did not start')
busy=job.busy;status=job.poll(doc)
if not busy or status.state!='running':raise RuntimeError('probe did not observe a live worker')
environment=q.build_environment()
record={'mode':mode,'observed_busy':busy,'observed_state':status.state,'produced_windows':status.produced_windows,'total_windows':capture.total_windows,'runtime_source':environment.git_revision}
# Leave the controller as the remaining owner of the reader capture before its
# native cancel/join and Python shared-pointer cleanup. JobHolder releases GIL.
del capture,doc,server;gc.collect()
if mode=='cancel':job.cancel()
elif mode!='close':raise ValueError('unknown shutdown probe')
del job;gc.collect()
record['shutdown_completed']=True
print(json.dumps(record))

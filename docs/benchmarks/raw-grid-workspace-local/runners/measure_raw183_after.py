from pathlib import Path
import json,statistics,sys,time
import numpy as np
import tifffile
import qupathxx as q
r=Path(__file__).resolve().parents[2];out=r/'build/raw183-owner-after';out.mkdir(exist_ok=False)
source=r/'build/raw183-owner-before/uniform.tif'
p=q.Project();image=p.add_tiff_image(str(source));doc=p.document(image);obj=doc.add_rectangle(q.Rect(0,0,32,24),'Fixed owner cost fixture')
rows=[]
for nx,ny in [(64,64),(256,256),(512,512),(65536,1)]:
 counts=q.RawGridSampling();counts.samples_x=nx;counts.samples_y=ny
 request=q.RawAnalysisRequest.for_grid_roi(doc,obj,counts)
 start=time.perf_counter_ns();result=q.analyze_raw(request);analysis=time.perf_counter_ns()-start
 assert result.statistics.pixels==nx*ny and result.statistics.means==[1.]
 keep=[];verify=[]
 for _ in range(7):
  start=time.perf_counter_ns();record=doc.keep_raw_analysis(request,result);keep.append(time.perf_counter_ns()-start)
  start=time.perf_counter_ns();assert doc.verify_raw_analysis(record,request,result);verify.append(time.perf_counter_ns()-start)
  doc.remove_raw_analysis(record)
 rows.append(dict(samples=[nx,ny],analysis_ns=analysis,keep_ns=keep,verify_ns=verify,median_keep_ns=statistics.median(keep),median_verify_ns=statistics.median(verify)))
env=q.build_environment();data=dict(schema='qpx.raw-owner-observation/1',compiled_source=env.git_revision,fingerprint=env.source_tree_sha256,scope='Local observation on shared unreserved workstation; no hardware/UI budget acceptance',rows=rows)
(out/'results.json').write_bytes((json.dumps(data,indent=2)+'\n').encode('utf-8'))
for row in rows:print(row['samples'],'keep median ms',row['median_keep_ns']/1e6,'verify median ms',row['median_verify_ns']/1e6)

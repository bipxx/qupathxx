from pathlib import Path
import json,statistics,time
import qupathxx as q
r=Path(__file__).resolve().parents[2];out=r/'build/raw183-capture-after.json';assert not out.exists()
server=q.open_tiff(str(r/'build/raw183-owner-before/uniform.tif'));doc=q.Document(server);obj=doc.add_rectangle(q.Rect(0,0,32,24),'Fixed owner capture fixture')
rows=[]
for nx,ny in [(64,64),(256,256),(512,512),(65536,1),(1000000,1)]:
 counts=q.RawGridSampling();counts.samples_x=nx;counts.samples_y=ny;values=[]
 for _ in range(7):
  start=time.perf_counter_ns();request=q.RawAnalysisRequest.for_grid_roi(doc,obj,counts);values.append(time.perf_counter_ns()-start)
 rows.append(dict(samples=[nx,ny],capture_ns=values,median_capture_ns=statistics.median(values)))
e=q.build_environment();out.write_bytes((json.dumps(dict(compiled_source=e.git_revision,fingerprint=e.source_tree_sha256,scope='Shared unreserved workstation observation; no hardware/UI budget acceptance',rows=rows),indent=2)+'\n').encode('utf-8'))
for row in rows:print(row['samples'],'capture median ms',row['median_capture_ns']/1e6)

"""Fixed ten-minute stability acceptance on the declared whole-slide tissue route."""
import ctypes as c
from ctypes import wintypes as w
import gc,hashlib,json,sys,time
from pathlib import Path
import qupathxx as q
class Counters(c.Structure):
 _fields_=[('cb',w.DWORD),('faults',w.DWORD)]+[(n,c.c_size_t) for n in ('peak_rss','rss','peak_paged','paged','peak_nonpaged','nonpaged','pagefile','peak_pagefile','private')]
k=c.WinDLL('kernel32');p=c.WinDLL('psapi');k.GetCurrentProcess.restype=w.HANDLE
p.GetProcessMemoryInfo.argtypes=[w.HANDLE,c.POINTER(Counters),w.DWORD]
def memory():
 v=Counters();v.cb=c.sizeof(v)
 if not p.GetProcessMemoryInfo(k.GetCurrentProcess(),c.byref(v),v.cb):raise c.WinError()
 return dict(rss=v.rss,private=v.private,peak_rss=v.peak_rss)
server=q.open_slide(sys.argv[1]);doc=q.Document(server);obj=doc.add_rectangle(q.Rect(0,0,doc.metadata.width,doc.metadata.height))
params=q.TissueParameters();params.level=2;params.tile_size=256;params.canonical_pyramid=True;params.work_bytes=128*1024*1024
request=q.TissueRequest(doc,obj,q.StainTransform([[.65,.70,.29],[.07,.99,.11]]),params)
rows=[];expected=None
begin=time.monotonic();i=0
while time.monotonic()-begin < 600 or i<32:
 start=time.perf_counter();result=q.detect_tissue(request)
 wall=(time.perf_counter()-start)*1000
 identity=(result.input_sha256,hashlib.sha256(result.labels).hexdigest(),result.foreground_pixels,result.component_count)
 if expected is None:expected=identity
 assert identity==expected,(i,identity)
 del result;gc.collect();rows.append(dict(iteration=i,elapsed_s=time.monotonic()-begin,wall_ms=wall,**memory()));i+=1
 if i%100==0:print("repeat",i,"elapsed",round(time.monotonic()-begin),flush=True)
assert doc.tissue_record_count==0
server.close()
# Predeclared bounded criterion after decoder/allocator warmup: no >32MiB drift.
drift=max(r['private'] for r in rows[8:])-min(r['private'] for r in rows[8:])
report=dict(source_tree=q.build_environment().source_tree_sha256,iterations=i,duration_s=time.monotonic()-begin,stable_identity=expected,private_range_after_8_bytes=drift,private_range_limit_bytes=32*1024*1024,runs=rows,scope='Ten-minute fixed-workload acceptance, exact labels each iteration and no durable record accumulation; not a universal leak proof or level-zero cell stream')
Path(sys.argv[2]).write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in report.items() if k!='runs'},indent=2))
assert drift<=32*1024*1024,drift

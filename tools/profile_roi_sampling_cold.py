import argparse,hashlib,json,math,statistics,time
from pathlib import Path
import qupathxx as q
from qupathxx import _native
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args()
policy=q.RoiSamplingPolicy.POLYGON_WINDING
cases=[]
for n in [4096,16384]:
 ring=[q.Point(math.cos(2*math.pi*i/n),math.sin(2*math.pi*i/n)) for i in range(n)]
 for label,point,expected in [('outside-envelope',q.Point(2,2),False),('outside-corner',q.Point(.95,.95),False),('inside',q.Point(0,0),True)]:
  samples=[]
  for i in range(10):
   start=time.perf_counter_ns();roi=q.Roi.polygon(ring);made=time.perf_counter_ns()
   value=roi.contains_sample(point,policy);queried=time.perf_counter_ns()
   again=roi.contains_sample(point,policy);repeated=time.perf_counter_ns()
   assert value==again==expected
   samples.append(dict(construction_ms=(made-start)/1e6,first_query_ms=(queried-made)/1e6,second_query_ms=(repeated-queried)/1e6))
  cases.append(dict(vertices=n,query=label,expected=expected,excluded=samples[:2],samples=samples[2:]))
e=q.build_environment()
r=dict(schema='qpx.sampling-first-query-observation/1',performance_accepted=False,native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),source_tree_sha256=e.source_tree_sha256,git_revision=e.git_revision,script_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),cases=cases,scope='Two discarded fresh-geometry observations and eight retained observations per shape/query. Geometry construction timed separately; first and repeated native winding queries have identical required truth values. Shared-workstation microbenchmark, no general hardware or RSS acceptance.')
a.output.write_bytes((json.dumps(r,indent=2)+'\n').encode('utf-8'))

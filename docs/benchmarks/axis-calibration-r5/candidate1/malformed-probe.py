from pathlib import Path
import sys,json,struct,tempfile
sys.path.insert(0,str(Path.cwd()/'tests'))
import qupathxx as q
from test_image_axis_calibration import foreign
root=Path(sys.argv[1]);results=[]
for case in ['missing-time-unit','unknown-time-unit','time-unit-type','time-array-type','time-element-type','time-overflow','too-many-source-times','unknown-z-unit','zero-z']:
    trial=Path(tempfile.mkdtemp(prefix='bad-axis-',dir=root));path=foreign(trial,'nanoseconds')
    data=path.parent/'data/1/data.qpdata';raw=data.read_bytes();assert raw[27]==0x74
    size=struct.unpack('>H',raw[28:30])[0];header=json.loads(raw[30:30+size].decode());cal=header['server']['metadata']['pixelCalibration']
    if case=='missing-time-unit':del cal['timeUnit']
    elif case=='unknown-time-unit':cal['timeUnit']='FORTNIGHTS'
    elif case=='time-unit-type':cal['timeUnit']=1
    elif case=='time-array-type':cal['timepoints']={}
    elif case=='time-element-type':cal['timepoints']=['NaN']
    elif case=='time-overflow':cal['timeUnit']='DAYS';cal['timepoints']=[1e308]
    elif case=='too-many-source-times':cal['timepoints']=[0]*5
    elif case=='unknown-z-unit':cal['zSpacing']['unit']='mm'
    else:cal['zSpacing']['value']=0
    encoded=json.dumps(header).encode();assert len(encoded)<65536
    data.write_bytes(raw[:28]+struct.pack('>H',len(encoded))+encoded+raw[30+size:])
    try:q.preview_qupath_project(str(path))
    except ValueError as error:results.append(dict(case=case,refused=True,error=str(error)))
    else:results.append(dict(case=case,refused=False))
(root/'malformed-results.json').write_text(json.dumps(dict(source=q.build_environment().git_revision,results=results),indent=2),encoding='utf-8')
print('malformed refusal controls',sum(x['refused'] for x in results),'of',len(results))

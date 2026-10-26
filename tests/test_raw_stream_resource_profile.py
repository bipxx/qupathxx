"""Fresh-process resource observations must prove coverage and their declared guard."""
import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace
import pytest
ROOT=Path(__file__).resolve().parents[1]
p=ROOT/'tools/profile_raw_stream_contours.py'
spec=importlib.util.spec_from_file_location('raw_resource_profile',p);profile=importlib.util.module_from_spec(spec);exec(compile(p.read_bytes(),str(p),'exec'),profile.__dict__)
CONTRACT=json.loads(profile.CONTRACT.read_text())


def rows():
    result=[]
    for pid,n in enumerate(CONTRACT['sizes'],1):
        result.append(dict(size=n,pid=pid,runtime_source='same',runtime_fingerprint='same',package='same',completed_samples=n*n,valid_samples=n*n,class_counts=[n*n//2,n*n//2],completed_windows=(n//128)**2,expected_windows=(n//128)**2,exact_rectangles=True,geometry=[dict(label=i,pixels=n*n//2,area=n*n/2,perimeter=3*n) for i in [1,2]],payload_bytes=2048,estimated_work_bytes=1048576,reader=dict(peak_cache_bytes=32768,decoded_bytes=n*n*2),trace_ms=1,memory_final=dict(peak_working_set_bytes=32*1024**2),memory_before_trace=dict(peak_working_set_bytes=16*1024**2)))
    return result


def test_complete_matching_fresh_process_observations():
    profile.check(CONTRACT,rows())


@pytest.mark.parametrize('change',['missing','duplicate','same_process','mixed_runtime','error','samples','counts','windows','geometry','payload','admission','cache','decode','nan_time','peak','decreasing_peak','spread'])
def test_resource_false_acceptance_controls(change):
    data=rows();row=data[-1]
    if change=='missing':data.pop()
    elif change=='duplicate':data[-1]=copy.deepcopy(data[0])
    elif change=='same_process':row['pid']=data[0]['pid']
    elif change=='mixed_runtime':row['runtime_source']='other'
    elif change=='error':row['error']='native prediction failed'
    elif change=='samples':row['completed_samples']-=1
    elif change=='counts':row['class_counts']=[row['size']**2,0]
    elif change=='windows':row['completed_windows']-=1
    elif change=='geometry':row['geometry'][0]['area']-=1
    elif change=='payload':row['payload_bytes']=0
    elif change=='admission':row['estimated_work_bytes']=CONTRACT['contour_work_bytes']+1
    elif change=='cache':row['reader']['peak_cache_bytes']=CONTRACT['reader_cache_bytes']+1
    elif change=='decode':row['reader']['decoded_bytes']+=1
    elif change=='nan_time':row['trace_ms']=float('nan')
    elif change=='peak':row['memory_final']['peak_working_set_bytes']=CONTRACT['process_peak_bytes']+1
    elif change=='decreasing_peak':row['memory_final']['peak_working_set_bytes']=1
    elif change=='spread':row['memory_final']['peak_working_set_bytes']=128*1024**2
    with pytest.raises(ValueError):profile.check(CONTRACT,data)


def geometry(points):
    return SimpleNamespace(paths=[[SimpleNamespace(x=x,y=y) for x,y in points]])


def test_authored_corners_are_order_independent_but_reject_shift_and_holes():
    expected=geometry([(128,256),(0,256),(0,0),(128,0)])
    assert profile.matches_rectangle(expected,1,256)
    assert not profile.matches_rectangle(expected,2,256)
    shifted=geometry([(1,0),(129,0),(129,256),(1,256)])
    assert not profile.matches_rectangle(shifted,1,256)
    expected.paths.append(expected.paths[0])
    assert not profile.matches_rectangle(expected,1,256)


@pytest.mark.skipif(sys.platform!='win32',reason='actual Windows CLI preflight')
def test_existing_work_destination_is_preserved(tmp_path):
    sentinel=tmp_path/'sentinel';sentinel.write_bytes(b'original')
    done=subprocess.run([sys.executable,'-I',str(p),'--work',str(tmp_path)],capture_output=True,text=True,timeout=15)
    assert done.returncode!=0 and 'work destination already exists' in done.stderr
    assert sentinel.read_bytes()==b'original' and sorted(v.name for v in tmp_path.iterdir())==['sentinel']


@pytest.mark.parametrize('flag',['-O','-OO'])
def test_optimized_execution_cannot_admit_missing_observations(flag):
    code="import importlib.util;from pathlib import Path;p=Path("+repr(str(p))+");s=importlib.util.spec_from_file_location('optimized_resource',p);m=importlib.util.module_from_spec(s);exec(compile(p.read_bytes(),str(p),'exec'),m.__dict__);m.check({'sizes':[256]},[])"
    done=subprocess.run([sys.executable,flag,'-I','-c',code],capture_output=True,text=True,timeout=15)
    assert done.returncode!=0 and 'missing/duplicate size' in done.stderr

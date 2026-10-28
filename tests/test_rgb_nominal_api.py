"""Installed native/Python/CLI RGB nominal reads against captured pixel hashes."""
import hashlib
import os
from pathlib import Path
import subprocess
import numpy as np
import pytest
import qupathxx as q

FIXTURE=Path(__file__).resolve().parent/'fixtures/rgb-nominal-v1'
CASES=[line.split() for line in (FIXTURE/'index.txt').read_text().splitlines()[1:]]


def source_pixels(case):
    return np.frombuffer((FIXTURE/case[1]).read_bytes(),dtype=np.uint8).reshape(int(case[3]),int(case[2]),3)


@pytest.mark.parametrize('case',CASES,ids=[v[0] for v in CASES])
def test_python_public_read_matches_captured_rgba_and_owns_output(case):
    doc=q.Document(q.Image.from_numpy(source_pixels(case)))
    request=q.RgbNominalRequest(*map(int,case[4:8]),float(case[8]))
    result=q.read_rgb_nominal_region(doc.server,request)
    doc.server.close()
    view=result.numpy()
    assert (result.width,result.height)==(int(case[9]),int(case[10]))
    assert hashlib.sha256(view.tobytes()).hexdigest()==case[11]
    assert not view.flags.writeable
    assert request.algorithm=='qpx.rgb-nominal/1'


@pytest.mark.parametrize('case',CASES,ids=[v[0] for v in CASES])
def test_native_region_cli_matches_all_captured_rgba(case,tmp_path):
    executable=Path(q.__file__).parent/('qupathxx_region.exe' if os.name=='nt' else 'qupathxx_region')
    assert executable.is_file()
    source=tmp_path/'input with spaces.ppm'
    data=b'P6\n'+str(case[2]).encode()+b' '+str(case[3]).encode()+b'\n255\n'+source_pixels(case).tobytes()
    source.write_bytes(data)
    output=tmp_path/'output.pam'
    command=[str(executable),'--rgb-nominal',str(source),*case[4:9],str(output)]
    process=subprocess.run(command,capture_output=True,timeout=30)
    assert process.returncode==0,process.stderr
    header,pixels=output.read_bytes().split(b'ENDHDR\n',1)
    assert b'WIDTH '+case[9].encode()+b'\n' in header
    assert b'HEIGHT '+case[10].encode()+b'\n' in header
    assert hashlib.sha256(pixels).hexdigest()==case[11]
    assert source.read_bytes()==data


@pytest.mark.parametrize('name',['x_px','y_px','width_px','height_px','downsample','algorithm'])
def test_request_is_immutable_with_explicit_pixel_units(name):
    request=q.RgbNominalRequest(x_px=3,y_px=5,width_px=24,height_px=20,downsample=1.1)
    assert (request.x_px,request.y_px,request.width_px,request.height_px,request.downsample)==(3,5,24,20,1.1)
    with pytest.raises(AttributeError):setattr(request,name,1)


@pytest.mark.parametrize('step',[0,-1,float('nan'),float('inf'),1e-320,100])
def test_invalid_ratio_or_empty_output_rejects_at_request_construction(step):
    with pytest.raises(ValueError):q.RgbNominalRequest(0,0,2,2,step)


@pytest.mark.parametrize('bounds',[(0,0,0,1),(0,0,1,-1),(2**63-1,0,2,1),(0,0,67108864,2)])
def test_invalid_extent_and_overflow_reject_at_construction(bounds):
    with pytest.raises(ValueError):q.RgbNominalRequest(*bounds,1)


def test_owned_outside_pixels_and_closed_reader_behavior():
    server=q.Document(q.Image.demo()).server
    request=q.RgbNominalRequest(-50,-50,2,2)
    result=q.read_rgb_nominal_region(server,request)
    np.testing.assert_array_equal(result.numpy(),np.broadcast_to([0,0,0,255],(2,2,4)))
    server.close()
    with pytest.raises(RuntimeError,match='closed'):q.read_rgb_nominal_region(server,request)
    assert result.data==bytes([0,0,0,255])*4


@pytest.mark.parametrize('ratio',['0','-1','nan','inf','0x1p0','1junk','1,5',' 1'])
def test_cli_rejects_invalid_ratio_without_output(tmp_path,ratio):
    executable=Path(q.__file__).parent/('qupathxx_region.exe' if os.name=='nt' else 'qupathxx_region')
    output=tmp_path/'invalid.pam'
    process=subprocess.run([str(executable),'--rgb-nominal',str(tmp_path/'must-not-open.ppm'),
        '0','0','2','2',ratio,str(output)],capture_output=True,timeout=30)
    assert process.returncode==1
    assert b'downsample' in process.stderr and not output.exists()


def test_cli_protects_original_source_and_rejects_output_admission(tmp_path):
    executable=Path(q.__file__).parent/('qupathxx_region.exe' if os.name=='nt' else 'qupathxx_region')
    source=tmp_path/'source.ppm';data=b'P6\n2 1\n255\n'+bytes([10,20,30,110,120,130]);source.write_bytes(data)
    command=[str(executable),'--rgb-nominal',str(source),'0','0','2','1','1',str(source)]
    process=subprocess.run(command,capture_output=True,timeout=30)
    assert process.returncode==1 and b'overwrite' in process.stderr
    assert source.read_bytes()==data
    command[-1]=str(tmp_path/'invalid.pam');command[-2]='1e-300'
    assert subprocess.run(command,capture_output=True,timeout=30).returncode==1
    assert not Path(command[-1]).exists() and source.read_bytes()==data

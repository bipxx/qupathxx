"""Installed standalone native raw exporter, including Unicode argv on Windows."""
import json
import os
from pathlib import Path
import subprocess
import numpy as np
import pytest
import qupathxx as q
from qupathxx import _native
import tifffile

pytestmark=pytest.mark.skipif(not q.has_tiff,reason='TIFF reader')
CLI=Path(_native.__file__).with_name('qupathxx_region.exe' if os.name=='nt' else 'qupathxx_region')


def run(*args):
    return subprocess.run([str(CLI),*map(str,args)],capture_output=True,text=True,encoding='utf-8',timeout=40)


@pytest.mark.parametrize('dtype',['uint16','int64','uint64','float64'])
def test_native_raw_export_exact_and_existing_output_preserved(tmp_path,dtype):
    values=np.arange(2*3*2*19*23,dtype='uint64').reshape(2,3,2,19,23).astype(dtype)
    if dtype=='uint64':values[1,2,1,0,0]=2**64-1
    path=tmp_path/'量化.ome.tif';output=tmp_path/'原始.npy'
    if dtype in ['int64','uint64']:
        tifffile.imwrite(path,values[1,2,1],ome=False,photometric='minisblack',tile=(16,16),compression='deflate',metadata=None)
        axes=[]
    else:
        tifffile.imwrite(path,values,ome=True,photometric='minisblack',tile=(16,16),compression='deflate',metadata={'axes':'TZCYX'})
        axes=['--z',2,'--time',1,'--channel',1]
    args=['--raw',path,output,*axes,'--width',23,'--height',19]
    done=run(*args);assert done.returncode==0,done.stderr
    receipt=json.loads(done.stdout);assert receipt['dtype']==dtype and receipt['valid_pixels']==19*23
    np.testing.assert_array_equal(np.load(output)[...,0],values[1,2,1])
    before=output.read_bytes();assert run(*args).returncode!=0 and output.read_bytes()==before
    assert not list(tmp_path.glob('*.qpx-tmp-*'))


def test_input_and_hardlink_cannot_be_overwritten(tmp_path):
    source=tmp_path/'input.tif';tifffile.imwrite(source,np.ones((8,8),dtype='uint16'),metadata=None)
    alias=tmp_path/'alias.npy';os.link(source,alias);before=source.read_bytes()
    for output in [source,alias]:
        done=run('--raw',source,output)
        assert done.returncode!=0 and source.read_bytes()==before


@pytest.mark.parametrize('flag,value',[('--z','-1'),('--channels','0'),('--width','999999999'),('--level','9'),('--unknown','1')])
def test_invalid_arguments_never_create_output(tmp_path,flag,value):
    source=tmp_path/'input.tif';tifffile.imwrite(source,np.ones((8,8),dtype='uint16'),metadata=None)
    output=tmp_path/'bad.npy';done=run('--raw',source,output,flag,value)
    assert done.returncode==2 and not output.exists() and not list(tmp_path.glob('*.qpx-tmp-*'))

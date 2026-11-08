"""Public native workflow is usable with and without the optional Python caller."""
import json
from pathlib import Path
import subprocess

import pytest
import qupathxx as q
from test_raw_pixel_python import tiff


@pytest.mark.skipif(not q.has_native_workflows or not q.has_tiff or not q.has_classifier_training, reason="native project/raw training")
def test_cpp_cli_and_python_interface_preserve_scientific_identity(tmp_path):
    pixels=tmp_path/'raw.tif';tiff(pixels)
    project=q.Project();image=project.add_image(str(pixels),'tiff');doc=project.document(image)
    doc.add_detection(q.Roi.rectangle(q.Rect(0,0,3,6)),class_name='A')
    doc.add_detection(q.Roi.rectangle(q.Rect(4,0,4,6)),class_name='B')
    source=tmp_path/'source.qpx';project.save_new(str(source));doc.server.close();before=source.read_bytes()
    direct=json.loads(q.run_raw_classifier_workflow(str(source),str(tmp_path/'interface'),image,2))
    binary=Path(q.__file__).with_name('qupathxx_batch.exe')
    child=subprocess.run([str(binary),'--raw-classifier-workflow',str(source),str(tmp_path/'cpp'),str(image),'--channels','2'],capture_output=True,text=True,encoding='utf-8',timeout=60)
    assert child.returncode==0,child.stdout+child.stderr
    native=json.loads(child.stdout)
    for field in ('rows','windows','model_sha256','stream_sha256'):assert native[field]==direct[field]
    assert native['rows']==36 and native['windows']==4 and source.read_bytes()==before
    witness=(tmp_path/'cpp/model.qrm').read_bytes()
    with pytest.raises(FileExistsError):q.run_raw_classifier_workflow(str(source),str(tmp_path/'cpp'),image,2)
    assert (tmp_path/'cpp/model.qrm').read_bytes()==witness and source.read_bytes()==before


@pytest.mark.skipif(not q.has_native_workflows,reason="native workflows")
@pytest.mark.parametrize('image,channels,error',[(True,1,TypeError),(1,True,TypeError),(-1,1,ValueError),(1,2**64,ValueError)])
def test_native_workflow_rejects_invalid_interface_numbers_before_files(tmp_path,image,channels,error):
    with pytest.raises(error):q.run_raw_classifier_workflow(str(tmp_path/'missing'),str(tmp_path/'new'),image,channels)
    assert not (tmp_path/'new').exists()

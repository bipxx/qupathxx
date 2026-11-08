"""The original failing test binary must survive a later producer failure."""
import hashlib
import importlib.util
import json
from pathlib import Path
from types import SimpleNamespace

import pytest


@pytest.mark.parametrize('failure', ['macos-native-inventory', 'native-tests'])
def test_original_test_program_retained_before_failure(tmp_path, monkeypatch, failure):
    path=Path(__file__).resolve().parents[1]/'tools/ci/run_macos_native.py'
    spec=importlib.util.spec_from_file_location('qpx_mac_failure_capture',path)
    producer=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(producer)
    work=tmp_path/'attempt'
    original=b'original compiled test program\x00\xff'
    monkeypatch.setattr(producer.platform,'system',lambda:'Darwin')
    monkeypatch.setattr(producer.platform,'machine',lambda:'arm64')
    monkeypatch.setattr(producer.sys,'argv',['producer','--work',str(work),'--architecture','arm64'])
    monkeypatch.setenv('GITHUB_SHA','a'*40)
    commands=[]
    def run(arguments, **kwargs):
        command=list(map(str,arguments));commands.append(command)
        if any('build_opencv_sdk.py' in value for value in command):
            sdk=work/'opencv-sdk';sdk.mkdir()
            for name in ['OpenCVConfig.cmake','OpenCVConfig-version.cmake']:
                (sdk/name).write_text('# test SDK config\n')
        if command[:2]==['cmake','--build']:
            build=work/'native-build';build.mkdir()
            (build/'qupathxx_pixel_workflow_tests').write_bytes(original)
        if command[0]=='ctest' or any('macos_native_inventory.py' in value for value in command):
            retained=work/'qupathxx_pixel_workflow_tests'
            assert retained.read_bytes()==original
            manifest=json.loads((work/'pixel-workflow-product.json').read_text())
            assert manifest['sha256']==hashlib.sha256(original).hexdigest()
            assert manifest['bytes']==len(original) and manifest['commit']=='a'*40
            assert manifest['architecture']=='arm64'
            assert 'not test acceptance' in manifest['scope']
        fails=(failure=='native-tests' and command[0]=='ctest') or (
            failure=='macos-native-inventory' and any('macos_native_inventory.py' in value for value in command))
        if fails:kwargs['stdout'].write('injected producer failure\n')
        return SimpleNamespace(returncode=int(fails))
    monkeypatch.setattr(producer.subprocess,'run',run)
    # The producer prepends tools while importing the existing SDK helper.
    monkeypatch.syspath_prepend(str(path.parents[1]))
    with pytest.raises(RuntimeError,match=failure+' failed'):
        producer.main()
    assert (work/'qupathxx_pixel_workflow_tests').read_bytes()==original
    assert (work/'native-build/qupathxx_pixel_workflow_tests').read_bytes()==original
    assert sum(command[0]=='ctest' for command in commands)==int(failure=='native-tests')

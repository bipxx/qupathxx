from pathlib import Path
import hashlib
import json
import subprocess
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/imagej-gaussian-kernel-300'
python=root/'build/ci/reader-locale-297-env/Scripts/python.exe'
args=[str(python),'-I','-X','utf8','-m','pytest','tests/test_qupath_gaussian.py','--collect-only','-q','--noconftest']
r=subprocess.run(args,cwd=work,capture_output=True,text=True);assert r.returncode==0
names=sorted(line.split('::',1)[1] for line in r.stdout.splitlines() if line.startswith('tests/test_qupath_gaussian.py::'))
assert len(names)==61
path=work/'tools/ci/scientific-grid-cases.json';cases=json.loads(path.read_text());assert len(cases['test_qupath_gaussian'])==60;cases['test_qupath_gaussian']=names
path.write_text(json.dumps(cases,indent=2)+'\n')
(out/'collect.log').write_text(r.stdout+r.stderr)
old=subprocess.check_output(['git','show','adac8ca9124677ac8ee4230e9eec6821360a8acd:CMakeLists.txt'],cwd=work).decode()
new=(work/'CMakeLists.txt').read_text();start=new.index('  add_executable(qupathxx_imagej_gaussian_tests');end=new.index('  target_compile_features(qupathxx_masked_gaussian_tests',start)
assert new[:start]+new[end:]==old
(out/'production-target-applicability.json').write_text(json.dumps(dict(previous_source='adac8ca9124677ac8ee4230e9eec6821360a8acd',only_cmake_change='standalone test target guarded by QUPATHXX_BUILD_TESTS',production_target_definitions_unchanged=True,private_source_not_linked_into_application=True,removed_test_block_sha256=hashlib.sha256(new[start:end].encode()).hexdigest(),application_native_source='28c33573ef732905e52fe49aa193bd8e5e067206'),indent=2)+'\n')
print('Bound61 reference cases and verified standalone-only CMake change')

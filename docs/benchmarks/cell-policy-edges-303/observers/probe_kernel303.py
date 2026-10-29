from pathlib import Path
import hashlib
import importlib.util
import json
import shutil
import subprocess
import numpy as np
from PIL import Image

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/cell-policy-edges-303'
source=out/'kernel-probe';source.mkdir(exist_ok=False)
old=root/'build/imagej-gaussian-kernel-300';products=json.loads((old/'windows-native-products.json').read_text(encoding='utf-8'))
kernel=next(item for item in products if Path(item['path']).name=='imagej_gaussian.obj')
assert hashlib.sha256(Path(kernel['path']).read_bytes()).hexdigest()==kernel['sha256']
manifest=json.loads((old/'compiler-inputs.json').read_text(encoding='utf-8'))
# Accept either retained manifest envelope; authenticate only this module's inputs.
files=manifest['files']
for name in ('cpp/imagej_gaussian.hpp','cpp/imagej_gaussian.cpp'):
    expected=files[name]
    if isinstance(expected,dict):expected=expected.get('sha256',expected.get('checked_out_sha256'))
    assert hashlib.sha256((work/name).read_bytes()).hexdigest()==expected
shutil.copyfile(kernel['path'],source/'imagej_gaussian.obj')
(source/'probe.cpp').write_text('''#include "imagej_gaussian.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
int main(int argc,char** argv) {
    try {
        if(argc!=7) throw std::invalid_argument("input output width height sigma_x sigma_y required");
        const auto width=std::stoull(argv[3]),height=std::stoull(argv[4]);
        if(!width || !height || width>(1ULL<<26)/height) throw std::invalid_argument("invalid probe dimensions");
        std::ifstream input(argv[1],std::ios::binary|std::ios::ate);
        if(!input || input.tellg()!=std::streamoff(width*height*4)) throw std::invalid_argument("probe input size differs");
        input.seekg(0);std::vector<float> values(width*height);
        input.read(reinterpret_cast<char*>(values.data()),std::streamsize(values.size()*4));
        if(!input) throw std::runtime_error("probe input failed");
        const auto blurred=qpx::detail::imagej154p_gaussian(values,width,height,std::stod(argv[5]),std::stod(argv[6]),256*1024*1024);
        const auto log=qpx::detail::imagej154p_laplacian(blurred,width,height,256*1024*1024);
        std::ofstream output(argv[2],std::ios::binary);output.write(reinterpret_cast<const char*>(log.data()),std::streamsize(log.size()*4));
        if(!output) throw std::runtime_error("probe output failed");
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\\n';return 1;}
}
''',encoding='utf-8')
(source/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\nproject(gaussian303 LANGUAGES CXX)\nadd_executable(gaussian303 probe.cpp "'+(source/'imagej_gaussian.obj').as_posix()+'")\ntarget_compile_features(gaussian303 PRIVATE cxx_std_17)\ntarget_compile_options(gaussian303 PRIVATE /fp:strict)\ntarget_include_directories(gaussian303 PRIVATE "'+(work/'cpp').as_posix()+'")\n',encoding='utf-8')
cmake=root/'.venv/Lib/site-packages/cmake/data/bin/cmake.exe';build=source/'build';commands=[]
for name,args in [('configure',[str(cmake),'-S',str(source),'-B',str(build),'-G','Visual Studio 17 2022']),('build',[str(cmake),'--build',str(build),'--config','Release','--parallel','2'])]:
    with (out/('kernel-'+name+'.log')).open('x',encoding='utf-8') as stream:r=subprocess.run(args,stdout=stream,stderr=subprocess.STDOUT)
    commands.append(dict(label=name,argv=args,returncode=r.returncode));assert r.returncode==0
spec=importlib.util.spec_from_file_location('kernel_edge_inputs',work/'tools/qupath_cell_policy_edges.py');tool=importlib.util.module_from_spec(spec);spec.loader.exec_module(tool)
checks=[];raw=source/'arrays';raw.mkdir()
for case in tool.contract()['cases']:
    metadata,stages,_=tool.data(out/'capture1',case);sigma=case.get('native',tool.contract()['native'])['sigma']/(case.get('requested_pixel_size_um',.5)/.5)
    input_path=raw/(case['name']+'-input.f32le');output_path=raw/(case['name']+'-native-log.f32le');expected=raw/(case['name']+'-reference-log.f32le')
    input_path.write_bytes(stages[0].astype('<f4').tobytes());expected.write_bytes(stages[1].astype('<f4').tobytes())
    args=[str(build/'Release/gaussian303.exe'),str(input_path),str(output_path),str(metadata['width']),str(metadata['height']),repr(sigma),repr(sigma)]
    r=subprocess.run(args,capture_output=True,text=True,encoding='utf-8');assert r.returncode==0,r.stderr
    actual=np.frombuffer(output_path.read_bytes(),dtype='<u4');wanted=np.frombuffer(expected.read_bytes(),dtype='<u4');different=actual!=wanted
    checks.append(dict(name=case['name'],sigma=sigma,differing_float32_values=int(np.count_nonzero(different)),
        native_sha256=hashlib.sha256(output_path.read_bytes()).hexdigest(),reference_sha256=hashlib.sha256(expected.read_bytes()).hexdigest()))
retained=[]
for path in (build/'Release/gaussian303.exe',build/'gaussian303.dir/Release/probe.obj',source/'imagej_gaussian.obj'):
    target=out/'kernel-products'/path.name;target.parent.mkdir(exist_ok=True);shutil.copyfile(path,target)
    retained.append(dict(path=str(target),bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
(out/'kernel-probe-proof.json').write_text(json.dumps(dict(imported_kernel=kernel,module_inputs_unchanged=True,application_rebuilt=False,commands=commands,products=retained,cases=checks),indent=2)+'\n',encoding='utf-8')
print([(case['name'],case['differing_float32_values']) for case in checks])

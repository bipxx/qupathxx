from pathlib import Path
import importlib.util
import json
import subprocess
import numpy as np
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/postlog-trace-304';probe=out/'new-basins';probe.mkdir(exist_ok=False)
(probe/'main.cpp').write_text('''#include "qupath_response_basins.hpp"
#include <fstream>
#include <iostream>
#include <string>
int main(int argc,char** argv) {
    try {
        if(argc!=5) return 2;const auto w=std::stoull(argv[3]),h=std::stoull(argv[4]);
        std::vector<float> input(w*h);std::ifstream source(argv[1],std::ios::binary);
        source.read(reinterpret_cast<char*>(input.data()),std::streamsize(input.size()*4));if(!source) return 3;
        const auto labels=qpx::cell_detail::qupath070_response_basins(input,w,h,10000,256*1024*1024);
        std::ofstream dest(argv[2],std::ios::binary);dest.write(reinterpret_cast<const char*>(labels.data()),std::streamsize(labels.size()*4));if(!dest) return 4;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\\n';return 1;}
}
''',encoding='utf-8')
(probe/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\nproject(newbasins LANGUAGES CXX)\nadd_executable(newbasins main.cpp)\ntarget_compile_features(newbasins PRIVATE cxx_std_17)\ntarget_compile_options(newbasins PRIVATE /fp:strict)\ntarget_include_directories(newbasins PRIVATE "'+(work/'cpp').as_posix()+'")\n',encoding='utf-8')
cmake=root/'.venv/Lib/site-packages/cmake/data/bin/cmake.exe';build=probe/'build'
for name,args in [('configure',[str(cmake),'-S',str(probe),'-B',str(build),'-G','Visual Studio 17 2022']),('build',[str(cmake),'--build',str(build),'--config','Release','--parallel','2'])]:
    with (out/('new-basins-'+name+'.log')).open('x',encoding='utf-8') as stream:r=subprocess.run(args,stdout=stream,stderr=subprocess.STDOUT)
    assert r.returncode==0
spec=importlib.util.spec_from_file_location('new_basin_edges',work/'tools/qupath_cell_policy_edges.py');tool=importlib.util.module_from_spec(spec);spec.loader.exec_module(tool)
comparison=tool.load('new_basin_metrics',work/'tools/compare_qupath_cells.py');checks=[]
for case in tool.contract()['cases']:
    metadata,stages,_=tool.data(tool.FIXTURE,case);h,w=stages[0].shape;name=case['name'];target=out/'arrays'/(name+'-new-basins.u32le')
    subprocess.run([str(build/'Release/newbasins.exe'),str(out/'arrays'/(name+'-log.f32le')),str(target),str(w),str(h)],check=True)
    actual=np.frombuffer(target.read_bytes(),dtype='<u4').reshape(h,w)
    metric=comparison.metrics(stages[2].astype(np.uint32),actual,tool.contract()['comparison'])
    checks.append(dict(name=name,metric=metric,raw_label_differences=int(np.count_nonzero(actual!=stages[2]))))
(out/'new-basins.json').write_text(json.dumps(dict(cases=checks),indent=2)+'\n',encoding='utf-8')
print([(case['name'],case['raw_label_differences'],case['metric']['foreground_iou'],min([m['iou'] for m in case['metric']['matches']] or [1])) for case in checks])

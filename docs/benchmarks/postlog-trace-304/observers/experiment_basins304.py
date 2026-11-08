from pathlib import Path
import importlib.util
import json
import subprocess
import numpy as np
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/postlog-trace-304';probe=out/'basin-probe';probe.mkdir(exist_ok=True);assert not any(probe.iterdir())
text=(work/'cpp/log_response_regions.hpp').read_text(encoding='utf-8');start=text.index('inline std::vector<std::uint32_t> log_response_basins(');end=text.index('\ninline std::vector<std::uint8_t> log_basin_regions',start)
function=text[start:end].replace('log_response_basins(', 'trace_basins(',1)
needle='std::size_t maximum_basins,const std::atomic<bool>* cancelled)'
assert function.count(needle)==1;function=function.replace(needle,'std::size_t maximum_basins,const std::atomic<bool>* cancelled,bool exclude_edge,bool cap_priority)')
needle='if((at&4095)==0) checkpoint(cancelled);\n            log_neighbors(queue[at]'
assert function.count(needle)==2
function=function.replace(needle,'if((at&4095)==0) checkpoint(cancelled);\n            const auto j=queue[at];if(exclude_edge && (! (j%width) || j%width+1==width || j<width || j/width+1==height)) higher=true;\n            log_neighbors(queue[at]',1)
function=function.replace('std::min(next.floor,response[j])','(cap_priority?std::min(next.floor,response[j]):response[j])')
driver='''#include "log_watershed.hpp"
#include <fstream>
#include <iostream>
#include <string>
namespace qpx::cell_detail {
'''+function+'''
}
int main(int argc,char** argv) {
    try {
        if(argc!=7) throw std::invalid_argument("response output width height exclude_edge cap_priority required");
        const auto w=std::stoull(argv[3]),h=std::stoull(argv[4]);std::ifstream input(argv[1],std::ios::binary);
        std::vector<float> response(w*h);input.read(reinterpret_cast<char*>(response.data()),std::streamsize(response.size()*4));
        if(!input) throw std::runtime_error("input failed");
        const auto labels=qpx::cell_detail::trace_basins(response,w,h,10000,nullptr,std::stoi(argv[5])!=0,std::stoi(argv[6])!=0);
        std::ofstream output(argv[2],std::ios::binary);output.write(reinterpret_cast<const char*>(labels.data()),std::streamsize(labels.size()*4));
        if(!output) throw std::runtime_error("output failed");
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\\n';return 1;}
}
'''
(probe/'probe.cpp').write_text(driver,encoding='utf-8')
(probe/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\nproject(basins304 LANGUAGES CXX)\nadd_executable(basins304 probe.cpp)\ntarget_compile_features(basins304 PRIVATE cxx_std_17)\ntarget_compile_options(basins304 PRIVATE /fp:strict)\ntarget_include_directories(basins304 PRIVATE "'+(work/'cpp').as_posix()+'")\n',encoding='utf-8')
cmake=root/'.venv/Lib/site-packages/cmake/data/bin/cmake.exe';build=probe/'build'
for name,args in [('configure',[str(cmake),'-S',str(probe),'-B',str(build),'-G','Visual Studio 17 2022']),('build',[str(cmake),'--build',str(build),'--config','Release','--parallel','2'])]:
    with (out/('basins-'+name+'.log')).open('x',encoding='utf-8') as stream:r=subprocess.run(args,stdout=stream,stderr=subprocess.STDOUT)
    assert r.returncode==0
spec=importlib.util.spec_from_file_location('basin_edges',work/'tools/qupath_cell_policy_edges.py');tool=importlib.util.module_from_spec(spec);spec.loader.exec_module(tool)
comparison=tool.load('basin_metrics',work/'tools/compare_qupath_cells.py');checks=[]
for case in tool.contract()['cases']:
    metadata,stages,_=tool.data(tool.FIXTURE,case);h,w=stages[0].shape;name=case['name'];variants={}
    for edge in (0,1):
        for cap in (0,1):
            target=out/'arrays'/(name+'-basins-e'+str(edge)+'-c'+str(cap)+'.u32le')
            subprocess.run([str(build/'Release/basins304.exe'),str(out/'arrays'/(name+'-log.f32le')),str(target),str(w),str(h),str(edge),str(cap)],check=True)
            actual=np.frombuffer(target.read_bytes(),dtype='<u4').reshape(h,w)
            if edge==0 and cap==1:assert target.read_bytes()==(out/'arrays'/(name+'-basins.u32le')).read_bytes()
            variants[str(edge)+str(cap)]=comparison.metrics(stages[2].astype(np.uint32),actual,tool.contract()['comparison'])
    checks.append(dict(name=name,variants=variants))
(out/'basin-experiments.json').write_text(json.dumps(dict(cases=checks,production_changed=False),indent=2)+'\n',encoding='utf-8')
print([(case['name'],{key:(value['reference_count'],value['native_count'],value['foreground_iou']) for key,value in case['variants'].items()}) for case in checks])

from pathlib import Path
import importlib.util
import json
import subprocess
import numpy as np
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/postlog-trace-304';probe=out/'nuclei-probe';probe.mkdir(exist_ok=False)
(probe/'main.cpp').write_text('''#include "qupath_nuclei.hpp"
#include "cell_expansion.hpp"
#include <fstream>
#include <iostream>
#include <string>
template<class T> std::vector<T> read(const char* name,std::size_t n) {std::vector<T> a(n);std::ifstream f(name,std::ios::binary);f.read(reinterpret_cast<char*>(a.data()),std::streamsize(n*sizeof(T)));if(!f) throw std::runtime_error("input failed");return a;}
template<class T> void write(const std::string& name,const std::vector<T>& a) {std::ofstream f(name,std::ios::binary);f.write(reinterpret_cast<const char*>(a.data()),std::streamsize(a.size()*sizeof(T)));if(!f) throw std::runtime_error("output failed");}
int main(int argc,char** argv) {
    try {
        if(argc!=12) return 2;const auto w=std::stoull(argv[5]),h=std::stoull(argv[6]);
        const auto score=read<float>(argv[1],w*h),response=read<float>(argv[2],w*h);const auto domain=read<std::uint8_t>(argv[3],w*h);
        qpx::cell_detail::LogParameters p;p.sigma=0;p.minimum_radius=0;p.minimum_spacing=0;p.threshold=std::stod(argv[11]);
        p.minimum_area=std::stoull(argv[8]);p.maximum_area=std::stoull(argv[9]);p.eight_connected_background=true;
        const auto result=qpx::cell_detail::qupath070_nuclei(score,response,domain,w,h,std::stod(argv[7]),p);
        const auto bodies=qpx::cell_detail::expand_cells_watershed(result.nuclei.labels,domain,w,h,std::stod(argv[10]));
        const std::string prefix=argv[4];write(prefix+"-candidate-basins.u32le",result.basins);write(prefix+"-candidate-regions.u8",result.regions);
        write(prefix+"-candidate-binary.u8",result.binary);write(prefix+"-candidate-refinement.u8",result.refinement);
        write(prefix+"-candidate-nuclei.u32le",result.nuclei.labels);write(prefix+"-candidate-bodies.u32le",bodies.labels);
    }catch(const std::exception& error){std::cerr<<error.what()<<'\\n';return 1;}
}
''',encoding='utf-8')
kernel=root/'build/imagej-gaussian-kernel-300/windows-products/imagej_gaussian.obj'
(probe/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\nproject(nuclei304 LANGUAGES CXX)\nadd_executable(nuclei304 main.cpp "'+kernel.as_posix()+'")\ntarget_compile_features(nuclei304 PRIVATE cxx_std_17)\ntarget_compile_options(nuclei304 PRIVATE /fp:strict)\ntarget_include_directories(nuclei304 PRIVATE "'+(work/'cpp').as_posix()+'")\n',encoding='utf-8')
cmake=root/'.venv/Lib/site-packages/cmake/data/bin/cmake.exe';build=probe/'build'
for name,args in [('configure',[str(cmake),'-S',str(probe),'-B',str(build),'-G','Visual Studio 17 2022']),('build',[str(cmake),'--build',str(build),'--config','Release','--parallel','2'])]:
    with (out/('nuclei-'+name+'.log')).open('x',encoding='utf-8') as stream:r=subprocess.run(args,stdout=stream,stderr=subprocess.STDOUT)
    assert r.returncode==0
spec=importlib.util.spec_from_file_location('nucleus_edges',work/'tools/qupath_cell_policy_edges.py');tool=importlib.util.module_from_spec(spec);spec.loader.exec_module(tool)
comparison=tool.load('nucleus_metrics',work/'tools/compare_qupath_cells.py');checks=[]
for case in tool.contract()['cases']:
    metadata,stages,rgb=tool.data(tool.FIXTURE,case);h,w=stages[0].shape;name=case['name'];prefix=out/'arrays'/name
    step=case.get('requested_pixel_size_um',.5)/.5;recipe=case.get('native',tool.contract()['native'])
    minimum=int(np.ceil(recipe['min_nucleus_pixels']/step**2));maximum=int(np.floor(recipe['max_nucleus_pixels']/step**2))
    args=[str(build/'Release/nuclei304.exe'),*[str(prefix)+suffix for suffix in ('-score.f32le','-log.f32le','-domain.u8')],str(prefix),str(w),str(h),repr(recipe['sigma']/step),str(minimum),str(maximum),repr(recipe['cell_expansion']/step),repr(recipe['threshold'])]
    r=subprocess.run(args,capture_output=True,text=True,encoding='utf-8');assert r.returncode==0,r.stderr
    binary=np.frombuffer(Path(str(prefix)+'-candidate-binary.u8').read_bytes(),dtype=np.uint8).reshape(h,w)
    nuclei=np.frombuffer(Path(str(prefix)+'-candidate-nuclei.u32le').read_bytes(),dtype='<u4').reshape(h,w)
    bodies=np.frombuffer(Path(str(prefix)+'-candidate-bodies.u32le').read_bytes(),dtype='<u4').reshape(h,w)
    features=tool.read(tool.FIXTURE/name/'cells.geojson')['features'];origin=case.get('roi',[0,0,256,192])[:2]
    reference=tool.geometry_labels(features,'geometry',(h,w),origin,step)
    refinement=None
    if 'Refined boundaries' in metadata['slices']:
        native=np.frombuffer(Path(str(prefix)+'-candidate-refinement.u8').read_bytes(),dtype=np.uint8).reshape(h,w)
        refinement=int(np.count_nonzero(native!=(stages[metadata['slices'].index('Refined boundaries')]>0)))
    checks.append(dict(name=name,binary_differences=int(np.count_nonzero(binary!=(stages[3]>0))),refinement_differences=refinement,
        nuclei=comparison.metrics(stages[-1].astype(np.uint32),nuclei,tool.contract()['comparison']),body=comparison.metrics(reference,bodies,tool.contract()['comparison'])))
(out/'candidate-nuclei.json').write_text(json.dumps(dict(cases=checks,public_request_delivered=False),indent=2)+'\n',encoding='utf-8')
print([(case['name'],case['binary_differences'],case['refinement_differences'],case['nuclei']['foreground_iou'],case['body']['foreground_iou']) for case in checks])

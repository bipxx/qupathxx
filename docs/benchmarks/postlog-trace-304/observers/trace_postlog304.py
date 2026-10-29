from pathlib import Path
import hashlib
import importlib.util
import json
import subprocess
import numpy as np
import qupathxx as q

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/postlog-trace-304';out.mkdir(exist_ok=False)
probe=out/'probe';probe.mkdir()
(probe/'trace.cpp').write_text('''#include "scientific_cell_recipe.hpp"
#include <fstream>
#include <iostream>
#include <string>
template<class T> std::vector<T> read(const char* name,std::size_t n) {
    std::ifstream file(name,std::ios::binary|std::ios::ate);
    if(!file || file.tellg()!=std::streamoff(n*sizeof(T))) throw std::runtime_error("trace input size differs");
    file.seekg(0);std::vector<T> result(n);file.read(reinterpret_cast<char*>(result.data()),std::streamsize(n*sizeof(T)));
    if(!file) throw std::runtime_error("trace input read failed");return result;
}
template<class T> void write(const std::string& name,const std::vector<T>& values) {
    std::ofstream file(name,std::ios::binary);file.write(reinterpret_cast<const char*>(values.data()),std::streamsize(values.size()*sizeof(T)));
    if(!file) throw std::runtime_error("trace output failed");
}
int main(int argc,char** argv) {
    try {
        if(argc!=11) throw std::invalid_argument("score response domain prefix width height step min max threshold required");
        const auto w=std::stoull(argv[5]),h=std::stoull(argv[6]);
        if(!w || !h || w>(1ULL<<26)/h) throw std::invalid_argument("invalid dimensions");
        const auto n=w*h;const auto input=read<float>(argv[1],n),response=read<float>(argv[2],n);const auto domain=read<std::uint8_t>(argv[3],n);
        std::vector<double> score(input.begin(),input.end());std::vector<std::uint8_t> positive(n),valid(n,1);
        for(std::size_t i=0;i<n;++i) positive[i]=response[i]>0;
        qpx::cell_detail::LogParameters p;p.sigma=0;p.threshold=std::stod(argv[10]);p.minimum_radius=0;p.minimum_spacing=0;
        p.minimum_area=std::stoull(argv[8]);p.maximum_area=std::stoull(argv[9]);p.eight_connected_background=true;
        const auto mask=qpx::cell_detail::log_regions_from_positive(score,positive,w,h,p,nullptr);
        const auto basins=qpx::cell_detail::log_response_basins(response,w,h,p.maximum_cells,nullptr);
        const auto step=std::stod(argv[7]);const qpx::cell_detail::PhysicalGridMetric metric(step,step);
        const auto current=qpx::cell_detail::grid_log_watershed(score,valid,domain,w,h,metric,0,p,nullptr,nullptr,&positive);
        const auto shape=qpx::cell_detail::sampled_shape_watershed(mask,w,h,p.prominence,p.maximum_cells,p.work_bytes,nullptr);
        const auto sampled=qpx::cell_detail::log_sampled_nuclei(input,domain,mask,w,h,p,nullptr,nullptr);
        const std::string prefix=argv[4];write(prefix+"-positive.u8",positive);write(prefix+"-regions.u8",mask);
        write(prefix+"-basins.u32le",basins);write(prefix+"-grid.u32le",current.labels);
        write(prefix+"-sampled.u8",shape.mask);write(prefix+"-sampled-nuclei.u32le",sampled.labels);
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\\n';return 1;}
}
''',encoding='utf-8')
(probe/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\nproject(postlog304 LANGUAGES CXX)\nadd_executable(postlog304 trace.cpp)\ntarget_compile_features(postlog304 PRIVATE cxx_std_17)\ntarget_compile_options(postlog304 PRIVATE /fp:strict)\ntarget_include_directories(postlog304 PRIVATE "'+(work/'cpp').as_posix()+'" "'+(work/'cpp/include').as_posix()+'")\n',encoding='utf-8')
cmake=root/'.venv/Lib/site-packages/cmake/data/bin/cmake.exe';build=probe/'build';commands=[]
for name,args in [('configure',[str(cmake),'-S',str(probe),'-B',str(build),'-G','Visual Studio 17 2022']),('build',[str(cmake),'--build',str(build),'--config','Release','--parallel','2'])]:
    with (out/(name+'.log')).open('x',encoding='utf-8') as stream:r=subprocess.run(args,stdout=stream,stderr=subprocess.STDOUT)
    commands.append(dict(label=name,argv=args,returncode=r.returncode));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n',encoding='utf-8');assert r.returncode==0
spec=importlib.util.spec_from_file_location('trace_edges',work/'tools/qupath_cell_policy_edges.py');tool=importlib.util.module_from_spec(spec);spec.loader.exec_module(tool)
comparison=tool.load('trace_metrics',work/'tools/compare_qupath_cells.py');results=[];arrays=out/'arrays';arrays.mkdir()
for case in tool.contract()['cases']:
    metadata,stages,rgb=tool.data(tool.FIXTURE,case);step=case.get('requested_pixel_size_um',.5)/.5
    x,y,w,h=case.get('roi',[0,0,256,192]);height,width=rgb.shape[:2]
    doc=q.Document(q.Image.from_numpy(rgb));doc.set_calibration(step,step)
    def ring(b):
        a,b_,c,d=b;return [q.Point(a,b_),q.Point(a+c,b_),q.Point(a+c,b_+d),q.Point(a,b_+d)]
    roi=[0,0,min(w/step,width),min(h/step,height)]
    if 'hole' in case:
        a,b,c,d=case['hole'];hole=[(a-x)/step,(b-y)/step,c/step,d/step]
        obj=doc.add_roi(q.Roi.polygon(ring(roi),[ring(hole)]))
    else:obj=doc.add_rectangle(q.Rect(*roi))
    recipe=case.get('native',tool.contract()['native']);p=q.GridCellParameters();p.processing=q.CellProcessingPolicy.IMAGEJ_154P;p.units=q.GridCellUnits.MICROMETRES
    p.threshold=recipe['threshold'];p.sigma=recipe['sigma'];p.cell_expansion=recipe['cell_expansion'];p.min_nucleus_area=recipe['min_nucleus_pixels'];p.max_nucleus_area=recipe['max_nucleus_pixels']
    transform=q.StainTransform(tool.contract()['vectors'],tool.contract()['white']);descriptor=q.NominalCellSampling(q.RgbNominalRequest(0,0,width,height,1),0,0)
    request=q.CellRequest(doc,obj,transform,descriptor,p);actual=q.detect_cells(request)
    yy,xx=np.mgrid[:height,:width];domain=((xx+.5)<roi[2])&((yy+.5)<roi[3])
    if 'hole' in case:domain&=~(((xx+.5)>=hole[0])&((xx+.5)<hole[0]+hole[2])&((yy+.5)>=hole[1])&((yy+.5)<hole[1]+hole[3]))
    prefix=arrays/case['name'];inputs=[]
    for suffix,values in [('-score.f32le',stages[0].astype('<f4')),('-log.f32le',stages[1].astype('<f4')),('-domain.u8',domain.astype(np.uint8))]:
        path=Path(str(prefix)+suffix);path.write_bytes(values.tobytes());inputs.append(path)
    resolved=request.resolved_grid_parameters
    args=[str(build/'Release/postlog304.exe'),*[str(p) for p in inputs],str(prefix),str(width),str(height),repr(step),str(resolved.min_nucleus_samples),str(resolved.max_nucleus_samples),repr(p.threshold)]
    r=subprocess.run(args,capture_output=True,text=True,encoding='utf-8');assert r.returncode==0,r.stderr
    current=np.frombuffer(Path(str(prefix)+'-grid.u32le').read_bytes(),dtype='<u4').reshape(height,width)
    np.testing.assert_array_equal(current,actual.nuclei.labels_numpy())
    basins=np.frombuffer(Path(str(prefix)+'-basins.u32le').read_bytes(),dtype='<u4').reshape(height,width)
    shaped=np.frombuffer(Path(str(prefix)+'-sampled.u8').read_bytes(),dtype=np.uint8).reshape(height,width)&domain
    sampled=np.frombuffer(Path(str(prefix)+'-sampled-nuclei.u32le').read_bytes(),dtype='<u4').reshape(height,width)
    reference_binary=stages[metadata['slices'].index('Binary')]>0
    results.append(dict(name=case['name'],current_grid_labels_equal_installed=True,
        basins=comparison.metrics(stages[2].astype(np.uint32),basins,tool.contract()['comparison']),
        sampled_binary_differences=int(np.count_nonzero(shaped!=reference_binary)),
        current_grid_final=comparison.metrics(stages[-1].astype(np.uint32),current,tool.contract()['comparison']),
        sampled_final=comparison.metrics(stages[-1].astype(np.uint32),sampled,tool.contract()['comparison'])))
(out/'trace.json').write_text(json.dumps(dict(native_source=q.build_environment().git_revision,cases=results,application_rebuilt=False,biological_validation=False),indent=2)+'\n',encoding='utf-8')
print([(case['name'],case['sampled_binary_differences'],case['sampled_final']['foreground_iou']) for case in results],flush=True)

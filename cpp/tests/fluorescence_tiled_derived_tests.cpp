#include "qupathxx/fluorescence_cell_workflow.hpp"
#include "qupathxx/fluorescence_tiled_checkpoint.hpp"
#include "qupathxx/dark_flat_workflow.hpp"
#include "qupathxx/linear_unmixing_workflow.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/workflow.hpp"
#include "quantitative_tiff_fixture.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <chrono>
using namespace qpx;
namespace fs=std::filesystem;
void check(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}
template<class E=std::exception,class F>void rejects(F f){try{f();}catch(const E&){return;}throw std::runtime_error("Expected derived tiled refusal");}
std::string bytes(const fs::path& p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void fixture(const fs::path& path,int role) {
    qpx_test::raw_pixel_tiff_fixture(path,80,52);std::fstream file(path,std::ios::in|std::ios::out|std::ios::binary);file.seekp(160);
    for(int y=0;y<52;++y)for(int x=0;x<80;++x)for(int c=0;c<2;++c) {
        const auto v=std::uint16_t(role==1?10:role==2?11:c?1000+x+10*y:10+100*(x>=37 && x<=39 && y>=20 && y<=22));
        file.put(char(v&255));file.put(char(v>>8));
    }file.close();check(bool(file),"Write authored tiled source/fields");
}
void moments(const RawCellCompartment& a,const RawCellCompartment& b) {
    check(a.pixels==b.pixels && a.valid_pixels==b.valid_pixels && a.channels.size()==b.channels.size(),"Derived whole/tile compartment counts agree");
    for(std::size_t c=0;c<a.channels.size();++c){const auto& x=a.channels[c];const auto& y=b.channels[c];
        check(x.finite_samples==y.finite_samples && x.nonfinite_samples==y.nonfinite_samples && x.sum==y.sum && x.mean==y.mean &&
            x.minimum==y.minimum && x.maximum==y.maximum && x.sample_standard_deviation==y.sample_standard_deviation,"Derived whole/tile raw moments agree exactly");}
}
void compare(const FluorescenceTileResult& tile,const FluorescenceCellRun& whole,int domain) {
    for(const auto& cell:tile.cells) {
        const auto& raster=whole.raster();const auto anchor=std::size_t(cell.anchor_y*80+cell.anchor_x);const auto label=raster.nuclei[anchor];check(label!=0,"Derived global anchor matches untiled nucleus");
        for(bool nucleus:{true,false}) {
            std::vector<std::size_t> actual,expected;for(const auto& span:nucleus?cell.nucleus:cell.cell)for(std::int64_t x=span.x;x<span.x+span.width;++x)actual.push_back(std::size_t(span.y*80+x));
            const auto& mask=nucleus?raster.nuclei:raster.bodies;for(std::size_t i=0;i<mask.size();++i)if(mask[i]==label)expected.push_back(i);
            check(actual==expected,"Derived tiled masks agree exactly with untiled ROI across seams");
        }
        const auto& reference=raster.measurements[label-1];moments(cell.measurements.nucleus,reference.nucleus);moments(cell.measurements.cell,reference.cell);
        check(bool(cell.measurements.cytoplasm)==bool(reference.cytoplasm),"Derived cytoplasm absence agrees");if(reference.cytoplasm)moments(*cell.measurements.cytoplasm,*reference.cytoplasm);
        double dna=0,marker=0;
        for(const auto& span:cell.cell)for(std::int64_t x=span.x;x<span.x+span.width;++x) {
            const double original=10+100*(x>=37 && x<=39 && span.y>=20 && span.y<=22),m=1000+x+10*span.y;
            dna+=domain==0?original-10:domain==1?original*.5:(original-10)*.5;marker+=domain==0?m-10:domain==1?m*2:(m-10)*2;
        }
        check(cell.measurements.cell.channels[0].sum==dna && cell.measurements.cell.channels[1].sum==marker,"Derived moments match independent subtraction/diagonal-mixing equations");
        if(domain)for(std::size_t c=2;c<4;++c)check(cell.measurements.cell.channels[c].sum==0,"Selected unmixing residuals stay zero without clamping");
    }
}
void qualify(const FluorescenceTiledRequest& request,const FluorescenceCellRun& whole,const fs::path& root,
    int domain,const std::string& correction,const std::string& unmix) {
    std::uint64_t observed=0;const auto full=checkpoint_tiled_fluorescence_cells(request,root.u8string(),{},[&](std::size_t,const FluorescenceTileResult& tile){compare(tile,whole,domain);observed+=tile.cells.size();});
    check(full.state=="ready" && full.cells==1 && observed==1 && full.resolved_threshold==whole.raster().resolved_threshold && full.accounted_work_bytes<=256u*1024*1024,"Derived global threshold/bounded report matches untiled reference");
    check(full.measurement_domain==whole.measurement_domain() && full.correction_record==correction && full.unmix_record==unmix,"Derived report retains exact measurement domain and parent recipes");
    const auto partial=fs::u8path(root.u8string()+"-resume");std::atomic<bool> cancel{false};
    rejects<WorkflowCancelled>([&]{checkpoint_tiled_fluorescence_cells(request,partial.u8string(),{},[&](std::size_t i,const FluorescenceTileResult&){if(i==1)cancel.store(true);},&cancel);});
    const auto resumed=checkpoint_tiled_fluorescence_cells(request,partial.u8string(),{},[&](std::size_t,const FluorescenceTileResult& tile){compare(tile,whole,domain);});
    check(resumed.reused_tiles==2 && resumed.segmented_tiles==6 && resumed.output_sha256==full.output_sha256 && resumed.input_sha256==full.input_sha256,"Derived interruption/resume preserves exact scientific output");
    for(const auto& entry:fs::directory_iterator(root))check(bytes(entry.path())==bytes(partial/entry.path().filename()),"Every derived persisted record is byte-identical after resume");
}
int main(){try {
    if(!has_tiff()){std::cout<<"Native TIFF capability unavailable; no derived source claim\n";return 0;}
    const auto root=fs::current_path()/("tiled-derived-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directory(root);
    fixture(root/"signal.tif",0);fixture(root/"dark.tif",1);fixture(root/"flat.tif",2);
    const auto before_signal=bytes(root/"signal.tif"),before_dark=bytes(root/"dark.tif"),before_flat=bytes(root/"flat.tif");
    Document doc(open_tiff((root/"signal.tif").u8string()));auto dark=open_tiff((root/"dark.tif").u8string()),flat=open_tiff((root/"flat.tif").u8string());
    const DarkFlatRequest correction(doc,dark,flat,{{1,1},0});RawRegionRequest r{{0,0,0,80,52},0,2};
    FluorescenceCellOptions p;p.threshold_method=FluorescenceThreshold::Otsu256;p.sigma=.7;p.cell_expansion=1.5;p.max_cells=1000;
    FluorescenceTileOptions t;t.width=20;t.height=26;t.halo=12;
    rejects([&]{FluorescenceTiledRequest missing(doc,correction,r,0,p,t);});const auto cid=keep_dark_flat_recipe(doc,correction);
    const FluorescenceTiledRequest corrected(doc,correction,r,0,p,t);const auto corrected_whole=detect_fluorescence_cells(FluorescenceCellRequest(doc,correction,r,0,p,{3,3}));
    qualify(corrected,*corrected_whole,root/"corrected",0,cid,{});
    remove_dark_flat_recipe(doc,cid);check(!corrected.matches(doc),"Removed correction invalidates tiled capture");doc.undo();check(corrected.matches(doc),"Correction Undo restores matching capture");
    const DarkFlatRequest changed(doc,dark,flat,{{2,1},0});keep_dark_flat_recipe(doc,changed);const FluorescenceTiledRequest renormalized(doc,changed,r,0,p,t);
    rejects<std::invalid_argument>([&]{checkpoint_tiled_fluorescence_cells(renormalized,(root/"corrected").u8string());});
    if(has_linear_unmixing()) {
        const auto m=doc.server()->raw_metadata();LinearMixingMatrix matrix;matrix.inputs={{0,m->channel_names.at(0)},{1,m->channel_names.at(1)}};matrix.components={"DNA","Marker"};matrix.coefficients={2,0,0,.5};
        const LinearUnmixRequest unmix(doc,matrix);rejects([&]{FluorescenceTiledRequest missing(doc,unmix,r,0,p,t);});const auto uid=keep_linear_unmix_recipe(doc,unmix);
        auto output=r;output.channels=4;const FluorescenceTiledRequest unmixed(doc,unmix,output,0,p,t);const auto unmix_whole=detect_fluorescence_cells(FluorescenceCellRequest(doc,unmix,output,0,p,{3,3}));
        qualify(unmixed,*unmix_whole,root/"unmixed",1,{},uid);
        matrix.input_domain=correction.source()->domain();const LinearUnmixRequest combined(doc,correction,matrix);const auto combined_id=keep_linear_unmix_recipe(doc,combined);
        const FluorescenceTiledRequest combined_request(doc,combined,output,0,p,t);const auto combined_whole=detect_fluorescence_cells(FluorescenceCellRequest(doc,combined,output,0,p,{3,3}));
        qualify(combined_request,*combined_whole,root/"combined",2,cid,combined_id);
        check(std::set<std::string>{corrected.source_sha256(),unmixed.source_sha256(),combined_request.source_sha256()}.size()==3,"Raw/corrected/unmixed measurement identities cannot alias");
        Document reopened(open_tiff((root/"signal.tif").u8string()));reopened.set_image_data_properties(doc.image_data_properties());
        const auto replayed_correction=restore_dark_flat_recipe(reopened,load_dark_flat_recipe(reopened,cid),open_tiff((root/"dark.tif").u8string()),open_tiff((root/"flat.tif").u8string()));
        const auto replayed_unmix=restore_linear_unmix_recipe(reopened,load_linear_unmix_recipe(reopened,combined_id),std::optional<DarkFlatRequest>{replayed_correction});
        const FluorescenceTiledRequest replayed(reopened,replayed_unmix,output,0,p,t);
        const auto replayed_report=checkpoint_tiled_fluorescence_cells(replayed,(root/"combined").u8string());
        check(replayed.sha256()==combined_request.sha256() && replayed_report.reused_tiles==8 && !replayed_report.segmented_tiles,"Fresh native field/operator replay resumes the same derived checkpoint");
        FluorescenceCellJob job;check(job.request(combined_request,(root/"combined").u8string(),{}),"Existing cell job resumes corrected unmixing");remove_linear_unmix_recipe(doc,combined_id);
        check(job.wait(doc).state=="stale" && !job.tiled_result(),"Missing retained unmixing recipe rejects owner delivery");doc.undo();
        check(job.request(combined_request,(root/"combined").u8string(),{}) && job.wait(doc).state=="ready" && job.tiled_result()->reused_tiles==8,"Restored derived recipe resumes through existing job");
    }
    check(bytes(root/"signal.tif")==before_signal && bytes(root/"dark.tif")==before_dark && bytes(root/"flat.tif")==before_flat,"Derived tiled workflows preserve all source/field bytes");
    {std::ofstream changed_file(root/"flat.tif",std::ios::binary|std::ios::app);changed_file.put('x');check(bool(changed_file),"Actually mutate an authored correction field");}
    check(!corrected.publication_matches(doc),"Actual correction field mutation invalidates publication");rejects<WorkflowInputChanged>([&]{checkpoint_tiled_fluorescence_cells(corrected,(root/"corrected").u8string());});
    std::cout<<"Derived tiled fluorescence: fixed normalization/component/residual domains, exact whole-ROI masks/moments, checkpoint resume, recipe/source invalidation and work bounds pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

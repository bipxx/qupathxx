#include "qupathxx/project_batch.hpp"
#include "qupathxx/raw_tables.hpp"
#include "fluorescence_fixture.hpp"
#include <iostream>

namespace fs=std::filesystem;
using namespace qpx;
void check(bool value) {if(!value)throw std::runtime_error("Fluorescence workflow check failed");}
template<class F> void rejects(F action) {try {action();}catch(const std::exception&){return;}throw std::runtime_error("Expected fluorescence refusal");}
int main() {try {
    Project identity;const auto root=fs::current_path()/(identity.identity()+"-fluorescence");fs::create_directory(root);
    for(const bool floating:{false,true}) {
        const auto prefix=floating?"float32":"uint16";const auto source=root/(std::string(prefix)+".ome.tif");
        qpx_test::fluorescence_fixture(source,floating);
        Project project;TiffOpenOptions opening;opening.view.z=1;
        const auto image=project.add_tiff_image(source.u8string(),opening);auto doc=project.document(image);
        doc->set_image_analysis_settings(ImageAnalysisSettings("FLUORESCENCE"));
        const auto metadata=doc->server()->raw_metadata();
        check(metadata->channels==3 && metadata->z_planes==2 && metadata->channel_names==std::vector<std::string>({"High","Zero","Marker"}));
        check(metadata->pixel_width_um==.5 && metadata->pixel_height_um==.75);
        const auto object=doc->add_rectangle({0,0,2,2},"Fluorescence ROI");
        RawRegionRequest region;region.region={0,0,0,2,2};region.channels=3;region.z=1;
        const RawAnalysisRequest request(*doc,region,object,false,16,4);
        const auto result=analyze_raw(request);const auto& stats=result->statistics();
        check(stats.finite_samples[0]==4 && stats.means[0]==16100 && stats.minima[0]==1100 && stats.maxima[0]==31100);
        check(stats.finite_samples[1]==4 && stats.means[1]==0 && stats.nonfinite_samples[1]==0);
        if(floating)check(stats.finite_samples[2]==2 && stats.nonfinite_samples[2]==2 && stats.means[2]==-.75 && stats.minima[2]==-2.75 && stats.maxima[2]==1.25);
        else check(stats.finite_samples[2]==4 && stats.means[2]==411.5);
        const auto record=doc->keep_raw_analysis(request,result);
        auto view=opening.view;view.channels={2,0,-1};view.lower={0,0,0};view.upper={1,1,1};doc->set_tiff_view(view);
        check(request.matches(*doc) && analyze_raw(request)->input_sha256()==result->input_sha256());
        const auto saved=root/(std::string(prefix)+".qpx");project.save_new(saved.u8string());
        auto reopened=Project::open(saved.u8string());auto restored=reopened->document(image);
        const auto replay=RawAnalysisRequest::replay(*restored,record);
        check(restored->verify_raw_analysis(record,replay,analyze_raw(replay)));
        check(restored->image_analysis_settings()->image_type()=="FLUORESCENCE");
        save_raw_analysis_csv(RawTableRequest(*restored,{record},true),(root/(std::string(prefix)+".csv")).u8string());
        rejects([&]{project.save_new(saved.u8string());});
        auto invalid=region;invalid.channels=4;rejects([&]{RawAnalysisRequest bad(*doc,invalid,object);analyze_raw(bad);});
        ProjectBatchOptions options;options.kind=ProjectBatchKind::Raw;options.raw=region;options.raw_follow_display=false;options.raw_max_pixels=4;options.roi_tile_size=16;
        const ProjectBatchRequest batch(*reopened,StainTransform({{1,0,0},{0,1,0}}),{{image,{object}}},options);
        const auto report=run_project_batch(batch,(root/(std::string(prefix)+"-batch")).u8string());
        if(report.state!="succeeded")throw std::runtime_error(report.error+(report.images.empty()?"":report.images[0].error));
        auto batched=Project::open(report.images.at(0).path)->document(image);
        check(batched->latest_raw_analysis(object)->result->statistics().means==stats.means);
        restored->update_roi(object,Roi::rectangle({0,0,1,1}));check(!replay.matches(*restored));
    }
    std::cout<<"OME fluorescence: uint16/float32 axes, raw counts/nonfinites, display independence, CSV, save/replay, native batch and stale/no-overwrite checks passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}

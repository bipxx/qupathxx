#include "qupathxx/fluorescence_cell_workflow.hpp"
#include "qupathxx/fluorescence_tiled_checkpoint.hpp"
#include "qupathxx/file_fingerprint.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/workflow.hpp"
#include "../sha256.hpp"
#include "../raw_pixel_archive_binary.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <cstring>
#include <algorithm>
#include <chrono>
using namespace qpx;
namespace fs=std::filesystem;
void check(bool yes,const std::string& message){if(!yes)throw std::runtime_error(message);}
void real(Sha256& h,double value){std::uint64_t bits;std::memcpy(&bits,&value,8);h.integer(bits);}
void compartment(Sha256& h,const RawCellCompartment& c) {
    h.integer(c.pixels);h.integer(c.valid_pixels);h.integer(c.channels.size());
    for(const auto& v:c.channels){h.integer(v.finite_samples);h.integer(v.nonfinite_samples);for(auto x:{v.sum,v.mean,v.minimum,v.maximum,v.sample_standard_deviation}){h.integer(bool(x));if(x)real(h,*x);}}
}
void equal(const RawCellCompartment& a,const RawCellCompartment& b) {
    Sha256 x,y;compartment(x,a);compartment(y,b);check(x.finish()==y.finish(),"Real whole/tile compartment values differ bitwise");
}
using Cells=std::map<std::string,std::string>;
std::set<std::uint32_t> complete_reference(const FluorescenceCellRun& run,const RawPixelBuffer& raw) {
    // Independent full-field policy oracle, before filtered watershed labels:
    // every child of an edge-cut foreground component remains unresolved.
    const auto width=std::size_t(raw.width()),height=std::size_t(raw.height()),n=width*height;
    std::vector<std::uint8_t> foreground(n),visited(n);std::vector<bool> unresolved(run.raster().measurements.size());
    std::visit([&](const auto& values){for(std::size_t i=0;i<n;++i){check(raw.valid()[i] && std::isfinite(double(values[i*raw.channels()])),"Real reference expects finite valid selected input");foreground[i]=double(values[i*raw.channels()])>run.raster().resolved_threshold;}},raw.storage());
    std::vector<std::size_t> component;component.reserve(n);
    for(std::size_t start=0;start<n;++start)if(foreground[start] && !visited[start]) {
        component.clear();component.push_back(start);visited[start]=1;std::size_t minx=width,miny=height,maxx=0,maxy=0;
        for(std::size_t at=0;at<component.size();++at){const auto i=component[at],x=i%width,y=i/width;minx=std::min(minx,x);maxx=std::max(maxx,x);miny=std::min(miny,y);maxy=std::max(maxy,y);
            for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){const auto px=std::int64_t(x)+dx,py=std::int64_t(y)+dy;if(px<0 || py<0 || px>=std::int64_t(width) || py>=std::int64_t(height))continue;const auto j=std::size_t(py)*width+std::size_t(px);if(foreground[j] && !visited[j]){visited[j]=1;component.push_back(j);}}}
        if(!minx || !miny || maxx+1==width || maxy+1==height)for(auto i:component)if(auto label=run.raster().nuclei[i])unresolved[label-1]=true;
    }
    for(std::size_t i=0;i<n;++i)if(auto label=run.raster().bodies[i]){const auto x=run.region().region.x+std::int64_t(i%width),y=run.region().region.y+std::int64_t(i/width);if(x<4 || y<4 || run.source_metadata().width-1-x<4 || run.source_metadata().height-1-y<4)unresolved[label-1]=true;}
    std::set<std::uint32_t> result;
    for(std::size_t i=0;i<unresolved.size();++i)if(!unresolved[i] && !run.raster().boundaries[i].nucleus && !run.raster().boundaries[i].cell)result.insert(std::uint32_t(i+1));return result;
}
struct Oracle {
    const FluorescenceCellRun& whole;std::int64_t width,origin_x,origin_y;std::set<std::uint32_t> expected,seen;Cells cells;
    Oracle(const FluorescenceCellRun& run,const std::set<std::uint32_t>& eligible):whole(run),width(run.raster().width),origin_x(run.region().region.x),origin_y(run.region().region.y),expected(eligible) {}
    void accept(const FluorescenceTileResult& tile) {
        for(const auto& c:tile.cells) {
            const auto label=whole.raster().nuclei.at(std::size_t((c.anchor_y-origin_y)*width+c.anchor_x-origin_x));
            check(expected.count(label) && seen.insert(label).second,"Real returned cell has no unique complete whole-ROI counterpart: anchor="+std::to_string(c.anchor_x)+","+std::to_string(c.anchor_y)+" label="+std::to_string(label)+" eligible="+std::to_string(expected.count(label)));
            Sha256 signature;signature.update(c.id.data(),c.id.size());signature.integer(std::uint64_t(c.anchor_x));signature.integer(std::uint64_t(c.anchor_y));
            for(bool nucleus:{true,false}) {
                const auto& mask=nucleus?whole.raster().nuclei:whole.raster().bodies;const auto& spans=nucleus?c.nucleus:c.cell;
                signature.integer(spans.size());
                std::vector<std::size_t> actual,reference;
                for(const auto& s:spans){signature.integer(std::uint64_t(s.y));signature.integer(std::uint64_t(s.x));signature.integer(std::uint64_t(s.width));for(std::int64_t x=s.x;x<s.x+s.width;++x)actual.push_back(std::size_t((s.y-origin_y)*width+x-origin_x));}
                for(std::size_t i=0;i<mask.size();++i)if(mask[i]==label)reference.push_back(i);
                check(actual==reference,"Real tiled nucleus/body geometry differs at a seam");
            }
            const auto& reference=whole.raster().measurements.at(label-1);equal(c.measurements.nucleus,reference.nucleus);equal(c.measurements.cell,reference.cell);
            check(bool(c.measurements.cytoplasm)==bool(reference.cytoplasm),"Real cytoplasm absence differs");if(reference.cytoplasm)equal(*c.measurements.cytoplasm,*reference.cytoplasm);
            compartment(signature,c.measurements.nucleus);compartment(signature,c.measurements.cell);signature.integer(bool(c.measurements.cytoplasm));if(c.measurements.cytoplasm)compartment(signature,*c.measurements.cytoplasm);
            check(cells.emplace(c.id,signature.finish()).second,"Real global identity is repeated");
        }
    }
    void finish(){check(seen==expected,"Real tiling did not return every complete reference cell: "+std::to_string(seen.size())+" / "+std::to_string(expected.size()));}
};
nlohmann::json summary(const FluorescenceTiledReport& r){return {{"state",r.state},{"cells",r.cells},{"tiles",r.tiles},{"unresolved_tiles",r.unresolved_tiles},{"reused_tiles",r.reused_tiles},{"segmented_tiles",r.segmented_tiles},{"threshold",r.resolved_threshold},{"accounted_work_bytes",r.accounted_work_bytes},{"input_sha256",r.input_sha256},{"output_sha256",r.output_sha256},{"elapsed_ms",r.elapsed_ms}};}
std::string bytes(const fs::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void persisted(const fs::path& root,std::size_t i,const FluorescenceTileResult& tile) {
    const auto record=bytes(root/("tile-"+std::to_string(i)+(tile.complete()?".qft":".qfu")));
    qpx::detail::raw_archive::Reader r(reinterpret_cast<const std::uint8_t*>(record.data()),record.size(),tile.complete()?"QFT\1":"QFU\1",nullptr,4u*1024*1024);
    r.text(64);r.text(64);check(r.integer()==i,"Real persisted tile index differs");r.real();check(r.text(64)==tile.input_sha256 && r.text(64)==tile.recipe_sha256,"Real persisted source/recipe hashes differ");
    check(r.integer()==tile.finite_segmentation_pixels_in_core,"Real persisted support count differs");
    if(!tile.complete()){check(r.integer()==tile.unresolved_foreground_pixels_in_core,"Real partial foreground status differs");check(r.integer()==tile.unresolved_labels.size(),"Real partial labels differ");for(auto label:tile.unresolved_labels)check(r.u32()==label,"Real partial label identity differs");}
    check(r.integer()==tile.cells.size(),"Certified cells from unresolved tiles were not persisted");
}
int main(int argc,char** argv){try {
    check(argc==3 || argc==4,"Usage: fluorescence_tiled_real_tests ADMITTED_TIFF NEW_OUTPUT_DIRECTORY [ADMITTED_CASE.json]");
    nlohmann::json profile={{"dataset","BBBC039v1"},{"license","CC0-1.0"},{"sha256","9eacee0e5a4c29af70307af4d310320632e31ed84b4d8b5fd4fce42930f780ec"}};
    if(argc==4){check(fs::file_size(argv[3])<=16384,"Real case profile exceeds limit");profile=nlohmann::json::parse(bytes(argv[3]));}
    const auto input=fingerprint_file(argv[1]);check(input->sha256==profile.at("sha256").get<std::string>(),"Input differs from admitted fluorescence fixture");
    const auto root=fs::absolute(fs::u8path(argv[2]));check(!fs::exists(root),"Real evidence output directory must be new");fs::create_directories(root);
    Document doc(open_tiff(argv[1]));const auto metadata=doc.server()->raw_metadata();RawRegionRequest region;region.region={0,0,0,metadata->width,metadata->height};region.channels=metadata->channels;
    if(profile.contains("roi")){
        const auto& r=profile.at("roi");region.region={r.at(0).get<std::int64_t>(),r.at(1).get<std::int64_t>(),0,r.at(2).get<std::int64_t>(),r.at(3).get<std::int64_t>()};
        region.first_channel=profile.at("first_channel").get<std::uint32_t>();region.channels=profile.at("channels").get<std::uint32_t>();
        check(metadata->width==profile.at("width") && metadata->height==profile.at("height") && metadata->channels==profile.at("source_channels"),"Real WSI axes differ from admitted profile");
        check(region.region.x>0 && region.region.y>0,"Whole-slide evidence must exercise nonzero global ROI origin");
        check(metadata->channel_names.at(region.first_channel)==profile.at("channel_name"),"Real selected channel differs from explicit scientific label");
    }
    region.validate(*metadata);
    FluorescenceCellOptions p;p.threshold_method=FluorescenceThreshold::Otsu256;p.min_nucleus_area=5;p.cell_expansion=3;p.max_cells=profile.value("max_cells",std::size_t(1000));p.work_bytes=256u*1024*1024;
    const auto whole=detect_fluorescence_cells(FluorescenceCellRequest(doc,region,0,p,{0,0}));
    const auto eligible=complete_reference(*whole,doc.server()->read_raw(region));
    check(!eligible.empty(),"Real ROI must contain certified complete cells; empty tissue is not acceptance evidence");
    FluorescenceTileOptions t;t.width=t.height=128;t.halo=256;const FluorescenceTiledRequest request(doc,region,0,p,t);
    check(request.tile_count()>2,"Real fixture must cross multiple tile seams");Oracle baseline_oracle(*whole,eligible);
    const auto baseline=checkpoint_tiled_fluorescence_cells(request,(root/"baseline").u8string(),{},[&](std::size_t i,const FluorescenceTileResult& tile){baseline_oracle.accept(tile);persisted(root/"baseline",i,tile);});baseline_oracle.finish();
    check(baseline.resolved_threshold==whole->raster().resolved_threshold && baseline.accounted_work_bytes<=p.work_bytes,"Real global threshold/work admission differs");
    const auto boundaries=whole->raster().measurements.size()-baseline_oracle.expected.size();if(boundaries)check(baseline.unresolved_tiles>0,"Real boundary cells must remain explicitly unresolved");
    std::atomic<bool> cancelled{false};bool stopped=false;double cancellation_ms=0;std::chrono::steady_clock::time_point cancellation_start;
    try{checkpoint_tiled_fluorescence_cells(request,(root/"resumed").u8string(),{},[&](std::size_t i,const FluorescenceTileResult&){if(i+1==request.tile_count()/2){cancellation_start=std::chrono::steady_clock::now();cancelled.store(true);}},&cancelled);}catch(const WorkflowCancelled&){stopped=true;cancellation_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-cancellation_start).count();}
    check(stopped && !fs::exists(root/"resumed"/"complete.qft"),"Real interrupted run must preserve incomplete status");Oracle resumed_oracle(*whole,eligible);
    const auto resumed=checkpoint_tiled_fluorescence_cells(request,(root/"resumed").u8string(),{},[&](std::size_t i,const FluorescenceTileResult& tile){resumed_oracle.accept(tile);persisted(root/"resumed",i,tile);});resumed_oracle.finish();
    check(resumed.reused_tiles>0 && resumed.segmented_tiles==resumed.tiles-resumed.reused_tiles && resumed_oracle.cells==baseline_oracle.cells && resumed.input_sha256==baseline.input_sha256 && resumed.output_sha256==baseline.output_sha256 && resumed.state==baseline.state && resumed.unresolved_tiles==baseline.unresolved_tiles,"Real interrupted/resumed geometry/moments/identity/status differ");
    for(const auto& file:fs::directory_iterator(root/"baseline"))check(bytes(file.path())==bytes(root/"resumed"/file.path().filename()),"Real complete/partial persisted records differ after resume");
    t.width=160;const FluorescenceTiledRequest alternate(doc,region,0,p,t);Oracle alternate_oracle(*whole,eligible);
    const auto changed=checkpoint_tiled_fluorescence_cells(alternate,(root/"alternate").u8string(),{},[&](std::size_t,const FluorescenceTileResult& tile){alternate_oracle.accept(tile);});alternate_oracle.finish();
    check(changed.resolved_threshold==baseline.resolved_threshold && alternate_oracle.cells==baseline_oracle.cells,"Real global cell identity/masks/moments depend on tile layout");
    const auto after=fingerprint_file(argv[1]);check(after->sha256==input->sha256,"Admitted real input bytes changed");
    nlohmann::json report={{"dataset",profile.at("dataset")},{"license",profile.at("license")},{"input_sha256",input->sha256},{"width",metadata->width},{"height",metadata->height},{"scope",profile.contains("roi")?"bounded nonzero-origin ROI from actual whole-slide image; not full-slide throughput":"retained fluorescence field ROI, not large whole-slide throughput"},{"roi",{region.region.x,region.region.y,region.region.width,region.region.height}},{"first_channel",region.first_channel},{"channels",region.channels},{"channel_names",metadata->channel_names},{"sample_type",sample_name(metadata->sample_type)},{"cancellation_after_tile_ms",cancellation_ms},{"max_cells",p.max_cells},{"whole_cells",whole->raster().measurements.size()},{"complete_reference_cells",baseline_oracle.expected.size()},{"boundary_reference_cells",boundaries},{"baseline",summary(baseline)},{"resumed",summary(resumed)},{"alternate",summary(changed)},{"exact_masks_moments_ids",true},{"input_unchanged",true}};
    std::ofstream out(root/"real.json");out<<report.dump(2)<<'\n';out.close();check(bool(out),"Write real tiled evidence");std::cout<<report.dump(2)<<'\n';
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

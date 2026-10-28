#include "qupathxx/project_batch.hpp"
#include "qupathxx/cell_tables.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
using namespace qpx;
void require(bool value) {if(!value) throw std::runtime_error("nominal cell batch contract failed");}
template<class F> void rejects(F action) {try {action();} catch(const std::exception&) {return;}throw std::runtime_error("nominal batch rejection missing");}
}
void check_nominal_cell_batches(const std::filesystem::path& root) {
    using namespace qpx;
    NominalCellRoiSampling sampling;sampling.context_x=sampling.context_y=0;sampling.validate();
    ProjectBatchOptions options;options.kind=ProjectBatchKind::Cells;options.cell_nominal=sampling;options.validate();
    auto invalid=options;invalid.cell_grid=GridCellSampling{};rejects([&]{invalid.validate();});
    invalid=options;invalid.kind=ProjectBatchKind::Roi;rejects([&]{invalid.validate();});
    invalid=options;invalid.cell_min_nucleus_area_um2=1;rejects([&]{invalid.validate();});
    invalid=options;invalid.cells.level=0;rejects([&]{invalid.validate();});
    invalid=options;invalid.cell_nominal->downsample=0;rejects([&]{invalid.validate();});
    invalid=options;invalid.cell_nominal->context_x=4097;rejects([&]{invalid.validate();});
    if(!has_project()) {std::cout<<"Nominal cell batch option admission passed; project storage unavailable\n";return;}
    const auto fixture=std::filesystem::path(QPX_NOMINAL_BATCH_FIXTURE)/"source.rgb";
    std::ifstream stream(fixture,std::ios::binary);require(bool(stream));
    const std::vector<std::uint8_t> pixels{std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
    require(pixels.size()==256*192*3);
    Project project;
    auto first=std::make_shared<Document>(Image(256,192,pixels));first->set_calibration(.5,.5);first->add_rectangle({0,0,256,192});
    auto second=std::make_shared<Document>(Image(256,192,pixels));second->set_calibration(.8,.4);second->add_rectangle({32.25,48.5,180.5,100.25});
    const auto image_a=project.add_document(first),image_b=project.add_document(second);
    const StainTransform transform({{.65,.7,.29},{.07,.99,.11}});
    auto& p=options.grid_cells;p.threshold=.4;p.sigma=1;p.cell_expansion=5;p.min_nucleus_area=16;p.max_nucleus_area=512;p.contours.work_bytes=1024*1024;
    const ProjectBatchRequest captured(project,transform,{},options);
    require(captured.parameters_json().find("nominal_cells")!=std::string::npos && captured.parameters_json().find("qpx.rgb-nominal/1")!=std::string::npos);
    auto inactive=options;inactive.cells.threshold=7;inactive.tissue.threshold=8;
    require(ProjectBatchRequest(project,transform,{},inactive).parameters_sha256()==captured.parameters_sha256());
    auto changed=options;changed.cell_nominal->context_x=1;
    require(ProjectBatchRequest(project,transform,{},changed).parameters_sha256()!=captured.parameters_sha256());
    changed=options;changed.cell_nominal->downsample=2;
    require(ProjectBatchRequest(project,transform,{},changed).parameters_sha256()!=captured.parameters_sha256());
    options.cell_nominal->downsample=3;options.grid_cells.min_nucleus_area=10000;
    const auto folder=(root/"nominal-cells").u8string();const auto saved=run_project_batch(captured,folder);
    require(saved.state=="succeeded" && saved.saved==2 && !saved.failed);
    require(first->cell_record_count()==0 && second->cell_record_count()==0);
    for(const auto& image:saved.images) {
        require(image.records==1 && image.objects==1 && (image.image_id==image_a || image.image_id==image_b));
        const auto doc=Project::open(image.path)->document(image.image_id);require(doc->cell_record_count()==1);
        const auto result=doc->cell_records()[0]->result;const auto* n=result->nuclei()->nominal_sampling();require(n && n->source_request().downsample==1.5);
        const auto& r=n->source_request();
        if(image.image_id==image_a) require(r.x_px==0 && r.y_px==0 && r.width_px==256 && r.height_px==192 && n->core_width()==171 && result->features().size()==3 && result->nuclei()->foreground_pixels()==379 && result->cells()->foreground_pixels()==825);
        else require(r.x_px==32 && r.y_px==48 && r.width_px==181 && r.height_px==101 && n->core_width()==121 && n->core_height()==67);
        require(doc->cell_status(doc->cell_records()[0]->id)=="unverified");
        const auto csv=batch_checkpoint_csv(image.path);require(csv.find("nominal_rgb")!=std::string::npos && csv.find("qpx.cell-table/7")!=std::string::npos);
    }
    const auto resumed=run_project_batch(captured,folder);require(resumed.state=="succeeded" && resumed.reused==2 && !resumed.saved);
    for(std::size_t i=0;i<saved.images.size();++i) require(saved.images[i].path==resumed.images[i].path && saved.images[i].pixel_sha256==resumed.images[i].pixel_sha256);
    std::atomic<bool> cancel{true};require(run_project_batch(captured,(root/"nominal-cancelled").u8string(),&cancel).state=="cancelled");
    cancel.store(false);const auto interrupted_folder=(root/"nominal-interrupted").u8string();
    const auto interrupted=run_project_batch(captured,interrupted_folder,&cancel,[&](const auto&){cancel.store(true);});
    require(interrupted.state=="cancelled" && interrupted.saved==1 && interrupted.images.size()==1);
    cancel.store(false);const auto continued=run_project_batch(captured,interrupted_folder,&cancel);
    require(continued.state=="succeeded" && continued.reused==1 && continued.saved==1);
    std::cout<<"Nominal cell batch options capture per-ROI bounds, QCL6 checkpoints, hash-only resume and typed CSV passed\n";
}

#include "qupathxx/project_batch.hpp"
#include "qupathxx/input_digest.hpp"
#include "qupathxx/cell_tables.hpp"
#include <filesystem>
#include <iostream>

namespace {
using namespace qpx;
void require_at(bool value,int line) {if(!value) throw std::runtime_error("grid cell frontend contract failed at line "+std::to_string(line));}
#define require(...) require_at((__VA_ARGS__),__LINE__)
template<class F> void rejects(F f) {try {f();} catch(const std::exception&) {return;}throw std::runtime_error("grid frontend admission did not reject");}
std::shared_ptr<Document> image(double x=.5,double y=.25) {
    std::vector<std::uint8_t> pixels(6*9*3,255);for(int row=3;row<6;++row) for(int col=2;col<4;++col) for(int c=0;c<3;++c) pixels[(row*6+col)*3+c]=32;
    auto doc=std::make_shared<Document>(Image(6,9,pixels));doc->set_calibration(x,y);doc->add_rectangle({0,0,6,9});return doc;
}
GridCellParameters parameters() {GridCellParameters p;p.units=GridCellUnits::Micrometres;p.sigma=0;p.log_prominence=.2;p.min_nucleus_area=.75;p.cell_expansion=.9;p.contours.work_bytes=1024*1024;return p;}
}
int write_grid_cell_cli_fixture(const std::string& path) {
    using namespace qpx;if(!has_project()) throw std::runtime_error("grid CLI fixture requires project storage");Project p;const auto id=p.add_document(image(),"Grid CLI input");p.save_new(path);
    std::cout<<"{\"schema\":\"qpx.grid-cli-input/1\",\"image\":"<<id<<",\"object\":1}\n";return 0;
}
void check_grid_cell_batches(const std::filesystem::path& root) {
    using namespace qpx;const StainTransform transform({{1,0,0},{0,1,0}});const GridCellSampling sampling{3,3,1,1};const auto p=parameters();
    auto doc=image();const auto fractional=doc->add_rectangle({.2,.3,5.3,8.4});const auto request=CellRequest::for_grid_roi(*doc,fractional,transform,sampling,p);
    const auto& grid=*request.scientific_grid();require(grid.x().origin()==0 && grid.y().origin()==0 && grid.x().extent()==6 && grid.y().extent()==9 && grid.x().samples()==3 && grid.y().context()==1);
    require(request.roi().equals(doc->annotation(fractional).geometry));const CellRequest explicit_request(*doc,fractional,transform,ScientificGrid(6,9,{0,6,3,1},{0,9,3,1}),p);
    require(input_sha256(request)==input_sha256(explicit_request));const auto result=detect_cells(request);require(result->features().size()==1 && result->features()[0].cell_pixels==3);
    auto invalid=sampling;invalid.samples_x=0;rejects([&]{CellRequest::for_grid_roi(*doc,fractional,transform,invalid,p);});invalid=sampling;invalid.context_y=4097;rejects([&]{invalid.validate();});
    Document tiny(Image(2,2,std::vector<std::uint8_t>(12,255)));const auto id=tiny.add_rectangle({.05,.05,.1,.1});auto pixels=p;pixels.units=GridCellUnits::LevelZeroPixels;
    require(detect_cells(CellRequest::for_grid_roi(tiny,id,transform,{5,5,0,0},pixels))->nuclei()->samples()==1);
    if(!has_project()) {std::cout<<"Grid ROI count planning and tiny scientific centers passed; batch storage unavailable\n";return;}
    Project project;auto first=image(),second=image(1,1);const auto a=project.add_document(first),b=project.add_document(second);
    ProjectBatchOptions options;options.kind=ProjectBatchKind::Cells;options.cell_grid=sampling;options.grid_cells=p;options.publish_cells=has_geos();
    const ProjectBatchRequest captured(project,transform,{},options);require(captured.parameters_json().find("\"grid_cells\":{")!=std::string::npos && captured.parameters_json().find("\"samples_x\":3")!=std::string::npos);
    auto changed=options;changed.cell_grid->samples_x=4;require(ProjectBatchRequest(project,transform,{},changed).parameters_sha256()!=captured.parameters_sha256());
    changed=options;changed.grid_cells.cell_expansion=1.;require(ProjectBatchRequest(project,transform,{},changed).parameters_sha256()!=captured.parameters_sha256());
    changed=options;changed.cell_min_nucleus_area_um2=1.;rejects([&]{changed.validate();});changed=options;changed.kind=ProjectBatchKind::Roi;rejects([&]{changed.validate();});
    std::atomic<bool> cancelled{false};const auto directory=(root/"scientific-grid").u8string();const auto interrupted=run_project_batch(captured,directory,&cancelled,[&](const auto& value){if(value.state=="saved") cancelled.store(true);});
    require(interrupted.state=="cancelled" && interrupted.saved==1);cancelled.store(false);const auto resumed=run_project_batch(captured,directory,&cancelled);
    if(resumed.state!="succeeded") throw std::runtime_error(resumed.error+(resumed.images.empty()?"":resumed.images.back().error));
    require(resumed.reused==1 && resumed.saved==1 && first->cell_record_count()==0 && second->cell_record_count()==0);
    for(const auto& saved:resumed.images) {
        const auto reopened=Project::open(saved.path)->document(saved.image_id);require(reopened->cell_record_count()==1);const auto record=reopened->cell_records()[0];
        require(record->result->grid_parameters() && record->result->features().size()==1);
        const auto expected=saved.image_id==a?3u:1u;require(record->result->features()[0].cell_pixels==expected);
        require(record->result->resolved_grid_parameters()->step_x==(saved.image_id==a?1.:2.));
        const auto csv=batch_checkpoint_csv(saved.path);require(csv.find("grid_parameter_units")!=std::string::npos && csv.find("qpx.cell-table/5")!=std::string::npos);
        if(has_geos()) require(reopened->object_count()==2);
    }
    const auto again=run_project_batch(captured,directory);require(again.state=="succeeded" && again.reused==2 && !again.saved);
    cancelled.store(true);require(run_project_batch(captured,(root/"grid-cancelled").u8string(),&cancelled).state=="cancelled");
    std::cout<<"Grid ROI planning, typed batch recipe, per-image calibration, interrupted resume and QCL5/CSV checkpoints passed\n";
}

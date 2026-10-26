#include "qupathxx/project_batch.hpp"
#include "object_forest_fixture.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
namespace {
void require(bool v) {if(!v) throw std::runtime_error("raw pixel batch check failed");}
void write_tiff(const std::filesystem::path& path) {
    std::vector<std::uint8_t> b(160+8*6*4);
    auto u16=[&](std::size_t at,std::uint16_t v) {b[at]=v&255;b[at+1]=v>>8;};
    auto u32=[&](std::size_t at,std::uint32_t v) {for(int i=0;i<4;++i) b[at+i]=std::uint8_t(v>>(8*i));};
    b[0]=b[1]='I';u16(2,42);u32(4,8);u16(8,12);
    const std::uint32_t tags[][4]={{256,4,1,8},{257,4,1,6},{258,3,2,0x00100010},{259,3,1,1},{262,3,1,1},{273,4,1,160},{277,3,1,2},{278,4,1,6},{279,4,1,8*6*4},{284,3,1,1},{338,3,1,0},{339,3,2,0x00010001}};
    for(std::size_t i=0;i<12;++i) {auto at=10+12*i;u16(at,std::uint16_t(tags[i][0]));u16(at+2,std::uint16_t(tags[i][1]));u32(at+4,tags[i][2]);u32(at+8,tags[i][3]);}
    for(std::size_t i=0;i<48;++i) {u16(160+4*i,std::uint16_t(i));u16(162+4*i,std::uint16_t(i+100));}
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(b.data()),b.size());out.close();require(bool(out));
}
}
void check_raw_pixel_batches(const std::filesystem::path& root) {
    using namespace qpx;namespace fs=std::filesystem;
    if(!has_project() || !has_tiff()) {std::cout<<"Raw pixel batch unavailable: project/TIFF capability OFF\n";return;}
    const auto source=root/"raw-batch.tif";write_tiff(source);auto server=open_tiff(source.u8string());auto doc=std::make_shared<Document>(server);
    const auto object=doc->add_rectangle({0,0,8,6});ScientificRawLattice lattice(8,6,{0,8,8},{0,6,6});ScientificRawOptions selection;selection.channels=2;
    RawPixelFeatureOptions features;features.offsets={0,0};features.scales={1,1};RawPixelRecipe recipe(*server->raw_metadata(),lattice,selection,features);
    std::vector<std::pair<std::string,std::string>> schema;for(const auto& f:recipe.feature_schema()) schema.emplace_back(f.name,f.unit);
    RawPixelClassifier model(ForestModel::from_bytes(qpx_test::object_forest_fixture(schema)),recipe);
    Project project;const auto image=project.add_document(doc);ProjectBatchOptions options;options.kind=ProjectBatchKind::RawPixels;options.raw_pixel_model=model;options.raw_pixels.tile_width=options.raw_pixels.tile_height=2;
    StainTransform transform({{1,0,0},{0,1,0}});ProjectBatchRequest request(project,transform,{},options);
    auto saved=run_project_batch(request,(root/"raw-pixels").u8string());if(saved.state!="succeeded") throw std::runtime_error(saved.error+(saved.images.empty()?"":saved.images[0].error));
    require(saved.saved==1 && saved.images[0].records==1 && saved.images[0].inferred_blocks==12 && saved.images[0].verified_blocks==12 && !doc->raw_pixel_stream_record_count());
    const auto result=Project::open(saved.images[0].path);const auto link=result->document(image)->raw_pixel_stream_records().at(0)->link;
    require(link->object_id()==object && fs::u8path(link->path()).extension()==".qws");RawPixelStreamFile file(link->path());file.verify();require(file.link()->same_content(*link));
    auto reused=run_project_batch(request,(root/"raw-pixels").u8string());require(reused.state=="succeeded" && reused.reused==1 && reused.images[0].hashed_blocks==12 && reused.images[0].verified_blocks==12 && !reused.images[0].inferred_blocks && reused.images[0].path==saved.images[0].path);
    require(batch_checkpoint_csv(saved.images[0].path).find("class_samples")!=std::string::npos);
    // Existing completed images survive interruption; unfinished images own no output.
    auto second=std::make_shared<Document>(server);second->add_rectangle({0,0,8,6});project.add_document(second);const ProjectBatchRequest multi(project,transform,{},options);std::atomic<bool> cancel{false};
    const auto interrupted=run_project_batch(multi,(root/"raw-interrupted").u8string(),&cancel,[&](const auto&) {cancel.store(true);});require(interrupted.state=="cancelled" && interrupted.saved==1);cancel.store(false);
    const auto resumed=run_project_batch(multi,(root/"raw-interrupted").u8string(),&cancel);require(resumed.state=="succeeded" && resumed.reused==1 && resumed.saved==1);
    {std::fstream changed(fs::u8path(link->path()),std::ios::binary|std::ios::in|std::ios::out);changed.seekp(-1,std::ios::end);changed.put('X');}
    require(run_project_batch(request,(root/"raw-pixels").u8string()).state=="failed");
    options.resume=false;require(run_project_batch(ProjectBatchRequest(project,transform,{{image,{object}}},options),(root/"raw-pixels").u8string()).saved==1);
    options.pixel_total_limit=4096;const auto limited=run_project_batch(ProjectBatchRequest(project,transform,{},options),(root/"raw-limit").u8string());require(limited.state=="failed");
    for(const auto& e:fs::recursive_directory_iterator(root/"raw-limit")) require(e.path().extension()!=".qws" && e.path().filename()!="result.qpxproj");
    server->close();std::cout<<"Raw pixel batch: portable model, exact input reuse, QWL1 verification, interruption and owned cleanup passed\n";
}

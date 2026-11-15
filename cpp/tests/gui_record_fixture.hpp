#pragma once
#include "quantitative_tiff_fixture.hpp"
#include "qupathxx/dark_flat_workflow.hpp"
#include "qupathxx/linear_unmixing_workflow.hpp"
#include "qupathxx/marker_rules.hpp"
#include "qupathxx/fluorescence_cell_objects.hpp"
namespace record_fixture {
inline std::shared_ptr<qpx::Document> source(const std::filesystem::path& dir) {
    const auto file=dir/"list-source.tif";qpx_test::raw_pixel_tiff_fixture(file,15,9);
    return std::make_shared<qpx::Document>(qpx::open_tiff(file.u8string()));
}
struct Recipes {std::string correction,unmix,marker,fluorescence;};
inline Recipes seed(qpx::Document& doc,const std::filesystem::path& dir) {
    using namespace qpx;
    const auto field=[&](const char* name,std::uint16_t a,std::uint16_t b){const auto file=dir/name;qpx_test::raw_pixel_tiff_fixture(file,15,9);std::fstream out(file,std::ios::binary|std::ios::in|std::ios::out);out.seekp(160);for(int i=0;i<135;++i)for(auto v:{a,b}){out.put(char(v&255));out.put(char(v>>8));}out.close();if(!out)throw std::runtime_error("cannot write list field fixture");return open_tiff(file.u8string());};
    DarkFlatRequest correction(doc,field("list-dark.tif",10,20),field("list-flat.tif",20,40),{{10,20},0});Recipes result;result.correction=keep_dark_flat_recipe(doc,correction);
    const auto m=doc.server()->raw_metadata();LinearMixingMatrix matrix;matrix.inputs={{0,m->channel_names.at(0)},{1,m->channel_names.at(1)}};matrix.components={"DNA","Marker"};matrix.coefficients={1,0,0,1};LinearUnmixRequest unmix(doc,matrix);result.unmix=keep_linear_unmix_recipe(doc,unmix);
    MarkerRule rule;rule.gates={{"DNA",0}};rule.nodes={{MarkerOperation::Gate,0,{}}};result.marker=keep_marker_rule(doc,rule);
    RawRegionRequest region;region.region={0,0,0,15,9};region.channels=2;FluorescenceCellOptions cells;cells.threshold=100;FluorescenceCellRequest request(doc,region,0,cells,{0,0});result.fluorescence=keep_fluorescence_cell_run(doc,request,*detect_fluorescence_cells(request));
    return result;
}
}

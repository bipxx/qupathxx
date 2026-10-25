#include "qupathxx/cell_tables.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/object_classifier_archive.hpp"
#include "../object_classifier_archive.hpp"
#include "../sha256.hpp"
#include "object_forest_fixture.hpp"
#include "grid_classifier_archive_reference.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>

namespace {
using namespace qpx;
void checked(bool v,int line) {if(!v) throw std::runtime_error("grid cell consumer check failed at line "+std::to_string(line));}
#define require(v) checked((v),__LINE__)
template<class F> void rejects(F f) {try {f();} catch(const std::exception&) {return;}throw std::runtime_error("grid cell consumer did not reject invalid input");}
using Csv=std::vector<std::vector<std::string>>;
Csv parse(const std::string& text) {
    Csv rows;std::vector<std::string> row;std::string field;bool quoted=false;
    for(std::size_t i=0;i<text.size();++i) {const auto c=text[i];
        if(c=='"') {if(quoted && i+1<text.size() && text[i+1]=='"') {field+='"';++i;}else quoted=!quoted;}
        else if(!quoted && c==',') {row.push_back(field);field.clear();}
        else if(!quoted && c=='\n') {if(!field.empty() && field.back()=='\r') field.pop_back();row.push_back(field);rows.push_back(row);row.clear();field.clear();}
        else field+=c;
    }
    require(!quoted && row.empty() && field.empty());for(const auto& r:rows) require(r.size()==rows[0].size());return rows;
}
std::map<std::string,std::string> mapped(const Csv& rows,std::size_t index) {
    std::map<std::string,std::string> out;for(std::size_t i=0;i<rows[0].size();++i) require(out.emplace(rows[0][i],rows[index][i]).second);return out;
}
std::vector<std::uint8_t> pixels() {std::vector<std::uint8_t> out(6*9*3,255);for(unsigned y=3;y<6;++y) for(unsigned x=2;x<4;++x) for(unsigned c=0;c<3;++c) out[(y*6+x)*3+c]=32;return out;}
std::uint64_t integer(const std::vector<std::uint8_t>& b,std::size_t at) {require(at+8<=b.size());std::uint64_t v=0;for(unsigned i=0;i<8;++i) v|=std::uint64_t(b[at+i])<<(8*i);return v;}
void integer(std::vector<std::uint8_t>& b,std::size_t at,std::uint64_t v) {require(at+8<=b.size());for(unsigned i=0;i<8;++i) b[at+i]=std::uint8_t(v>>(8*i));}
void sign(std::vector<std::uint8_t>& b,std::size_t start,std::size_t size) {Sha256 h;h.update(b.data()+start,size-64);const auto digest=h.finish();std::copy(digest.begin(),digest.end(),b.begin()+start+size-64);}
}
void check_grid_cell_consumers() {
    using namespace qpx;
    {
        const auto& data=grid_classifier_archive_reference::bytes;ImageMetadata metadata;metadata.width=1;metadata.height=1;metadata.levels={{1,1,1}};
        const auto decoded=ObjectClassifierArchiveCodec::decode(data,sizeof(data),metadata,nullptr);const auto encoded=ObjectClassifierArchiveCodec::encode(*decoded,nullptr);
        require(encoded.size()==sizeof(data) && std::equal(encoded.begin(),encoded.end(),data));const auto origin=decoded->archive->table()->cell_origin(0);
        require(origin && origin->source->grid_parameters && origin->source->resolved_grid_parameters.step_x==.5 && origin->source->resolved_grid_parameters.step_y==.25);
        std::cout<<"Independent Python QCR1/QCA5 typed grid source bytes decode and canonical re-encode passed\n";
    }
    auto doc=std::make_shared<Document>(Image(6,9,pixels()));doc->set_calibration(.5,.25);const auto parent=doc->add_rectangle({0,0,6,9});
    const StainTransform transform({{1,0,0},{0,1,0}});GridCellParameters p;p.units=GridCellUnits::Micrometres;p.sigma=0;p.log_prominence=.2;p.min_nucleus_area=.75;p.cell_expansion=.9;p.contours.work_bytes=1024*1024;
    const ScientificGrid grid(6,9,{0,6,3,1},{0,9,3,1});const CellRequest request(*doc,parent,transform,grid,p);const auto result=detect_cells(request);
    require(result->features().size()==1);const auto run=doc->keep_cells(request,result);CellTableOptions options;options.include_capture_calibration=true;
    const auto csv=cell_measurements_csv(CellTableRequest(*doc,{run},options));const auto rows=parse(csv);require(rows.size()==2);const auto row=mapped(rows,1);
    require(row.at("table_schema")=="qpx.cell-table/6" && row.at("sampling_grid")=="explicit" && row.at("grid_parameter_units")=="um");
    require(row.at("downsample").empty() && row.at("sigma_sample_px").empty() && row.at("expansion_sample_px").empty() && row.at("context_padding_sample_px").empty());
    require(row.at("sample_step_x_px")=="2" && row.at("sample_step_y_px")=="3" && row.at("grid_context_x")=="1" && row.at("grid_context_y")=="1");
    require(row.at("grid_image_width_px")=="6" && row.at("grid_image_height_px")=="9" && row.at("grid_samples_x")=="3" && row.at("grid_samples_y")=="3");
    require(row.at("grid_resolved_step_x")=="1" && row.at("grid_resolved_step_y")=="0.75" && row.at("grid_capture_pixel_width_um")=="0.5" && row.at("grid_capture_pixel_height_um")=="0.25");
    require(row.at("grid_max_nucleus_area").empty() && row.at("nucleus_sample_area_um2")=="0.75" && row.at("cell_sample_area_um2")=="2.25");
    require(row.at("detector_mode")=="grid-log-bodies" && row.at("grid_input_format")=="qpx.grid-opaque-rgb-area/1");
    options.include_capture_calibration=false;const auto simple=mapped(parse(cell_measurements_csv(CellTableRequest(*doc,{run},options))),1);
    require(simple.at("table_schema")=="qpx.cell-table/5" && simple.at("grid_capture_pixel_width_um")=="0.5");
    CellParameters natural;natural.sigma=0;natural.min_peak_radius=0;natural.min_seed_distance=0;natural.min_nucleus_pixels=1;natural.cell_expansion=0;
    const CellRequest legacy(*doc,parent,transform,natural);const auto legacy_result=detect_cells(legacy);const auto old=doc->keep_cells(legacy,legacy_result);
    const auto legacy_only=cell_measurements_csv(CellTableRequest(*doc,{old}));require(legacy_only.find("sampling_grid")==std::string::npos && legacy_only.find("qpx.cell-table/1")!=std::string::npos);
    options.include_capture_calibration=true;const auto mixed=parse(cell_measurements_csv(CellTableRequest(*doc,{},options)));require(mixed.size()==3);
    const auto old_row=mapped(mixed,2);require(old_row.at("sampling_grid")=="natural" && old_row.at("downsample")=="1" && old_row.at("grid_parameter_units").empty());
    require(old_row.at("grid_image_width_px").empty() && old_row.at("sample_step_x_px")=="1");
    const CellTableRequest frozen(*doc,{},options);const auto frozen_csv=cell_measurements_csv(frozen);doc->set_calibration(1,1);
    require(cell_measurements_csv(frozen)==frozen_csv);const auto changed=mapped(parse(cell_measurements_csv(CellTableRequest(*doc,{run},options))),1);
    require(changed.at("status_at_capture")=="calibration_changed" && changed.at("grid_capture_pixel_width_um")=="0.5" && changed.at("cell_sample_area_um2")=="2.25");
    doc->set_calibration(.5,.25);
    if(!has_geos()) {std::cout<<"Grid cell CSV units, mixed natural records and captured calibration passed; classifier publication requires GEOS\n";return;}
    const auto published=doc->publish_cell_record(run),old_published=doc->publish_cell_record(old);require(published.size()==1 && old_published.size()==1);
    const auto manual=doc->add_cell(Roi::rectangle({0,0,1,1}),Roi::rectangle({.1,.1,.1,.1}),"manual, \"quoted\"");
    options.published_objects=true;const auto objects=parse(cell_measurements_csv(CellTableRequest(*doc,{},options)));require(objects.size()==4);
    bool saw_manual=false;for(std::size_t i=1;i<objects.size();++i) {const auto r=mapped(objects,i);require(r.at("table_schema")=="qpx.cell-object-table/6");
        if(r.at("object_id")==std::to_string(manual)) {saw_manual=true;require(r.at("sampling_grid").empty() && r.at("grid_parameter_units").empty() && r.at("object_name")=="manual, \"quoted\"");}}
    require(saw_manual);
    // Upsampling may produce more compartment samples than native image pixels.
    auto finer=p;finer.min_nucleus_area=.05;const ScientificGrid fine(6,9,{0,6,30,1},{0,9,45,1});const CellRequest upsample(*doc,parent,transform,fine,finer);
    const auto dense=detect_cells(upsample);require(!dense->features().empty() && dense->features()[0].cell_pixels>54);const auto up=doc->keep_cells(upsample,dense);const auto up_published=doc->publish_cell_record(up);
    auto selection=published;selection.insert(selection.end(),old_published.begin(),old_published.end());selection.insert(selection.end(),up_published.begin(),up_published.end());
    ObjectFeatureOptions feature_options;feature_options.nucleus_shape=true;feature_options.cell_rgb=true;const ObjectFeatureRequest features(*doc,selection,feature_options);
    const auto table=extract_object_features(features);std::vector<std::pair<std::string,std::string>> schema;for(const auto& f:table->features()) schema.emplace_back(f.name,f.unit);
    const auto model=ForestModel::from_bytes(qpx_test::object_forest_fixture(schema));const auto prediction=classify_objects(features,model);const auto archive=archive_object_classifier(prediction->table(),model,prediction);
    ObjectClassifierRecord record;record.id=doc->revision()+1;record.name="Grid classifier source audit";record.archive=archive;
    const auto bytes=ObjectClassifierArchiveCodec::encode(record,nullptr);
    // QCR envelope: id, name, applied, vote threshold, changed-ID count, artifact blob.
    const auto blob=4+8+8+record.name.size()+8+8+8;const auto artifact=blob+8,artifact_size=std::size_t(integer(bytes,blob));
    require(std::string(bytes.begin()+artifact,bytes.begin()+artifact+4)==std::string("QCA\5",4));
    const auto restored=ObjectClassifierArchiveCodec::decode(bytes.data(),bytes.size(),doc->metadata(),nullptr);require(ObjectClassifierArchiveCodec::encode(*restored,nullptr)==bytes);
    require(restored->archive->table()->sha256()==archive->table()->sha256() && restored->archive->result()->predictions().size()==selection.size());
    bool saw_grid=false,saw_legacy=false,saw_upsampling=false;
    for(std::size_t i=0;i<selection.size();++i) {const auto origin=*restored->archive->table()->cell_origin(i);const auto& source=*origin.source;
        if(source.scientific_grid) {saw_grid=true;require(source.grid_parameters && source.grid_parameters->units==GridCellUnits::Micrometres && std::isnan(source.downsample));
            require(source.provenance->effective_pixel_width_um==.5 && source.provenance->effective_pixel_height_um==.25);if(source.scientific_grid->x().samples()==30) saw_upsampling=true;
        } else {saw_legacy=true;require(source.parameters.algorithm==CellAlgorithm::DistanceWatershed && source.downsample==1);}}
    require(saw_grid && saw_legacy && saw_upsampling);
    auto load=[&](const auto& packet){ObjectClassifierArchiveCodec::decode(packet.data(),packet.size(),doc->metadata(),nullptr);};
    auto bad=bytes;bad[artifact+3]=6;sign(bad,artifact,artifact_size);sign(bad,0,bad.size());rejects([&]{load(bad);});
    const std::string tag="qpx.grid-log-watershed-bodies/1";const auto found=std::search(bytes.begin()+artifact,bytes.end()-64,tag.begin(),tag.end());require(found!=bytes.end()-64);
    auto at=std::size_t(found-bytes.begin())+tag.size();at+=8+std::size_t(integer(bytes,at));at+=8+std::size_t(integer(bytes,at)); // input SHA and ROI
    require(integer(bytes,at)==4);bad=bytes;integer(bad,at+8,2);sign(bad,artifact,artifact_size);sign(bad,0,bad.size());rejects([&]{load(bad);});
    const auto grid_at=at+152;bad=bytes;integer(bad,grid_at+6*8,0);sign(bad,artifact,artifact_size);sign(bad,0,bad.size());rejects([&]{load(bad);});
    const auto q=grid_at+80+8+std::size_t(integer(bytes,grid_at+80))+96;bad=bytes;bad[q]^=1;sign(bad,artifact,artifact_size);sign(bad,0,bad.size());rejects([&]{load(bad);});
    std::atomic<bool> cancelled{true};rejects([&]{ObjectClassifierArchiveCodec::decode(bytes.data(),bytes.size(),doc->metadata(),&cancelled);});
    ObjectClassifierJob job;require(job.predict(features,model) && job.wait(*doc).state=="ready");job.apply(*doc);require(doc->classifier_record_count()==1);
    if(has_project()) {
        auto project=std::make_shared<Project>();const auto image=project->add_document(doc);const auto path=(std::filesystem::current_path()/(project->identity()+"-grid-consumers.qpxproj")).u8string();project->save(path);
        const auto opened=Project::open(path);const auto reopened=opened->document(image);require(reopened->classifier_record_count()==1);
        const auto historical=reopened->classifier_records()[0]->archive->table();require(historical->sha256()==table->sha256());
        for(std::size_t i=0;i<historical->object_ids().size();++i) require(historical->cell_origin(i)->source->input_sha256==table->cell_origin(i)->source->input_sha256);
        const auto project_csv=parse(cell_measurements_csv(CellTableRequest(*opened,{},options)));require(project_csv.size()>1 && mapped(project_csv,1).at("table_schema")=="qpx.project-cell-object-table/6");
        opened->save(path);require(Project::open(path)->document(image)->classifier_record_count()==1);std::filesystem::remove(std::filesystem::u8path(path));
        std::cout<<"QCA5 mixed grid/natural classifier sources and CSV project save/reopen passed\n";
    }
    std::cout<<"Grid cell CSV schema 5/6, mixed/manual rows, captured units, QCA5 provenance and upsampled source limits passed\n";
}

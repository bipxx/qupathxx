#include "qupathxx/cells.hpp"
#include "qupathxx/input_digest.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/cell_tables.hpp"
#include "qupathxx/object_classifier_archive.hpp"
#include "../cell_archive.hpp"
#include "../object_classifier_archive.hpp"
#include "../geometry_archive.hpp"
#include "../grid_cell_descriptor.hpp"
#include "../sha256.hpp"
#include "../provenance_codec.hpp"
#include "qupathxx/provenance.hpp"
#include "object_forest_fixture.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
using namespace qpx;
void checked(bool value,int line) {if(!value) throw std::runtime_error("nominal cell check failed at line "+std::to_string(line));}
#define verify(value) checked(bool(value),__LINE__)
template<class E,class F> void rejects(F action) {try {action();} catch(const E&) {return;}throw std::runtime_error("nominal cell rejection missing");}
std::vector<std::uint8_t> fixture(const char* name) {
    std::ifstream stream(std::filesystem::path(QPX_NOMINAL_CELL_FIXTURE)/name,std::ios::binary);verify(stream.good());
    return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}
std::vector<std::uint32_t> labels(const char* name) {
    const auto bytes=fixture(name);verify(bytes.size()==171*128*4);std::vector<std::uint32_t> result;
    for(std::size_t i=0;i<bytes.size();i+=4) result.push_back(std::uint32_t(bytes[i]) | std::uint32_t(bytes[i+1])<<8 | std::uint32_t(bytes[i+2])<<16 | std::uint32_t(bytes[i+3])<<24);
    return result;
}
void sign(std::vector<std::uint8_t>& bytes,const Roi& roi) {
    const auto geometry=GeometryArchiveCodec::encode(roi);Sha256 hash;hash.integer(geometry.size());hash.update(geometry.data(),geometry.size());
    hash.update(bytes.data(),bytes.size()-64);const auto checksum=hash.finish();std::copy(checksum.begin(),checksum.end(),bytes.end()-64);
}
struct Counter {std::size_t bytes=0;void integer(std::uint64_t) {bytes+=8;}void real(double) {bytes+=8;}};
struct Reader final:ImageServer {
    std::shared_ptr<ImageServer> source;ImageMetadata info;mutable unsigned reads=0;bool stopped=false,changed=false;
    std::atomic<bool>* cancel_on_read=nullptr;
    explicit Reader(const std::vector<std::uint8_t>& rgb):source(dense_image_server(Image(256,192,rgb))),info(source->metadata()) {}
    const ImageMetadata& metadata() const override {return info;}
    PixelBuffer read_region(const RegionRequest& r) const override {
        ++reads;auto pixels=source->read_region(r);if(changed) pixels.writable_bytes()[0]^=1;
        if(cancel_on_read) cancel_on_read->store(true);return pixels;
    }
    void close() override {stopped=true;source->close();}bool closed() const override {return stopped;}
};
}
void check_nominal_cells() {
    using namespace qpx;
    const auto original=fixture("source.rgb"),processing=fixture("processed.rgb");
    auto reader=std::make_shared<Reader>(original);auto document=std::make_shared<Document>(reader);auto& doc=*document;
    doc.set_calibration(.5,.5);const auto id=doc.add_rectangle({0,0,256,192});
    const StainTransform transform({{.65,.7,.29},{.07,.99,.11}});
    GridCellParameters p;p.threshold=.4;p.sigma=1;p.cell_expansion=5;p.min_nucleus_area=16;p.max_nucleus_area=512;
    p.contours.work_bytes=1024*1024;
    const NominalCellSampling sampling(RgbNominalRequest(0,0,256,192,1.5),0,0);
    const CellRequest request(doc,id,transform,sampling,p);verify(reader->reads==0);
    const auto factory=CellRequest::for_nominal_roi(doc,id,transform,1.5,0,0,p);
    verify(factory.nominal_sampling()->source_request().width_px==256 && factory.bounds().width==256.5 && reader->reads==0);
    {
        Document fractional(Image::demo());const auto object=fractional.add_rectangle({20.25,30.5,80.5,60.25});
        const auto planned=CellRequest::for_nominal_roi(fractional,object,transform,2.3,3,4,p);const auto& n=*planned.nominal_sampling();
        const auto& r=n.source_request();verify(r.x_px==20 && r.y_px==30 && r.width_px==81 && r.height_px==61 && r.downsample==2.3);
        verify(n.context_x()==3 && n.context_y()==4 && planned.roi().equals(fractional.annotation(object).geometry));
    }
    verify(request.nominal_sampling() && !request.scientific_grid() && request.grid_parameters());
    verify(request.bounds().width==256.5 && request.bounds().height==192);
    verify(request.nominal_sampling()->edge_x(171)==256.5 && request.nominal_sampling()->center_x(170)==255.75);
    verify(request.resolved_grid_parameters()->step_x==1.5 && request.resolved_grid_parameters()->step_y==1.5);
    const auto pixels=read_rgb_nominal_region(*reader,sampling.source_request());verify(reader->reads==1);
    for(std::size_t i=0;i<processing.size();++i) verify(processing[i]==pixels.bytes()[i/3*4+i%3]);
    const auto result=detect_cells(request);verify(reader->reads==2);
    verify(std::string(result->algorithm())=="qpx.nominal-rgb-log-watershed-bodies/1" && result->features().size()==3);
    verify(result->nuclei()->labels()==labels("nucleus.u32le") && result->cells()->labels()==labels("body.u32le"));
    verify(result->nuclei()->foreground_pixels()==379 && result->cells()->foreground_pixels()==825);
    verify(result->nuclei()->nominal_sampling() && !result->nuclei()->scientific_grid());
    verify(result->nuclei()->downsample_x()==1.5 && result->nuclei()->sample_edge_x(171)==256.5);
    verify(input_sha256(request)==result->nuclei()->input_sha256() && reader->reads==3);
    for(const auto& f:result->features()) {
        verify(result->calibrated_feature(f.label).cell_sample_area_um2==double(f.cell_pixels)*.75*.75);
        ColorVector means{},od{};std::uint64_t count=0;
        for(std::size_t i=0;i<result->cells()->labels().size();++i) if(result->cells()->labels()[i]==f.label) {
            ++count;for(unsigned c=0;c<3;++c) {const auto value=processing[3*i+c];means[c]+=value;od[c]+=std::max(0.,std::log10(255./std::max(1.,double(value))));}
        }
        verify(count==f.cell_pixels);for(unsigned c=0;c<3;++c) {
            verify(std::abs(f.cell_rgb[c]-means[c]/double(count))<1e-6);
            verify(std::abs(f.cell_channels[c]-od[c]/double(count))<1e-6);
        }
    }
    const auto bytes=CellArchiveCodec::encode(*result);verify(bytes[3]==6);
    auto load=[&](const auto& packet){return CellArchiveCodec::decode(packet.data(),packet.size(),request.roi(),doc.metadata());};
    const auto historical=load(bytes);verify(reader->reads==3);
    verify(CellArchiveCodec::encode(*historical)==bytes && historical->cells()->labels()==result->cells()->labels());
    verify(historical->nuclei()->nominal_sampling()->source_request().downsample==1.5);
    ProvenanceCodec::attach(historical->nuclei(),result->nuclei()->provenance());
    auto changed_provenance=std::make_shared<AnalysisProvenance>(*result->nuclei()->provenance());++changed_provenance->captured_unix_ms;
    rejects<std::invalid_argument>([&]{ProvenanceCodec::attach(historical->nuclei(),changed_provenance);});
    rejects<std::invalid_argument>([&]{ProvenanceCodec::attach(historical->nuclei(),{});});
    Counter prefix;cell_detail::grid_parameters_write(prefix,p);const auto geometry_at=4+prefix.bytes;
    for(const auto offset:{geometry_at,geometry_at+16,geometry_at+32,geometry_at+48,geometry_at+56}) {
        auto bad=bytes;for(unsigned i=0;i<8;++i) bad[offset+i]=255;sign(bad,request.roi());rejects<std::exception>([&]{load(bad);});
    }
    auto bad=bytes;bad[3]=7;sign(bad,request.roi());rejects<std::exception>([&]{load(bad);});
    bad=bytes;bad[bytes.size()/2]^=1;rejects<std::exception>([&]{load(bad);});
    const auto run=doc.keep_cells(request,result);verify(doc.cell_status(run)=="current");
    const auto csv=cell_measurements_csv(CellTableRequest(doc,{run}));
    verify(csv.find("qpx.cell-table/7")!=std::string::npos && csv.find("nominal_rgb")!=std::string::npos && csv.find("nominal-log-bodies")!=std::string::npos);
    CellTableOptions table_options;table_options.include_capture_calibration=true;
    verify(cell_measurements_csv(CellTableRequest(doc,{run},table_options)).find("qpx.cell-table/8")!=std::string::npos);
    const auto replay=CellRequest::replay(doc,run);verify(replay.nominal_sampling() && doc.verify_cells(run,replay,*detect_cells(replay)));
    reader->changed=true;verify(input_sha256(request)!=result->nuclei()->input_sha256());
    verify(!doc.verify_cells(run,replay,*detect_cells(replay)) && doc.cell_status(run)=="verification_failed");reader->changed=false;
    verify(doc.verify_cells(run,replay,*detect_cells(replay)) && doc.cell_status(run)=="current");
    const NominalCellSampling context(sampling.source_request(),3,4);
    const auto contextual=detect_cells(CellRequest(doc,id,transform,context,p));
    verify(contextual->features().size()==3 && contextual->nuclei()->samples()==result->nuclei()->samples());
    for(std::int64_t y=0;y<128;++y) for(std::int64_t x=0;x<171;++x) {
        const auto i=std::size_t(y*171+x),j=std::size_t((y+4)*context.width()+x+3);
        verify(contextual->nuclei()->labels()[j]==result->nuclei()->labels()[i]);
        verify(contextual->cells()->labels()[j]==result->cells()->labels()[i]);
    }
    const auto contextual_bytes=CellArchiveCodec::encode(*contextual);
    verify(CellArchiveCodec::encode(*load(contextual_bytes))==contextual_bytes);
    auto limited=p;limited.work_bytes=request.estimated_work_bytes();CellRequest exact(doc,id,transform,sampling,limited);
    --limited.work_bytes;const auto before=reader->reads;rejects<std::length_error>([&]{CellRequest denied(doc,id,transform,sampling,limited);});verify(reader->reads==before);
    std::atomic<bool> cancelled{true};rejects<std::runtime_error>([&]{detect_cells(request,&cancelled);});verify(reader->reads==before);
    cancelled.store(false);reader->cancel_on_read=&cancelled;rejects<std::runtime_error>([&]{detect_cells(request,&cancelled);});reader->cancel_on_read=nullptr;verify(reader->reads==before+1);
    if(has_geos()) {
        const auto ids=doc.publish_cell_record(run);verify(ids.size()==3 && doc.cell_object_status(ids[0])=="current");
        verify(doc.undo() && doc.redo());
        ObjectFeatureOptions options;options.cell_rgb=true;const ObjectFeatureRequest features(doc,ids,options);const auto table=extract_object_features(features);
        std::vector<std::pair<std::string,std::string>> schema;for(const auto& f:table->features()) schema.emplace_back(f.name,f.unit);
        const auto model=ForestModel::from_bytes(qpx_test::object_forest_fixture(schema));const auto classified=classify_objects(features,model);
        ObjectClassifierRecord record;record.id=doc.revision()+1;record.name="Nominal RGB cells";record.archive=archive_object_classifier(classified->table(),model,classified);
        const auto packet=ObjectClassifierArchiveCodec::encode(record,nullptr);const auto restored=ObjectClassifierArchiveCodec::decode(packet.data(),packet.size(),doc.metadata(),nullptr);
        const auto origin=restored->archive->table()->cell_origin(0);verify(origin && origin->source->nominal_sampling && !origin->source->scientific_grid);
        verify(origin->source->nominal_sampling->source_request().downsample==1.5);
        verify(ObjectClassifierArchiveCodec::encode(*restored,nullptr)==packet);
        std::cout<<"Nominal cell original-coordinate contours, publication, undo/redo and QCA6 classifier origins passed\n";
    }
    if(has_project()) {
        // The instrumented external reader above intentionally has no dense
        // embedding API. Exercise persistence with a real original dense image.
        auto dense=std::make_shared<Document>(Image(256,192,original));dense->set_calibration(.5,.5);
        const auto object=dense->add_rectangle({0,0,256,192});const CellRequest input(*dense,object,transform,sampling,p);
        const auto detected=detect_cells(input);const auto record=dense->keep_cells(input,detected);const auto saved=CellArchiveCodec::encode(*detected);
        auto project=std::make_shared<Project>();const auto image=project->add_document(dense);
        const auto path=(std::filesystem::current_path()/(project->identity()+"-nominal-cells.qpxproj")).u8string();project->save(path);
        const auto reopened=Project::open(path);auto d=reopened->document(image);verify(d->cell_status(record)=="unverified");
        verify(CellArchiveCodec::encode(*d->cell_record(record)->result)==saved);
        const auto fresh=CellRequest::replay(*d,record);verify(d->verify_cells(record,fresh,*detect_cells(fresh)) && d->cell_status(record)=="current");
        reopened->save(path);verify(CellArchiveCodec::encode(*Project::open(path)->document(image)->cell_record(record)->result)==saved);
        std::filesystem::remove(std::filesystem::u8path(path));
        std::cout<<"QCL6 project save/reopen, source-bound replay, verification and second save passed\n";
    }
    doc.set_calibration(.5,.25);verify(doc.cell_status(run)=="calibration_changed");rejects<std::invalid_argument>([&]{CellRequest::replay(doc,run);});
    auto physical=p;physical.units=GridCellUnits::Micrometres;const CellRequest anisotropic(doc,id,transform,sampling,physical);
    verify(anisotropic.resolved_grid_parameters()->step_x==.75 && anisotropic.resolved_grid_parameters()->step_y==.375);
    doc.clear_calibration();rejects<std::invalid_argument>([&]{CellRequest unknown(doc,id,transform,sampling,physical);});
    const auto unknown=detect_cells(CellRequest(doc,id,transform,sampling,p));verify(!unknown->calibrated_feature(1).cell_sample_area_um2);
    {
        auto compatible=p;compatible.processing=CellProcessingPolicy::ImageJ154p;
        const auto before=reader->reads;
        rejects<std::invalid_argument>([&]{CellRequest unsupported(doc,id,transform,ScientificGrid(256,192,{0,256,171,0},{0,192,128,0}),compatible);});
        verify(reader->reads==before);
        auto invalid=compatible;invalid.processing=static_cast<CellProcessingPolicy>(3);rejects<std::invalid_argument>([&]{invalid.validate();});
        const auto captured=CellRequest(doc,id,transform,sampling,compatible);const auto detected=detect_cells(captured);
        verify(std::string(detected->algorithm())=="qpx.nominal-rgb-imagej154p-log-watershed-bodies/1");
        verify(detected->features().size()==3 && detected->nuclei()->foreground_pixels()==379 && detected->cells()->foreground_pixels()==825);
        const auto packet=CellArchiveCodec::encode(*detected);verify(packet[3]==7);
        const auto historical=load(packet);verify(historical->grid_parameters()->processing==CellProcessingPolicy::ImageJ154p && CellArchiveCodec::encode(*historical)==packet);
        const auto kept=doc.keep_cells(captured,detected);const auto replayed=CellRequest::replay(doc,kept);
        verify(replayed.grid_parameters()->processing==CellProcessingPolicy::ImageJ154p && doc.verify_cells(kept,replayed,*detect_cells(replayed)));
        verify(cell_measurements_csv(CellTableRequest(doc,{kept})).find("qpx.cell-table/9")!=std::string::npos);
        CellTableOptions options;options.include_capture_calibration=true;
        const auto mixed=cell_measurements_csv(CellTableRequest(doc,{run,kept},options));
        verify(mixed.find("qpx.cell-table/10")!=std::string::npos && mixed.find("processing_policy")!=std::string::npos && mixed.find("quantitative/1")!=std::string::npos && mixed.find("imagej154p/1")!=std::string::npos);
        const auto halo_request=CellRequest::for_nominal_roi(doc,id,transform,1.5,3,4,compatible);const auto halo=detect_cells(halo_request);
        for(std::int64_t y=0;y<128;++y) for(std::int64_t x=0;x<171;++x) {
            const auto i=std::size_t(y*171+x),j=std::size_t((y+4)*halo_request.nominal_sampling()->width()+x+3);
            verify(halo->nuclei()->labels()[j]==detected->nuclei()->labels()[i] && halo->cells()->labels()[j]==detected->cells()->labels()[i]);
        }
        if(has_geos()) {
            const auto ids=doc.publish_cell_record(kept);ObjectFeatureOptions f;f.cell_rgb=true;
            const ObjectFeatureRequest features(doc,ids,f);const auto table=extract_object_features(features);
            std::vector<std::pair<std::string,std::string>> schema;for(const auto& feature:table->features()) schema.emplace_back(feature.name,feature.unit);
            const auto model=ForestModel::from_bytes(qpx_test::object_forest_fixture(schema));const auto classified=classify_objects(features,model);
            ObjectClassifierRecord record;record.id=doc.revision()+1;record.name="ImageJ processing cells";record.archive=archive_object_classifier(classified->table(),model,classified);
            const auto encoded=ObjectClassifierArchiveCodec::encode(record,nullptr);const auto restored=ObjectClassifierArchiveCodec::decode(encoded.data(),encoded.size(),doc.metadata(),nullptr);
            verify(restored->archive->table()->cell_origin(0)->source->grid_parameters->processing==CellProcessingPolicy::ImageJ154p);
            verify(ObjectClassifierArchiveCodec::encode(*restored,nullptr)==encoded);
        }
        std::cout<<"ImageJ cell processing opt-in, QCL7/QCA7 identity, CSV9/10, replay and supported context passed\n";
    }
    for(auto policy:{CellProcessingPolicy::Quantitative,CellProcessingPolicy::ImageJ154p,CellProcessingPolicy::QuPath070})
    for(auto coordinates:{RgbNominalSampling::IntegerOrigin,RgbNominalSampling::AffineInverse}) {
        auto source=std::make_shared<Reader>(original);Document fresh(source);fresh.set_calibration(.5,.5);
        const auto object=fresh.add_rectangle({0,0,256,192});auto parameters=p;parameters.processing=policy;
        const auto captured=CellRequest::for_nominal_roi(fresh,object,transform,1.5,0,0,parameters,coordinates);
        verify(source->reads==0 && captured.nominal_sampling()->source_request().sampling==coordinates);
        const auto detected=detect_cells(captured);
        verify(detected->features().size()==3 && detected->nuclei()->foreground_pixels()==379 && detected->cells()->foreground_pixels()==825);
        const auto packet=CellArchiveCodec::encode(*detected);
        const bool extended=coordinates==RgbNominalSampling::AffineInverse || policy==CellProcessingPolicy::QuPath070;
        verify(packet[3]==(extended?8:policy==CellProcessingPolicy::ImageJ154p?7:6));
        const auto restored=CellArchiveCodec::decode(packet.data(),packet.size(),captured.roi(),fresh.metadata());
        verify(restored->grid_parameters()->processing==policy && restored->nuclei()->nominal_sampling()->source_request().sampling==coordinates);
        verify(CellArchiveCodec::encode(*restored)==packet);
        const auto kept=fresh.keep_cells(captured,detected);const auto replay=CellRequest::replay(fresh,kept);
        verify(replay.nominal_sampling()->source_request().sampling==coordinates && replay.grid_parameters()->processing==policy);
        verify(fresh.verify_cells(kept,replay,*detect_cells(replay)));
        if(coordinates==RgbNominalSampling::AffineInverse) {
            const auto csv=cell_measurements_csv(CellTableRequest(fresh,{kept}));
            verify(csv.find("qpx.cell-table/11")!=std::string::npos && csv.find("sampling_algorithm")!=std::string::npos && csv.find("qpx.rgb-nominal-affine/1")!=std::string::npos);
            Counter fields;cell_detail::grid_parameters_write(fields,parameters,true);
            auto corrupt=packet;corrupt[4+fields.bytes+72]=2;sign(corrupt,captured.roi());
            rejects<std::invalid_argument>([&]{CellArchiveCodec::decode(corrupt.data(),corrupt.size(),captured.roi(),fresh.metadata());});
        }
        if(has_geos()) {
            const auto ids=fresh.publish_cell_record(kept);ObjectFeatureOptions f;f.cell_rgb=true;
            const ObjectFeatureRequest features(fresh,ids,f);const auto table=extract_object_features(features);
            std::vector<std::pair<std::string,std::string>> schema;for(const auto& feature:table->features()) schema.emplace_back(feature.name,feature.unit);
            const auto model=ForestModel::from_bytes(qpx_test::object_forest_fixture(schema));const auto classified=classify_objects(features,model);
            ObjectClassifierRecord record;record.id=fresh.revision()+1;record.name="Versioned cell choices";record.archive=archive_object_classifier(classified->table(),model,classified);
            const auto encoded=ObjectClassifierArchiveCodec::encode(record,nullptr);const auto decoded=ObjectClassifierArchiveCodec::decode(encoded.data(),encoded.size(),fresh.metadata(),nullptr);
            verify(decoded->archive->table()->cell_origin(0)->source->nominal_sampling->source_request().sampling==coordinates);
            verify(decoded->archive->table()->cell_origin(0)->source->grid_parameters->processing==policy);
            verify(ObjectClassifierArchiveCodec::encode(*decoded,nullptr)==encoded);
        }
        if(policy==CellProcessingPolicy::QuPath070) {
            const auto before=source->reads;fresh.set_calibration(.5,.75);parameters.units=GridCellUnits::Micrometres;
            rejects<std::invalid_argument>([&]{CellRequest::for_nominal_roi(fresh,object,transform,1.5,0,0,parameters,coordinates);});
            verify(source->reads==before);fresh.set_calibration(.5,.5);parameters.units=GridCellUnits::LevelZeroPixels;parameters.min_seed_distance=1;
            rejects<std::invalid_argument>([&]{CellRequest::for_nominal_roi(fresh,object,transform,1.5,0,0,parameters,coordinates);});
            verify(source->reads==before);
        }
    }
    std::cout<<"Original-source sampling/processing choices, QCL8 canonical replay, CSV11 and pre-IO metric/filter refusals passed\n";
    reader->close();verify(load(bytes)->cells()->labels()==result->cells()->labels());rejects<std::runtime_error>([&]{detect_cells(request);});
    std::cout<<"Original-source nominal cell captured RGB/masks/features, QCL6 canonical history, replay, calibration, budgets and cancellation passed\n";
    std::cout<<"Native nominal ROI bounds factory and versioned nominal cell CSV descriptors passed\n";
}

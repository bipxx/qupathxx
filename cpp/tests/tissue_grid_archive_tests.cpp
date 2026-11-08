#include "../tissue_archive.hpp"
#include "../provenance_codec.hpp"
#include "../scientific_grid_codec.hpp"
#include "../sha256.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/project_batch.hpp"
#include "qupathxx/measurement_tables.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>
using namespace qpx;
namespace {
void require(bool v,const char* text) {if(!v) throw std::runtime_error(text);}
template<class F> void reject(F f) {bool failed=false;try {f();}catch(const std::exception&) {failed=true;}require(failed,"expected grid archive refusal");}
void resign(std::vector<std::uint8_t>& bytes,const Roi& roi) {
    Sha256 hash;const auto wkt=roi.wkt();hash.integer(wkt.size());hash.update(wkt.data(),wkt.size());hash.update(bytes.data(),bytes.size()-64);
    const auto digest=hash.finish();std::copy(digest.begin(),digest.end(),bytes.end()-64);
}
std::uint64_t get(const std::vector<std::uint8_t>& bytes,std::size_t at) {
    require(at+8<=bytes.size(),"archive integer outside payload");std::uint64_t v=0;
    for(int i=0;i<8;++i) v|=std::uint64_t(bytes[at+i])<<(8*i);return v;
}
void put(std::vector<std::uint8_t>& bytes,std::size_t at,std::uint64_t v) {
    require(at+8<bytes.size()-64,"mutation outside archive prefix");
    for(int i=0;i<8;++i) bytes[at+i]=std::uint8_t(v>>(8*i));
}
std::vector<std::vector<std::string>> parse(const std::string& csv) {
    std::vector<std::vector<std::string>> rows;std::vector<std::string> row;std::string field;bool quoted=false;
    for(std::size_t i=0;i<csv.size();++i) {
        const auto c=csv[i];
        if(c=='"') {if(quoted && i+1<csv.size() && csv[i+1]=='"') {field+='"';++i;}else quoted=!quoted;}
        else if(!quoted && c==',') {row.push_back(field);field.clear();}
        else if(!quoted && c=='\n') {if(!field.empty() && field.back()=='\r') field.pop_back();row.push_back(field);rows.push_back(row);row.clear();field.clear();}
        else field+=c;
    }
    return rows;
}
}
void check_tissue_grid_archives() {
    StainTransform transform({{1,0,0},{0,1,0}});
    auto doc=std::make_shared<Document>(Image(9,7,std::vector<std::uint8_t>(9*7*3,0)));
    const auto object=doc->add_rectangle({1,1,7,5});doc->set_calibration(.5,.25);
    const ScientificGrid grid(9,7,{1,7,5,1},{1,5,3,2});
    std::uint64_t first=0;
    for(auto policy:{RoiSamplingPolicy::GeometryCoverage,RoiSamplingPolicy::PolygonWinding}) for(auto sigma:{0.,.8}) for(auto median:{false,true}) {
        TissueParameters p;p.sampling_policy=policy;p.smoothing_sigma=sigma;p.median_cleanup=median;
        TissueRequest request(*doc,object,transform,grid,p);const auto mask=detect_tissue(request);
        const auto bytes=TissueArchiveCodec::encode(*mask);require(bytes[3]==5,"grid must use QTM5");
        const auto loaded=TissueArchiveCodec::decode(bytes.data(),bytes.size(),mask->roi(),doc->metadata());
        require(TissueArchiveCodec::encode(*loaded)==bytes,"QTM5 canonical round trip");
        require(loaded->labels()==mask->labels() && scientific_grid_equal(*loaded->scientific_grid(),grid),"grid mask/descriptor round trip");
        require(loaded->components().size()==mask->components().size() && loaded->samples()==mask->samples() && loaded->cleaned_pixels()==mask->cleaned_pixels(),"grid statistics round trip");
        for(std::size_t i=0;i<mask->components().size();++i) {
            const auto& a=mask->components()[i];const auto& b=loaded->components()[i];
            require(a.pixels==b.pixels && a.centroid.x==b.centroid.x && a.centroid.y==b.centroid.y && a.bounds.width==b.bounds.width && a.bounds.height==b.bounds.height,"grid component geometry round trip");
        }
        reject([&]{ProvenanceCodec::attach(loaded,{});});
        auto other=std::make_shared<AnalysisProvenance>(*loaded->provenance());other->effective_pixel_width_um=2;
        reject([&]{ProvenanceCodec::attach(loaded,other);});
        reject([&]{doc->keep_tissue(request,loaded);}); // decoded data has no live request ticket
        const auto record=doc->keep_tissue(request,mask);if(!first) first=record;
        require(doc->tissue_status(record)=="current","live grid record current");
        auto copy=*doc;require(copy.tissue_status(record)=="unverified","copied grid record unverified");
        const auto replay=TissueRequest::replay(copy,record);
        require(copy.verify_tissue(record,replay,*detect_tissue(replay)),"grid replay verifies");
        copy.set_calibration(1,.25);require(copy.tissue_status(record)=="source_changed","grid retained calibration status");
        reject([&]{TissueRequest::replay(copy,record);});
        // Prefix mutations are re-signed: failures must enforce the recipe, not merely checksum.
        // QTM5 prefix: magic4, policy/sigma/median24, identities24, nucleus8,
        // feature8, threshold8, eleven parameter integers88, level/downsample16,
        // reserved natural bounds32, then ten signed grid descriptor integers.
        constexpr std::size_t descriptor=212;
        for(const auto mutation:std::array<std::pair<std::size_t,std::uint64_t>,5>{{
            {4,2},{20,2},{180,1},{descriptor+6*8,0},{descriptor+8*8,4097}}}) {
            auto bad=bytes;put(bad,mutation.first,mutation.second);resign(bad,mask->roi());
            reject([&]{TissueArchiveCodec::decode(bad.data(),bad.size(),mask->roi(),doc->metadata());});
        }
        std::atomic<bool> cancel{true};reject([&]{TissueArchiveCodec::decode(bytes.data(),bytes.size(),mask->roi(),doc->metadata(),&cancel);});
    }
    {
        TissueParameters p;const ScientificGrid signed_grid(9,7,{-1,7,5,1},{0,7,3,0});
        const auto request=TissueRequest(*doc,object,transform,signed_grid,p);const auto mask=detect_tissue(request);
        const auto bytes=TissueArchiveCodec::encode(*mask);const auto restored=TissueArchiveCodec::decode(bytes.data(),bytes.size(),mask->roi(),doc->metadata());
        require(scientific_grid_equal(*restored->scientific_grid(),signed_grid) && TissueArchiveCodec::encode(*restored)==bytes,"signed-origin grid round trip");
        const auto record=doc->keep_tissue(request,mask);
        const auto rows=parse(measurements_csv(MeasurementTableRequest(*doc,MeasurementTableKind::Tissue,{record})));
        const auto column=std::find(rows[0].begin(),rows[0].end(),"grid_origin_x_px")-rows[0].begin();
        require(rows.size()==2 && rows[1].at(std::size_t(column))=="-1","signed-origin CSV4");
        p.threshold=100;const auto empty_request=TissueRequest(*doc,object,transform,grid,p);const auto empty=detect_tissue(empty_request);
        const auto empty_bytes=TissueArchiveCodec::encode(*empty);const auto empty_restored=TissueArchiveCodec::decode(empty_bytes.data(),empty_bytes.size(),empty->roi(),doc->metadata());
        require(empty_restored->components().empty() && empty_restored->samples()==15 && TissueArchiveCodec::encode(*empty_restored)==empty_bytes,"empty grid mask round trip");
        const auto empty_id=doc->keep_tissue(empty_request,empty);
        const auto empty_rows=parse(measurements_csv(MeasurementTableRequest(*doc,MeasurementTableKind::Tissue,{empty_id})));
        require(empty_rows.size()==2 && empty_rows[0].size()==empty_rows[1].size(),"empty grid CSV4 width");
        // Move one foreground sample from eligible core to context. Keep the
        // declared total unchanged: core eligibility must reject the archive.
        const auto saved=doc->tissue_record(first)->result;auto bad=TissueArchiveCodec::encode(*saved);
        const auto payload=std::size_t(556+get(bad,292)); // QTM5 common fields + embedded provenance
        put(bad,payload-16,0);put(bad,payload-8,saved->labels().size());
        auto labels=saved->labels();require(labels.front()==0,"off-core witness starts in background");
        const auto on=std::find_if(labels.begin(),labels.end(),[](auto v){return v!=0;});require(on!=labels.end(),"off-core witness has foreground");
        labels.front()=*on;*on=0;bad.resize(payload);
        for(auto label:labels) for(int i=0;i<4;++i) bad.push_back(std::uint8_t(label>>(8*i)));
        bad.resize(bad.size()+64);resign(bad,saved->roi());
        reject([&]{TissueArchiveCodec::decode(bad.data(),bad.size(),saved->roi(),doc->metadata());});
    }
    // A different grid must not verify a saved result or trigger scalar-downsample access.
    auto copy=*doc;TissueParameters p;
    TissueRequest different(copy,object,transform,ScientificGrid(9,7,{1,7,4,1},{1,5,3,2}),p);
    require(!copy.verify_tissue(first,different,*detect_tissue(different)),"different descriptor does not verify");
    require(copy.tissue_status(first)=="verification_failed","failed grid verification status");
    TissueRequest natural(*doc,object,transform);const auto old=doc->keep_tissue(natural,detect_tissue(natural));
    MeasurementTableRequest mixed(*doc,MeasurementTableKind::Tissue,{old,first});const auto csv=measurements_csv(mixed);const auto rows=parse(csv);
    require(rows.size()==3,"mixed table has two component rows");
    auto index=[&](const char* name) {auto it=std::find(rows[0].begin(),rows[0].end(),name);require(it!=rows[0].end(),"missing CSV4 column");return std::size_t(it-rows[0].begin());};
    bool saw_grid=false,saw_natural=false;
    for(std::size_t i=1;i<rows.size();++i) {
        const auto& row=rows[i];require(row.size()==rows[0].size(),"mixed CSV4 column width");
        require(row[index("table_schema")]=="qpx.tissue-table/4","mixed CSV4 schema");
        if(row[index("sampling_kind")]=="area-rgb") {
            saw_grid=true;require(row[index("downsample")].empty(),"grid has no scalar downsample");
            require(row[index("grid_samples_x")]=="5" && row[index("grid_context_y")]=="2","CSV4 descriptor values");
            require(std::abs(std::stod(row[index("component_area_px2")])-35)<1e-12 && std::abs(std::stod(row[index("component_area_um2")])-4.375)<1e-12,"anisotropic CSV areas");
        } else {saw_natural=true;require(row[index("grid_samples_x")].empty() && row[index("downsample")]=="1","mixed natural fields");}
    }
    require(saw_grid && saw_natural,"both sampling kinds exported");
    const auto old_csv=measurements_csv(MeasurementTableRequest(*doc,MeasurementTableKind::Tissue,{old}));
    require(old_csv.find("qpx.tissue-table/1")!=std::string::npos && old_csv.find("sampling_kind")==std::string::npos,"old-only CSV remains version1");
    MeasurementTableOptions exact;exact.snapshot_budget=mixed.snapshot_bytes();
    require(MeasurementTableRequest(*doc,MeasurementTableKind::Tissue,{old,first},exact).snapshot_bytes()==exact.snapshot_budget,"mixed snapshot exact bound");
    --exact.snapshot_budget;reject([&]{MeasurementTableRequest(*doc,MeasurementTableKind::Tissue,{old,first},exact);});
    if(!has_project()) return;
    auto project=std::make_shared<Project>();const auto image=project->add_document(doc);
    const auto path=std::filesystem::current_path()/(project->identity()+"-tissue-grid.qpxproj");project->save(path.u8string());
    const auto restored=Project::open(path.u8string())->document(image);
    require(restored->tissue_record(first)->result->labels()==doc->tissue_record(first)->result->labels() && restored->tissue_status(first)=="unverified","project grid restart");
    const auto replay=TissueRequest::replay(*restored,first);
    require(restored->verify_tissue(first,replay,*detect_tissue(replay)),"project grid replay verifies");
    {
        auto sampling=std::make_shared<TissueGridSampling>();sampling->samples_x=5;sampling->samples_y=3;sampling->context_x=1;sampling->context_y=2;
        ProjectBatchOptions options;options.kind=ProjectBatchKind::Tissue;options.tissue_grid=sampling;
        const ProjectBatchRequest captured(*project,transform,{},options);const auto identity=captured.parameters_sha256();
        sampling->samples_x=4;require(ProjectBatchRequest(*project,transform,{},options).parameters_sha256()!=identity,"grid batch identity tracks counts");
        const auto directory=std::filesystem::current_path()/(project->identity()+"-grid-snapshot");
        const auto report=run_project_batch(captured,directory.u8string());
        require(report.state=="succeeded" && report.saved==1,"grid batch captured snapshot executes");
        const auto output=Project::open(report.images.at(0).path)->document(image)->latest_tissue(object)->result;
        require(output->scientific_grid() && output->scientific_grid()->x().samples()==5,"C++ mutable alias does not change captured sampling");
        require(run_project_batch(captured,directory.u8string()).reused==1,"grid batch checkpoint resumes");
        options.kind=ProjectBatchKind::Cells;reject([&]{options.validate();});
        // Only retire this test's named output under its current build directory.
        require(directory.parent_path()==std::filesystem::current_path(),"batch output cleanup stays inside test directory");
        std::filesystem::remove_all(directory);
    }
    std::filesystem::remove(path);
#if defined(_MSC_VER) && defined(_WIN64)
    // Pinned MSVC/x64 old archive and exact stored admission boundaries, all three parameter groups.
    auto legacy=Project::open((std::filesystem::path(QUPATHXX_TEST_FIXTURES)/"tissue-grid-v1/pre-grid.qpxproj").u8string());
    const auto d=legacy->document(1);
    constexpr std::array<std::array<std::size_t,6>,3> expected{{
        {{2542,3058,68865,109149,1340,1118720}},
        {{2550,3066,68873,109189,1369,1120936}},
        {{2558,3074,68881,109229,1398,1120936}}
    }};
    for(std::size_t i=0;i<expected.size();++i) {
        const auto id=std::uint64_t(i+3);const auto record=d->tissue_record(id);const auto& e=expected[i];
        require(record->result->retained_bytes()==e[0] && record->payload_bytes()==e[1],"pre-grid retained/payload exact boundaries");
        MeasurementTableOptions opt;opt.snapshot_budget=e[2];
        require(MeasurementTableRequest(*d,MeasurementTableKind::Tissue,{id},opt).snapshot_bytes()==e[2],"pre-grid table exact bound");
        ProjectBatchOptions batch;batch.kind=ProjectBatchKind::Tissue;batch.tissue=record->result->parameters();batch.snapshot_budget=e[3];
        const ProjectBatchRequest request(*legacy,transform,{},batch);
        require(request.snapshot_bytes()==e[3] && request.parameters_json().size()==e[4],"pre-grid batch exact bound/json");
        require(TissueRequest::replay(*d,id).estimated_work_bytes()==e[5],"pre-grid replay exact work bound");
    }
#endif
}

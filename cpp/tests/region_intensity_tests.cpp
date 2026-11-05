#include "qupathxx/analysis.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/project_batch.hpp"
#include "qupathxx/measurement_tables.hpp"
#include "../region_statistics_codec.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <iostream>
#include <memory>
#ifdef QPX_REGION_SQLITE
#include <sqlite3.h>
#endif
using namespace qpx;
namespace {
void check(bool v,const char* text) {if(!v) throw std::runtime_error(text);}
template<class F> void reject(F f) {bool yes=false;try {f();}catch(const std::exception&) {yes=true;}check(yes,"missing region statistics rejection");}
void near(double a,double b) {check(std::abs(a-b)<=1e-12*(1+std::abs(b)),"independent intensity moment differs");}
class Raster final:public ImageServer {
    ImageMetadata metadata_;
public:
    mutable std::uint64_t reads=0;
    Raster() {metadata_.width=4;metadata_.height=4;metadata_.levels={{4,4,1}};}
    const ImageMetadata& metadata() const override {return metadata_;}
    static ColorVector rgb(int x,int y) {return {double(10+x*30+y*7),double(20+x*11+y*40),double(30+x*20+y*15)};}
    PixelBuffer read_region(const RegionRequest& r) const override {
        validate_region(metadata_,r);++reads;PixelBuffer pixels(r.width,r.height);
        for(int y=0;y<r.height;++y) for(int x=0;x<r.width;++x) {
            const int xx=int(r.x)+x,yy=int(r.y)+y;const auto colors=rgb(xx,yy);const auto at=std::size_t(y*r.width+x)*4;
            for(int c=0;c<3;++c) pixels.writable_bytes()[at+c]=std::uint8_t(colors[c]);
            pixels.writable_bytes()[at+3]=(xx==1 && yy==0)?128:255;
        }
        return pixels;
    }
    bool closed() const override {return false;}
    void close() override {}
};
void expected(const ChannelIntensityStatistics& got,const std::vector<ColorVector>& values) {
    for(int c=0;c<3;++c) {
        long double sum=0;double lo=values.front()[c],hi=lo;
        for(const auto& v:values) {sum+=v[c];lo=std::min(lo,v[c]);hi=std::max(hi,v[c]);}
        near(got.sum[c],double(sum));near(got.minimum[c],lo);near(got.maximum[c],hi);
        check(bool(got.sample_standard_deviation)==(values.size()>1),"singleton SD must be absent");
        if(values.size()>1) {
            const auto mean=sum/values.size();long double squares=0;
            for(const auto& v:values) {const auto delta=v[c]-mean;squares+=delta*delta;}
            near((*got.sample_standard_deviation)[c],std::sqrt(double(squares/(values.size()-1))));
        }
    }
}
#ifdef QPX_REGION_SQLITE
void storage_faults(const std::filesystem::path& pristine,std::uint64_t image,std::uint64_t record) {
    struct Fault {const char* name;const char* sql;const char* error;};
    const Fault faults[]={
        {"missing", "DELETE FROM analysis_statistics", "payload/algorithm mismatch"},
        {"orphan", "INSERT INTO analysis_statistics SELECT image_id,'999999',payload FROM analysis_statistics", "orphaned region statistics"},
        {"tampered", "UPDATE analysis_statistics SET payload=zeroblob(length(payload))", "invalid region statistics encoding"},
        {"legacy-parent", "INSERT INTO analysis_statistics SELECT image_id,'4',payload FROM analysis_statistics", "checksum mismatch"},
        {"parent-mean", "UPDATE analysis_records SET mean_r=mean_r+1 WHERE id IN (SELECT id FROM analysis_statistics)", "checksum mismatch"},
        {"downgrade", "PRAGMA user_version=26", "payload/algorithm mismatch"}
    };
    for(const auto& fault:faults) {
        const auto path=std::filesystem::path(pristine.u8string()+"-"+fault.name);
        std::filesystem::copy_file(pristine,path,std::filesystem::copy_options::overwrite_existing);
        sqlite3* raw=nullptr;
        check(sqlite3_open(path.u8string().c_str(),&raw)==SQLITE_OK,"open SQL fault fixture");
        auto db=std::unique_ptr<sqlite3,decltype(&sqlite3_close)>(raw,sqlite3_close);
        check(sqlite3_exec(db.get(),fault.sql,nullptr,nullptr,nullptr)==SQLITE_OK,"apply SQL fault fixture");
        // Close the writer before the actual production loader checks the corruption.
        sqlite3_close(db.release());
        bool rejected=false;
        try {Project::open(path.u8string());}
        catch(const std::invalid_argument& e) {
            check(std::string(e.what()).find(fault.error)!=std::string::npos,e.what());rejected=true;
        }
        check(rejected,"corrupt SQL statistics payload was accepted");
        std::filesystem::remove(path);
        std::cout<<"Region SQL boundary rejected: "<<fault.name<<'\n';
    }
    const auto clean=Project::open(pristine.u8string());
    check(clean->document(image)->analysis_record(record).result.intensity_statistics!=nullptr,"fault checks changed pristine project");
}
#endif
void numeric_cases() {
    auto server=std::make_shared<Raster>();Document doc(server);StainTransform transform({{.65,.70,.29},{.07,.99,.11}});
    const auto rectangle=doc.add_rectangle({0,0,4,4});
    const auto singleton=doc.add_rectangle({0,0,1,1});
    const auto empty=doc.add_rectangle({1,0,1,1});
    const auto cell=doc.add_cell(Roi::rectangle({0,0,4,4}),Roi::rectangle({0,0,1,1}));
    std::uint64_t triangle=0;if(has_geos()) triangle=doc.add_roi(Roi::polygon({{0,0},{3.25,0},{0,3.25}}));
    const auto pixels=server->read_region({0,0,0,4,4});
    for(auto kind:{AnalysisKind::OpticalDensityMeans,AnalysisKind::StainMeans}) for(auto policy:{RoiSamplingPolicy::GeometryCoverage,RoiSamplingPolicy::PolygonWinding}) {
        const auto channels=kind==AnalysisKind::StainMeans?transform.deconvolve(pixels):transform.optical_densities(pixels);
        for(auto object:{rectangle,singleton,cell,triangle}) {
            if(!object) continue;const bool nucleus=object==cell;
            const AnalysisRequest plain(doc,object,transform,kind,nucleus,16,policy);
            const auto request=AnalysisRequest::with_intensity_statistics(doc,object,transform,kind,nucleus,16,policy);
            const auto reads=server->reads;const auto result=analyze(request);
            check(server->reads==reads+1,"statistics must use one source read");
            const auto baseline=analyze(plain);
            check(!baseline.intensity_statistics && baseline.means==result.means && baseline.input_sha256==result.input_sha256,"legacy means/input stream changed");
            std::vector<ColorVector> rgb,transformed;
            for(int y=0;y<4;++y) for(int x=0;x<4;++x) {
                if(x==1 && y==0) continue;
                if((object==singleton || nucleus) && (x || y)) continue;
                if(object==triangle && x+y>2) continue;
                rgb.push_back(Raster::rgb(x,y));const auto at=std::size_t(y*4+x)*3;
                transformed.push_back({channels.values()[at],channels.values()[at+1],channels.values()[at+2]});
            }
            check(result.samples==rgb.size() && result.intensity_statistics->samples==rgb.size(),"ROI/alpha/nucleus statistics domain");
            expected(result.intensity_statistics->rgb,rgb);expected(result.intensity_statistics->channels,transformed);
            const auto bytes=detail::encode_region_statistics(result);
            check(*detail::decode_region_statistics(result,bytes.data(),bytes.size())==*result.intensity_statistics,"region payload round trip");
            auto corrupt=bytes;corrupt[13]^=1;reject([&]{detail::decode_region_statistics(result,corrupt.data(),corrupt.size());});
            auto other=result;other.object_revision++;reject([&]{detail::decode_region_statistics(other,bytes.data(),bytes.size());});
            auto missing=result;missing.intensity_statistics.reset();reject([&]{doc.keep_analysis(request,missing);});
            const auto id=doc.keep_analysis(request,result);check(doc.analysis_status(id)=="current","retained measured result current");
            auto copy=doc;const auto replay=AnalysisRequest::replay(copy,id);
            check(replay.measure_intensity() && copy.verify_analysis(id,replay,analyze(replay)),"measured replay verifies");
        }
    }
    const auto request=AnalysisRequest::with_intensity_statistics(doc,rectangle,transform);
    auto result=analyze(request);auto alias=std::make_shared<CompartmentIntensityStatistics>(*result.intensity_statistics);result.intensity_statistics=alias;
    const auto kept=doc.keep_analysis(request,result);alias->rgb.minimum[0]=-99;
    check(doc.analysis_record(kept).result.intensity_statistics->rgb.minimum[0]>=0,"retained result must freeze an external C++ alias");
    reject([&]{analyze(AnalysisRequest::with_intensity_statistics(doc,empty,transform));});
    std::atomic<bool> cancelled{true};const auto reads=server->reads;reject([&]{analyze(request,&cancelled);});check(server->reads==reads,"pre-cancel before source IO");
    AnalysisJob job;check(job.request(request),"start measured analysis job");doc.rename_annotation(rectangle,"Changed");check(job.wait(doc,rectangle).state=="stale" && !job.result(),"stale measured result unpublished");
}
}
void check_region_intensity(const char* fixture) {
    numeric_cases();if(!has_project()) return;
    auto original=Project::open(fixture);auto legacy=original->document(1);
    constexpr std::array<std::size_t,5> payload{2141,2141,2149,2149,2141},table{67948,67948,67956,67956,67948};
#if defined(_MSC_VER) && defined(_WIN64)
    for(std::size_t i=0;i<5;++i) {
        const auto id=std::uint64_t(i+4);const auto record=legacy->analysis_record(id);
        check(record.payload_bytes()==payload[i],"old region payload boundary changed");
        MeasurementTableOptions options;options.snapshot_budget=table[i];
        check(MeasurementTableRequest(*legacy,MeasurementTableKind::Roi,{id},options).snapshot_bytes()==table[i],"old region table boundary changed");
        const auto transform=StainTransform::from_normalized(record.result.stains,record.result.white);
        ProjectBatchOptions batch;batch.roi_kind=record.result.kind;batch.roi_sampling_policy=record.result.sampling_policy();batch.snapshot_budget=i==2 || i==3?111137:111105;
        check(ProjectBatchRequest(*original,transform,{},batch).snapshot_bytes()==batch.snapshot_budget,"old region batch boundary changed");
    }
#endif
    auto doc=std::make_shared<Document>(*legacy);auto project=std::make_shared<Project>();const auto image=project->add_document(doc);
    const auto old=doc->analysis_record(4);const auto transform=StainTransform::from_normalized(old.result.stains,old.result.white);
    const auto request=AnalysisRequest::with_intensity_statistics(*doc,old.result.object_id,transform,old.result.kind);
    const auto value=analyze(request);const auto id=doc->keep_analysis(request,value);
    const auto mixed=measurements_csv(MeasurementTableRequest(*doc,MeasurementTableKind::Roi,{old.id,id}));
    check(mixed.find("qpx.roi-table/2")!=std::string::npos && mixed.find("sample_sd_channel_3")!=std::string::npos,"measured ROI CSV2 fields");
    std::size_t at=0,columns=0,rows=0;
    while(at<mixed.size()) {const auto end=mixed.find('\n',at);check(end!=std::string::npos,"CSV row ends");const auto count=std::count(mixed.begin()+at,mixed.begin()+end,',');if(!rows) columns=count;else check(count==columns,"mixed CSV rows are rectangular");++rows;at=end+1;}
    check(rows==3,"mixed ROI CSV row count");
    const auto old_only=measurements_csv(MeasurementTableRequest(*doc,MeasurementTableKind::Roi,{old.id}));
    check(old_only.find("qpx.roi-table/1")!=std::string::npos && old_only.find("intensity_statistics_format")==std::string::npos,"old-only ROI CSV unchanged");
    const auto path=std::filesystem::current_path()/(project->identity()+"-region-statistics.qpxproj");project->save(path.u8string());
    const auto reopened=Project::open(path.u8string())->document(image);const auto loaded=reopened->analysis_record(id);
    check(loaded.result.intensity_statistics && *loaded.result.intensity_statistics==*value.intensity_statistics,"project statistics payload survives restart");
    const auto replay=AnalysisRequest::replay(*reopened,id);check(reopened->verify_analysis(id,replay,analyze(replay)),"project measured replay verifies");
#ifdef QPX_REGION_SQLITE
    storage_faults(path,image,id);
#endif
    std::filesystem::remove(path);
}

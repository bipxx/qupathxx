#include "qupathxx/pixel_contours.hpp"
#include "object_forest_fixture.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace qpx;
void checked(bool value,int line) {if(!value) throw std::runtime_error("pixel workflow check failed at line "+std::to_string(line));}
#define check(v) checked((v),__LINE__)
template<class F> void rejects(F f) {try {f();}catch(const std::exception&) {return;}throw std::runtime_error("missing pixel workflow rejection");}
class Pyramid final:public ImageServer {
public:
    ImageMetadata info;mutable std::atomic<unsigned> reads{0};mutable std::atomic<bool> entered{false},release{true};std::atomic<bool> shut{false};
    Pyramid() {info.width=64;info.height=48;info.levels={{64,48,1},{43,32,1.5},{32,24,2}};}
    const ImageMetadata& metadata() const override {return info;}
    PixelBuffer read_region(const RegionRequest& r) const override {
        validate_region(info,r);++reads;entered.store(true);while(!release.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        const auto scale=info.levels[r.level].downsample;const auto gx=std::llround(r.x/scale),gy=std::llround(r.y/scale);PixelBuffer pixels(r.width,r.height);auto& b=pixels.writable_bytes();
        for(std::int64_t y=0;y<r.height;++y) for(std::int64_t x=0;x<r.width;++x) {const auto i=static_cast<std::size_t>(y*r.width+x);const auto xx=gx+x,yy=gy+y;b[i*4]=xx<info.levels[r.level].width/2?0:200;b[i*4+1]=static_cast<std::uint8_t>(yy);b[i*4+2]=17;b[i*4+3]=(xx+yy)%17==0?0:(xx+2*yy)%29==0?128:255;}return pixels;
    }
    void close() override {shut.store(true);}bool closed() const override {return shut.load();}
};
PixelClassifier fixture(const PixelFeatureOptions& options,const StainTransform& transform,std::optional<PixelResolution> resolution={}) {
    std::vector<std::pair<std::string,std::string>> schema;for(const auto& f:pixel_feature_schema(options,transform)) schema.emplace_back(f.name,f.unit);return PixelClassifier(ForestModel::from_bytes(qpx_test::object_forest_fixture(schema)),options,transform,resolution);
}
int main() {try {
    const StainTransform transform({{1,0,0},{0,1,0}});auto source=std::make_shared<Pyramid>();Document doc(source);doc.set_calibration(.5,.25);
    const auto a=doc.add_detection(Roi::rectangle({0,0,32,48}),"A",0,"A"),b=doc.add_detection(Roi::rectangle({32,0,32,48}),"B",0,"B"),duplicate=doc.add_detection(Roi::rectangle({1,1,10,10}),"A duplicate",0,"A");
    PixelTrainingOptions options;options.features.sigmas={0,1};options.tile_size=16;options.max_samples=200;options.max_samples_per_class=60;
    const PixelTrainingRequest request(doc,{b,duplicate,a},transform,options);check(request.matches(doc) && request.resolution().physical() && request.resolution().pixel_width_um==.5 && source->reads==0);
    const auto table=sample_pixel_training(request);check(table->row_count()==120 && table->sampled_class_counts()==std::vector<std::uint64_t>({60,60}) && table->ambiguous_pixels()==0 && table->estimated_work_bytes()<=options.work_bytes);
    std::uint64_t eligible[2]={};for(int y=0;y<48;++y) for(int x=0;x<64;++x) if((x+y)%17 && (x+2*y)%29) ++eligible[x<32?0:1];check(table->eligible_class_pixels()==std::vector<std::uint64_t>({eligible[0],eligible[1]}));
    for(std::size_t i=0;i<table->row_count();++i) {const auto x=table->coordinates()[i*2],y=table->coordinates()[i*2+1];check(table->values()[i*6]==(x<32?0:200) && table->values()[i*6+2]==y);check(table->annotation_ids()[i]==(x<32?a:b));}
    auto other=options;other.tile_size=32;const auto retiled=sample_pixel_training(PixelTrainingRequest(doc,{a,b,duplicate},transform,other));check(table->sha256()==retiled->sha256() && table->input_sha256()!=retiled->input_sha256());
    auto few=options;few.max_scanned_pixels=1;const auto before=source->reads.load();rejects([&]{sample_pixel_training(PixelTrainingRequest(doc,{a,b},transform,few));});check(source->reads==before);
    few=options;few.work_bytes=1024*1024;few.max_samples=100000;few.max_samples_per_class=50000;rejects([&]{sample_pixel_training(PixelTrainingRequest(doc,{a,b},transform,few));});check(source->reads==before);
    std::atomic<bool> cancelled{true};rejects([&]{sample_pixel_training(request,&cancelled);});Document copied=doc;check(!request.matches(copied));
    const auto overlap=doc.add_detection(Roi::rectangle({25,0,14,48}),"Conflict",0,"C");rejects([&]{sample_pixel_training(PixelTrainingRequest(doc,{a,b,overlap},transform,options));});
    doc.remove_annotation(overlap);check(!request.matches(doc));
    const auto conflict_b=doc.add_detection(Roi::rectangle({25,0,14,48}),"B overlap",0,"B");const auto conflicting=sample_pixel_training(PixelTrainingRequest(doc,{a,b,conflict_b},transform,options));check(conflicting->ambiguous_pixels()>0 && conflicting->eligible_class_pixels()[0]<eligible[0]);
    doc.remove_annotation(conflict_b);
    for(int selected_level:{0,1,2}) {
        auto selected=options;selected.level=selected_level;const auto sample=sample_pixel_training(PixelTrainingRequest(doc,{a,b},transform,selected));const auto model=fixture(selected.features,transform,sample->resolution());check(model.bytes()[3]==2 && PixelClassifier::from_bytes(model.bytes()).bytes()==model.bytes());check(pixel_classifier_level(doc,model)==selected_level);
        const auto roi=doc.add_roi(Roi::ellipse({3,4,57,39}));PixelPredictionOptions prediction;prediction.tile_size=16;const PixelPredictionRequest predict(doc,roi,model,prediction);const auto result=predict_pixel_region(predict);check(result->estimated_work_bytes()<=prediction.work_bytes && result->level()==selected_level);
        const auto& level=source->info.levels[selected_level];const auto rgba=source->read_region({0,0,selected_level,level.width,level.height});const auto full=classify_pixels(rgba,model);const auto region=result->level_bounds();std::uint64_t counts[2]={};
        for(std::int64_t y=0;y<result->height();++y) for(std::int64_t x=0;x<result->width();++x) {const auto gx=static_cast<std::int64_t>(region.x)+x,gy=static_cast<std::int64_t>(region.y)+y;const auto i=static_cast<std::size_t>(y*result->width()+x),j=static_cast<std::size_t>(gy*level.width+gx);const bool valid=doc.annotation(roi).geometry.contains({(gx+.5)*level.downsample,(gy+.5)*level.downsample}) && full->valid()[j];check(result->valid()[i]==valid);if(valid) {check(result->labels()[i]==full->labels()[j] && result->vote_fractions()[i]==full->vote_fractions()[j]);++counts[result->labels()[i]-1];}else check(!result->labels()[i] && std::isnan(result->vote_fractions()[i]));}
        check(result->class_counts()==std::vector<std::uint64_t>({counts[0],counts[1]}));prediction.tile_size=32;const auto tiled=predict_pixel_region(PixelPredictionRequest(doc,roi,model,prediction));check(tiled->sha256()==result->sha256());
        rejects([&]{predict_pixel_region(predict,&cancelled);});prediction.work_bytes=1024*1024;rejects([&]{PixelPredictionRequest too_small(doc,roi,model,prediction);});
        PixelContourJob contour_job;
        const PixelPredictionRequest unrelated(doc,roi,model);
        rejects([&]{contour_job.request(unrelated,result);});check(!contour_job.busy() && !contour_job.result());
        const auto reads_before_trace=source->reads.load();
        check(contour_job.request(predict,result) && !contour_job.request(predict,result));
        contour_job.cancel();check(contour_job.wait(doc).state=="cancelled" && !contour_job.result());
        TissueContourOptions limits;limits.work_bytes=1;check(contour_job.request(predict,result,limits));
        const auto failure=contour_job.wait(doc);check(failure.state=="failed" && !failure.error.empty() && !contour_job.result());
        check(contour_job.request(predict,result));const auto traced=contour_job.wait(doc);
        check(traced.generation==3 && traced.phase=="finished" && !contour_job.busy() && source->reads==reads_before_trace);
        if(has_geos()) {
            check(traced.state=="ready" && contour_job.result()->prediction_sha256()==result->sha256());
            for(const auto& c:contour_job.result()->contours()->components()) check(c.pixels==result->class_counts().at(c.label-1));
        }else check(traced.state=="failed" && !contour_job.result());
        PixelPredictionJob job;check(job.request(predict));check(job.wait(doc).state=="ready" && job.result());doc.rename_annotation(roi,"Changed");check(job.poll(doc).state=="stale" && !job.result());if(has_geos()) check(contour_job.poll(doc).state=="stale" && !contour_job.result());
        doc.remove_annotation(roi);
    }
    const auto bare=fixture(options.features,transform);check(bare.bytes()[3]==1);rejects([&]{pixel_classifier_level(doc,bare);});
    PixelResolution bad;bad.pixel_width_um=.5;rejects([&]{bad.validate();});
    auto bound=fixture(options.features,transform,table->resolution());doc.set_calibration(.7,.25);rejects([&]{pixel_classifier_level(doc,bound);});doc.set_calibration(.5,.25);
    if(has_classifier_training()) {auto parameters=ForestParameters{};parameters.trees=8;parameters.max_depth=4;const auto trained=train_pixel_classifier(*table,parameters);check(trained.resolution()->physical());PixelTrainingJob job;check(job.train(PixelTrainingRequest(doc,{a,b},transform,options),parameters));check(job.wait(doc).state=="ready" && job.model() && job.table());doc.rename_annotation(a,"Retired");check(job.poll(doc).state=="stale" && !job.model() && !job.table());}
    {
        auto gated=std::make_shared<Pyramid>();Document blocked(gated);const auto left=blocked.add_detection(Roi::rectangle({0,0,32,48}),"Left",0,"A"),right=blocked.add_detection(Roi::rectangle({32,0,32,48}),"Right",0,"B");
        PixelTrainingJob job;struct Release {std::shared_ptr<Pyramid> p;~Release(){p->release.store(true);}} release{gated};gated->release.store(false);const PixelTrainingRequest r(blocked,{left,right},transform,options);check(job.train(r) && !job.train(r));
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!gated->entered && std::chrono::steady_clock::now()<deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));check(gated->entered);blocked.rename_annotation(left,"Changed");check(job.poll(blocked).state=="cancelling");gated->release.store(true);check(job.wait(blocked).state=="stale" && !job.model());
    }
    std::cout<<"Annotation sampling, overlap/balance, portable resolution, tiled source prediction and stale jobs passed\n";return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}

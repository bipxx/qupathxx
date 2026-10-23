#include "qupathxx/pixel_contours.hpp"
#include "qupathxx/pixel_classifier_archive.hpp"
#include "qupathxx/pixel_stream.hpp"
#include "qupathxx/pixel_stream_file.hpp"
#include "../gui_pixel_archives.hpp"
#include "../sha256.hpp"
#include "object_forest_fixture.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <filesystem>
#include <fstream>
#include <random>
using namespace qpx;
void checked(bool value,int line) {if(!value) throw std::runtime_error("pixel workflow check failed at line "+std::to_string(line));}
#define check(v) checked((v),__LINE__)
template<class F> void rejects(F f) {try {f();}catch(const std::exception&) {return;}throw std::runtime_error("missing pixel workflow rejection");}
class Pyramid final:public ImageServer {
public:
    ImageMetadata info;mutable std::atomic<unsigned> reads{0};mutable std::atomic<bool> entered{false},release{true};std::atomic<bool> shut{false};unsigned fail_after=std::numeric_limits<unsigned>::max();
    Pyramid() {info.width=64;info.height=48;info.levels={{64,48,1},{43,32,1.5},{32,24,2}};}
    const ImageMetadata& metadata() const override {return info;}
    PixelBuffer read_region(const RegionRequest& r) const override {
        validate_region(info,r);if(++reads>fail_after) throw std::runtime_error("injected source read failure");entered.store(true);while(!release.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
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
        const PixelPredictionRequest window_request(doc,roi,model,prediction,Rect{17.25,11.2,18.5,13.7});const auto window=predict_pixel_region(window_request);
        check(window_request.windowed() && window->windowed() && !predict.windowed() && !result->windowed() && window->matches(window_request) && !window->matches(predict));
        const auto window_grid=window->level_bounds();check(window->width()<result->width() && window->height()<result->height());
        for(std::int64_t y=0;y<window->height();++y) for(std::int64_t x=0;x<window->width();++x) {
            const auto i=std::size_t(y*window->width()+x),j=std::size_t((std::int64_t(window_grid.y-region.y)+y)*result->width()+std::int64_t(window_grid.x-region.x)+x);
            check(window->labels()[i]==result->labels()[j] && window->valid()[i]==result->valid()[j]);if(window->valid()[i]) check(window->vote_fractions()[i]==result->vote_fractions()[j]);else check(std::isnan(window->vote_fractions()[i]));
        }
        const auto window_archive=archive_pixel_prediction(window);check(window_archive->bytes()[3]==2 && window_archive->annotations().front().geometry.wkt()==doc.annotation(roi).geometry.wkt());
        const auto reopened_window=PixelClassifierArchive::from_bytes(window_archive->bytes());check(reopened_window->result()->windowed() && reopened_window->result()->sha256()==window->sha256() && reopened_window->bytes()==archive_pixel_prediction(reopened_window->result())->bytes() && !reopened_window->result()->matches(window_request));
        rejects([&]{PixelPredictionRequest invalid(doc,roi,model,prediction,Rect{100,100,2,2});});
        rejects([&]{PixelPredictionRequest invalid(doc,roi,model,prediction,Rect{1,1,0,2});});
        for(std::int64_t y=0;y<result->height();++y) for(std::int64_t x=0;x<result->width();++x) {const auto gx=static_cast<std::int64_t>(region.x)+x,gy=static_cast<std::int64_t>(region.y)+y;const auto i=static_cast<std::size_t>(y*result->width()+x),j=static_cast<std::size_t>(gy*level.width+gx);const bool valid=doc.annotation(roi).geometry.contains({(gx+.5)*level.downsample,(gy+.5)*level.downsample}) && full->valid()[j];check(result->valid()[i]==valid);if(valid) {check(result->labels()[i]==full->labels()[j] && result->vote_fractions()[i]==full->vote_fractions()[j]);++counts[result->labels()[i]-1];}else check(!result->labels()[i] && std::isnan(result->vote_fractions()[i]));}
        check(result->class_counts()==std::vector<std::uint64_t>({counts[0],counts[1]}));
        PixelStreamOptions stream_options;stream_options.block_size=16;stream_options.prediction.tile_size=16;stream_options.prediction.work_bytes=8*1024*1024;
        PixelPredictionStream stream(doc,roi,model,stream_options);check(stream.matches(doc) && !stream.windowed() && stream.completed_blocks()==0 && stream.estimated_work_bytes()<=stream_options.prediction.work_bytes);
        while(auto block=stream.next()) {const auto grid=block->level_bounds();check(block->windowed() && block->width()<=16 && block->height()<=16);
            for(std::int64_t y=0;y<block->height();++y) for(std::int64_t x=0;x<block->width();++x) {const auto i=std::size_t(y*block->width()+x),j=std::size_t((std::int64_t(grid.y-region.y)+y)*result->width()+std::int64_t(grid.x-region.x)+x);check(block->labels()[i]==result->labels()[j] && block->valid()[i]==result->valid()[j]);if(block->valid()[i]) check(block->vote_fractions()[i]==result->vote_fractions()[j]);}
            const auto saved=archive_pixel_prediction(block);check(PixelClassifierArchive::from_bytes(saved->bytes())->result()->sha256()==block->sha256());
        }
        check(stream.state()=="complete" && stream.completed_blocks()==stream.total_blocks() && stream.class_counts()==result->class_counts() && stream.valid_pixels()==result->valid_pixels() && !stream.next());
        PixelPredictionStream cancelled_stream(doc,roi,model,stream_options);const auto reads=source->reads.load();cancelled_stream.cancel();rejects([&]{cancelled_stream.next();});check(cancelled_stream.state()=="cancelled" && !cancelled_stream.completed_blocks() && source->reads==reads);
        {
            struct Output {std::filesystem::path path;bool owned=false;~Output(){if(owned) {std::error_code ec;std::filesystem::remove(path,ec);}}};
            std::random_device random;Output output{std::filesystem::temp_directory_path()/("qupathxx-stream-"+std::to_string(random())+"-"+std::to_string(random())+".qps")};
            PixelPredictionStream writer(doc,roi,model,stream_options);const auto summary=save_pixel_stream(writer,output.path.u8string());output.owned=true;
            PixelStreamFile file(output.path.u8string());check(!file.verified() && file.declared_summary().sha256==summary.sha256 && summary.blocks==writer.total_blocks());file.verify();check(file.verified() && summary.class_counts==result->class_counts());
            std::uint64_t seen=0;for(std::uint64_t i=0;i<summary.blocks;++i) {const auto block=file.read_block(i);seen+=std::uint64_t(block->width())*block->height();check(block->windowed() && !block->matches(predict));}check(seen==std::uint64_t(result->width())*result->height());
            rejects([&]{file.read_block(summary.blocks);});PixelPredictionStream occupied(doc,roi,model,stream_options);rejects([&]{save_pixel_stream(occupied,output.path.u8string());});check(!occupied.completed_blocks());
            PixelPredictionStream replacement(doc,roi,model,stream_options);const auto replaced=save_pixel_stream(replacement,output.path.u8string(),true);PixelStreamFile reopened(output.path.u8string());reopened.verify();check(replaced.class_counts==summary.class_counts);
            auto broken=std::make_shared<Pyramid>();broken->fail_after=1;Document failed(broken);failed.set_calibration(.5,.25);const auto failed_id=failed.add_roi(Roi::rectangle({3,4,57,39}));PixelPredictionStream failed_writer(failed,failed_id,model,stream_options);
            std::ifstream before_file(output.path,std::ios::binary);const std::vector<char> before((std::istreambuf_iterator<char>(before_file)),{});before_file.close();
            rejects([&]{save_pixel_stream(failed_writer,output.path.u8string(),true);});check(failed_writer.completed_blocks()==1);
            std::ifstream after_file(output.path,std::ios::binary);const std::vector<char> after((std::istreambuf_iterator<char>(after_file)),{});check(before==after);after_file.close();
            PixelStreamExportJob export_job;check(export_job.request(doc,roi,model,output.path.u8string(),stream_options,{},true));
            check(!export_job.request(doc,roi,model,output.path.u8string(),stream_options,{},true));
            const auto saved=export_job.wait(doc);check(saved.state=="saved" && saved.summary && saved.summary->class_counts==summary.class_counts && saved.current && saved.completed_blocks==saved.total_blocks && !export_job.busy());
            export_job.cancel();check(export_job.poll(doc).state=="saved");
            check(export_job.request(failed,failed_id,model,output.path.u8string(),stream_options,{},true));const auto failed_export=export_job.wait(failed);check(failed_export.state=="failed" && !failed_export.summary && failed_export.generation==2);
            PixelStreamFile survived(output.path.u8string());survived.verify();check(survived.declared_summary().class_counts==summary.class_counts);
            std::fstream mutate(output.path,std::ios::in|std::ios::out|std::ios::binary);mutate.seekg(-1,std::ios::end);char last;mutate.get(last);mutate.seekp(-1,std::ios::end);mutate.put(last^1);mutate.close();
            rejects([&]{survived.read_block(0);});rejects([&]{survived.verify();});check(!survived.verified());
        }
        prediction.tile_size=32;const auto tiled=predict_pixel_region(PixelPredictionRequest(doc,roi,model,prediction));check(tiled->sha256()==result->sha256());
        rejects([&]{predict_pixel_region(predict,&cancelled);});prediction.work_bytes=1024*1024;rejects([&]{PixelPredictionRequest too_small(doc,roi,model,prediction);});
        const auto archived=archive_pixel_prediction(result);check(archived->purpose()=="prediction" && archived->annotations().front().id==roi);
        const auto restored=PixelClassifierArchive::from_bytes(archived->bytes());check(restored->sha256()==archived->sha256() && restored->result()->labels()==result->labels() && !restored->result()->matches(predict));
        check(restored->result()->sha256()==result->sha256() && restored->result()->input_sha256()==result->input_sha256());
        auto corrupted=archived->bytes();corrupted.back()^=1;rejects([&]{PixelClassifierArchive::from_bytes(corrupted);});
        const auto record=doc.keep_pixel_prediction(predict,archived,"Native pixel result");check(doc.pixel_classifier_record(record)->archive==archived && doc.pixel_classifier_record(record)->captured_next_id>roi && doc.remove_pixel_classifier_record(record));
        const PixelPredictionRequest current(doc,roi,model,result->options());const auto current_result=predict_pixel_region(current);
        PixelContourJob contour_job;
        const PixelPredictionRequest unrelated(doc,roi,model);
        rejects([&]{contour_job.request(unrelated,result);});check(!contour_job.busy() && !contour_job.result());
        const auto reads_before_trace=source->reads.load();
        check(contour_job.request(current,current_result) && !contour_job.request(current,current_result));
        contour_job.cancel();check(contour_job.wait(doc).state=="cancelled" && !contour_job.result());
        TissueContourOptions limits;limits.work_bytes=1;check(contour_job.request(current,current_result,limits));
        const auto failure=contour_job.wait(doc);check(failure.state=="failed" && !failure.error.empty() && !contour_job.result());
        check(contour_job.request(current,current_result));const auto traced=contour_job.wait(doc);
        check(traced.generation==3 && traced.phase=="finished" && !contour_job.busy() && source->reads==reads_before_trace);
        if(has_geos()) {
            check(traced.state=="ready" && contour_job.result()->prediction_sha256()==current_result->sha256());
            for(const auto& c:contour_job.result()->contours()->components()) check(c.pixels==current_result->class_counts().at(c.label-1));
        }else check(traced.state=="failed" && !contour_job.result());
        PixelPredictionJob job;check(job.request(current));check(job.wait(doc).state=="ready" && job.result());doc.rename_annotation(roi,"Changed");check(job.poll(doc).state=="stale" && !job.result());if(has_geos()) check(contour_job.poll(doc).state=="stale" && !contour_job.result());
        doc.remove_annotation(roi);
    }
    {
        auto huge=std::make_shared<Pyramid>();huge->info.width=huge->info.height=32000;huge->info.levels={{32000,32000,1}};
        Document large(huge);large.set_calibration(.5,.25);const auto id=large.add_roi(Roi::rectangle({0,0,32000,32000}));const auto model=fixture(options.features,transform,table->resolution());
        PixelStreamOptions stream_options;stream_options.block_size=128;stream_options.prediction.work_bytes=8*1024*1024;
        rejects([&]{PixelPredictionRequest full(large,id,model,stream_options.prediction);});
        PixelPredictionStream stream(large,id,model,stream_options);check(!huge->reads && stream.total_blocks()==62500 && stream.estimated_work_bytes()<=stream_options.prediction.work_bytes);
        check(stream.next()->width()==128 && stream.next()->height()==128 && stream.completed_blocks()==2 && huge->reads==2);stream.cancel();rejects([&]{stream.next();});
        PixelPredictionStream gated(large,id,model,stream_options);huge->entered.store(false);huge->release.store(false);
        auto worker=std::async(std::launch::async,[&] {try {gated.next();return false;}catch(const std::runtime_error&) {return true;}});
        struct ReleaseStreamRead {std::shared_ptr<Pyramid> source;~ReleaseStreamRead() {source->release.store(true);}} release{huge};
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!huge->entered && std::chrono::steady_clock::now()<deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        check(huge->entered && gated.busy());rejects([&]{gated.next();});gated.cancel();huge->release.store(true);
        check(worker.get() && !gated.busy() && gated.state()=="cancelled" && !gated.completed_blocks() && !gated.valid_pixels());
        std::random_device random;const auto path=std::filesystem::temp_directory_path()/("qupathxx-cancel-export-"+std::to_string(random())+"-"+std::to_string(random())+".qps");
        PixelStreamExportJob export_job;huge->entered.store(false);huge->release.store(false);check(export_job.request(large,id,model,path.u8string(),stream_options));
        const auto export_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!huge->entered && std::chrono::steady_clock::now()<export_deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        const bool entered=huge->entered;export_job.cancel();huge->release.store(true);const auto cancelled_export=export_job.wait(large);
        check(entered && cancelled_export.state=="cancelled" && !cancelled_export.summary && !cancelled_export.completed_blocks && !std::filesystem::exists(path));
    }
    {
        // Float rectangle reporting cannot distinguish adjacent grid integers
        // here. A re-signed prefix must still match the prototype exactly.
        constexpr std::int64_t base=9007199254741120LL;auto far=std::make_shared<Pyramid>();far->info.width=base+1024;far->info.levels={{base+1024,48,1}};
        Document distant(far);distant.set_calibration(.5,.25);const auto id=distant.add_roi(Roi::rectangle({double(base),4,32,32}));const auto model=fixture(options.features,transform,table->resolution());PixelStreamOptions settings;settings.block_size=16;
        std::random_device random;const auto path=std::filesystem::temp_directory_path()/("qupathxx-far-stream-"+std::to_string(random())+"-"+std::to_string(random())+".qps");
        struct RemoveOwned {std::filesystem::path path;bool owned=false;~RemoveOwned(){if(owned) {std::error_code ec;std::filesystem::remove(path,ec);}}} output{path};
        PixelPredictionStream stream(distant,id,model,settings,Rect{double(base),4,16,16});save_pixel_stream(stream,path.u8string());output.owned=true;PixelStreamFile valid(path.u8string());valid.verify();
        std::ifstream input(path,std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(input)),{});input.close();
        auto integer=[&](std::size_t at){std::uint64_t n=0;for(int i=0;i<8;++i) n|=std::uint64_t(static_cast<unsigned char>(bytes[at+i]))<<(8*i);return n;};
        const auto length=integer(60),header_at=68+length;for(int i=0;i<8;++i) bytes[4+i]=static_cast<char>(std::uint64_t(base+1)>>(8*i));
        Sha256 header;header.update(bytes.data(),header_at);const auto header_hash=header.finish();std::copy(header_hash.begin(),header_hash.end(),bytes.begin()+header_at);
        const auto prototype_hash=std::string(bytes.data()+68+length-64,64);Sha256 ordered;const std::string domain="qpx.pixel-stream-content/1";ordered.update(domain.data(),domain.size());ordered.update(header_hash.data(),64);ordered.update(prototype_hash.data(),64);const auto commitment=ordered.finish();std::copy(commitment.begin(),commitment.end(),bytes.end()-128);
        const auto footer_at=bytes.size()-(4+16+model.classes().size()*8+128);Sha256 footer;footer.update(header_hash.data(),64);footer.update(bytes.data()+footer_at,bytes.size()-64-footer_at);const auto footer_hash=footer.finish();std::copy(footer_hash.begin(),footer_hash.end(),bytes.end()-64);
        std::ofstream changed(path,std::ios::binary|std::ios::trunc);changed.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));changed.close();rejects([&]{PixelStreamFile rejected(path.u8string());});
    }
    const auto bare=fixture(options.features,transform);check(bare.bytes()[3]==1);rejects([&]{pixel_classifier_level(doc,bare);});
    PixelResolution bad;bad.pixel_width_um=.5;rejects([&]{bad.validate();});
    auto bound=fixture(options.features,transform,table->resolution());doc.set_calibration(.7,.25);rejects([&]{pixel_classifier_level(doc,bound);});doc.set_calibration(.5,.25);
    if(has_classifier_training()) {auto parameters=ForestParameters{};parameters.trees=8;parameters.max_depth=4;const auto trained=train_pixel_classifier(*table,parameters);check(trained.resolution()->physical());const auto training_archive=archive_pixel_training(table,trained);const auto reopened_training=PixelClassifierArchive::from_bytes(training_archive->bytes());check(reopened_training->table()->values()==table->values() && reopened_training->annotations().size()==3 && reopened_training->model()->bytes()==trained.bytes());PixelTrainingJob job;check(job.train(PixelTrainingRequest(doc,{a,b},transform,options),parameters));check(job.wait(doc).state=="ready" && job.model() && job.table());doc.rename_annotation(a,"Retired");check(job.poll(doc).state=="stale" && !job.model() && !job.table());}
    {
        auto gated=std::make_shared<Pyramid>();Document blocked(gated);const auto left=blocked.add_detection(Roi::rectangle({0,0,32,48}),"Left",0,"A"),right=blocked.add_detection(Roi::rectangle({32,0,32,48}),"Right",0,"B");
        PixelTrainingJob job;struct Release {std::shared_ptr<Pyramid> p;~Release(){p->release.store(true);}} release{gated};gated->release.store(false);const PixelTrainingRequest r(blocked,{left,right},transform,options);check(job.train(r) && !job.train(r));
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!gated->entered && std::chrono::steady_clock::now()<deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));check(gated->entered);blocked.rename_annotation(left,"Changed");check(job.poll(blocked).state=="cancelling");gated->release.store(true);check(job.wait(blocked).state=="stale" && !job.model());
    }
    {
        // Complete preparation before polling to make cancellation/staleness
        // reconciliation deterministic, independent of machine/frame timing.
        Document retained(std::make_shared<Pyramid>());retained.set_calibration(.5,.25);
        const auto id=retained.add_roi(Roi::rectangle({0,0,16,16}));
        auto model=std::make_shared<const PixelClassifier>(fixture(options.features,transform,table->resolution()));
        PixelPredictionRequest request(retained,id,*model);auto result=predict_pixel_region(request);
        detail::GuiPixelArchives archives;std::map<std::uint64_t,std::shared_ptr<const PixelClassifierRecord>> records;
        const auto identity=std::make_shared<const char>(0);const auto revision=retained.revision();
        archives.keep(request,result);archives.wait();archives.cancel();archives.update(retained,records,identity,true);
        check(archives.state()=="cancelled" && retained.revision()==revision && !retained.pixel_classifier_record_count());
        archives.keep(request,result);archives.wait();retained.rename_annotation(id,"Changed during preparation");
        archives.update(retained,records,identity,true);check(archives.state()=="stale" && !retained.pixel_classifier_record_count());
        request=PixelPredictionRequest(retained,id,*model);result=predict_pixel_region(request);
        archives.keep(request,result);archives.wait();archives.update(retained,records,identity,true);
        check(archives.state()=="kept" && retained.pixel_classifier_record_count()==1 && archives.kept());
        check(retained.pixel_classifier_record(archives.kept())->archive->result()==result);
        archives.update(retained,records,std::make_shared<const char>(0),true);
        check(archives.state()=="idle" && !archives.kept() && !archives.selected());
    }
    std::cout<<"Annotation sampling, overlap/balance, portable resolution, tiled source prediction and stale jobs passed\n";return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}

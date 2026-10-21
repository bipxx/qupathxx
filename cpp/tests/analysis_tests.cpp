#include "qupathxx/analysis.hpp"
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>

void check(bool value) {if(!value) throw std::runtime_error("analysis check failed");}
class GateServer final:public qpx::ImageServer {
    qpx::ImageMetadata metadata_;
public:
    mutable std::atomic<bool> entered{false},released{false};
    std::atomic<bool> is_closed{false};
    bool transparent=false;
    GateServer() {metadata_.width=64;metadata_.height=64;metadata_.levels={{64,64,1}};}
    const qpx::ImageMetadata& metadata() const override {return metadata_;}
    qpx::PixelBuffer read_region(const qpx::RegionRequest& r) const override {
        entered.store(true);
        while(!released.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        qpx::PixelBuffer result(r.width,r.height);
        std::fill(result.writable_bytes().begin(),result.writable_bytes().end(),255);
        if(transparent) for(std::size_t i=3;i<result.bytes().size();i+=4) result.writable_bytes()[i]=0;
        return result;
    }
    bool closed() const override {return is_closed.load();}
    void close() override {is_closed.store(true);}
};
int main() {
    try {
        qpx::StainTransform transform({{1,0,0},{0,1,0}});
        qpx::Document document(qpx::Image(2,1,{255,255,255,10,20,30}));
        const auto id=document.add_rectangle({0,0,2,1});
        qpx::AnalysisRequest request(document,id,transform);
        const auto value=qpx::analyze(request);
        check(value.samples==2 && value.tiles==1);
        check(std::abs(value.means[0]-static_cast<float>(qpx::optical_density(10))/2)<1e-12);
        const auto kept=document.keep_analysis(request,value);check(document.analysis_status(kept)=="current");
        check(document.analysis_record_count()==1 && document.latest_analysis(id)->id==kept);
        qpx::Document copy=document;
        check(!request.matches(copy,id));
        check(copy.analysis_status(kept)=="unverified");
        const auto replay=qpx::AnalysisRequest::replay(copy,kept);
        check(copy.verify_analysis(kept,replay,qpx::analyze(replay)) && copy.analysis_status(kept)=="current");
        check(copy.remove_analysis(kept) && document.analysis_record_count()==1);
        auto fresh=qpx::AnalysisRequest(copy,id,transform); copy=document;
        check(!fresh.matches(copy,id));
        qpx::Document moved=std::move(document); check(!request.matches(moved,id));
        auto source=std::make_shared<GateServer>(); qpx::Document blocked(source);
        const auto blocked_id=blocked.add_rectangle({0,0,64,64});
        auto snapshot=qpx::AnalysisRequest(blocked,blocked_id,transform,qpx::AnalysisKind::OpticalDensityMeans,false,16);
        qpx::AnalysisJob job;
        struct Release {std::shared_ptr<GateServer> source;~Release(){source->released.store(true);}} release{source};
        check(job.request(snapshot)); check(!job.request(snapshot));
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(!source->entered.load() && std::chrono::steady_clock::now()<deadline) std::this_thread::yield();
        if(!source->entered.load()) {source->released.store(true);throw std::runtime_error("worker never entered");}
        const auto start=std::chrono::steady_clock::now();
        auto status=job.poll(blocked,blocked_id);
        check(status.state=="running" && !job.result());
        check(std::chrono::steady_clock::now()-start<std::chrono::milliseconds(100));
        blocked.rename_annotation(blocked_id,"Changed");
        status=job.poll(blocked,blocked_id); source->released.store(true);
        check(status.state=="cancelling"); check(job.wait(blocked,blocked_id).state=="stale" && !job.result());
        auto current=qpx::AnalysisRequest(blocked,blocked_id,transform);
        check(job.request(current)); job.cancel();
        check(job.wait(blocked,blocked_id).state=="cancelled" && !job.result());
        check(job.request(current)); check(job.wait(blocked,blocked_id).state=="ready");
        check(job.result()->samples==4096);
        source->close(); check(job.poll(blocked,blocked_id).state=="stale" && !job.result());
        auto empty_source=std::make_shared<GateServer>(); empty_source->released.store(true); empty_source->transparent=true;
        qpx::Document empty(empty_source); const auto empty_id=empty.add_rectangle({0,0,64,64});
        check(job.request(qpx::AnalysisRequest(empty,empty_id,transform)));
        const auto failed=job.wait(empty,empty_id);
        check(failed.state=="failed" && failed.error=="ROI contains no opaque pixel centers" && !job.result());
        std::cout<<"analysis passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}

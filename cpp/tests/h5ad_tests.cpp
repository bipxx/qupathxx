#include "qupathxx/h5ad.hpp"
#include "qupathxx/workflow.hpp"
#include <filesystem>
#include <iostream>
#include <map>
#include <atomic>
#include <chrono>
#include <fstream>
#include <future>
using namespace qpx;
namespace fs=std::filesystem;
void check(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}
template<class F> void rejects(F fn){bool refused=false;try{fn();}catch(const std::exception&){refused=true;}check(refused,"Malformed/unsupported H5AD was accepted");}
H5adOptions options(){H5adOptions p;p.domain=ExpressionValueDomain::Counts;p.value_domain="raw-counts";p.frame.id="authored-spatial";p.frame.source_id="authored-fixture";p.frame.unit=CoordinateUnit::Micrometre;return p;}
int main(){try {
    check(has_h5ad(),"Native H5AD feature unavailable");const fs::path root=QPX_H5AD_FIXTURES;
    const std::map<std::pair<std::uint64_t,std::uint64_t>,std::uint64_t> expected={{{0,1},7},{{0,3},9007199254740993ULL},{{1,0},3},{{1,2},5},{{2,1},11}};
    const std::map<std::pair<std::uint64_t,std::uint64_t>,double> continuous={{{0,1},-1.25},{{0,3},2.5},{{1,0},.125},{{1,2},3.75},{{2,1},11}};
    for(const std::string kind:{"array","csr_matrix","csc_matrix"})for(bool derived:{false,true}){
        std::cerr<<"Checking "<<kind<<(derived?" continuous":" counts")<<std::endl;
        auto p=options();if(derived){p.domain=ExpressionValueDomain::Continuous;p.value_domain="explicit-log-normalized";}
        H5adReader reader((root/(kind+(derived?"-continuous":"")+".h5ad")).u8string(),p);const auto& m=reader.metadata();
        check(m.domain==*p.domain && m.value_domain==p.value_domain && m.encoding==kind,"Declared H5AD interpretation changed");
        check(m.observations==std::vector<std::string>{"spot_c","spot_a","spot_b"} && m.features==std::vector<std::string>{"gene_z","gene_a",u8"gene_\u03b1","gene_b"},"H5AD IDs/order/UTF8 changed");
        check(m.positions.size()==3 && m.positions[0].x==100.25 && m.positions[0].y==-.5 && m.positions[1].x==8 && m.positions[2].y==12.5,"H5AD coordinate axes/order changed");
        check(m.frame.sha256()==p.frame.sha256() && !m.ignored_metadata.empty() && m.payload_bytes<=reader.stream_work_bytes() && reader.stream_work_bytes()<p.work_bytes,"H5AD frame/metadata/work declaration differs");
        check(m.entries==(kind=="array"?12u:5u) && m.source_sha256.size()==64 && m.input_sha256.size()==64,"H5AD source identity/entries absent");
        std::map<std::pair<std::uint64_t,std::uint64_t>,double> got_real;std::map<std::pair<std::uint64_t,std::uint64_t>,std::uint64_t> got;std::size_t entries=0;
        reader.stream([&](const H5adEntry& e){check(e.observation<3 && e.feature<4,"H5AD axes transposed");++entries;if(derived){if(e.continuous)check(got_real.emplace(std::make_pair(e.observation,e.feature),e.continuous).second,"Repeated coordinate");}else if(e.count)check(got.emplace(std::make_pair(e.observation,e.feature),e.count).second,"Repeated coordinate");});
        check(entries==m.entries && (derived?got_real==continuous:got==expected),"Independent dense/CSR/CSC values differ or uint64 precision lost");
        std::atomic<bool> cancel{true};rejects([&]{reader.stream([](const H5adEntry&){},&cancel);});cancel=false;bool cancelled=false;
        try{reader.stream([&](const H5adEntry&){cancel=true;},&cancel);}catch(const WorkflowCancelled&){cancelled=true;}check(cancelled,"Active native H5AD cancellation ignored");
    }
    auto p=options();p.matrix="layers/counts";H5adReader layer((root/"selected-layer.h5ad").u8string(),p);std::uint64_t sum=0;layer.stream([&](const H5adEntry& e){sum+=e.count;});check(sum==9007199254741019ULL,"Selected layer data/axis differs");
    p=options();p.domain.reset();rejects([&]{H5adReader reader((root/"array.h5ad").u8string(),p);});
    for(const char* file:{"duplicate-observation","duplicate-feature","long-id","matrix-shape","spatial-shape","pointer-negative","pointer-order","pointer-end","index-negative","index-range","index-float","duplicate-coordinate","count-negative","count-fractional","nonfinite","future-version","long-attribute","soft-link","external-link","external-storage","virtual","unreviewed-filter"}){
        std::cerr<<"Refusing "<<file<<std::endl;
        rejects([&]{H5adReader reader((root/(std::string(file)+".h5ad")).u8string(),options());reader.stream([](const H5adEntry&){});});
    }
    p=options();p.domain=ExpressionValueDomain::Continuous;p.value_domain="explicit-continuous";rejects([&]{H5adReader reader((root/"array.h5ad").u8string(),p);reader.stream([](const H5adEntry&){});});
    for(int limit=0;limit<4;++limit){p=options();if(limit==0)p.file_bytes=64;if(limit==1)p.max_observations=2;if(limit==2)p.max_entries=4;if(limit==3)p.chunk_bytes=64;rejects([&]{H5adReader reader((root/"array.h5ad").u8string(),p);reader.stream([](const H5adEntry&){});});}
    p=options();p.frame.id="different-frame";H5adReader a((root/"array.h5ad").u8string(),options()),b((root/"array.h5ad").u8string(),p);check(a.metadata().source_sha256!=b.metadata().source_sha256,"Coordinate frame absent from H5AD identity");
    std::promise<void> entered,release,queued;auto entered_future=entered.get_future();auto release_future=release.get_future().share();auto queued_future=queued.get_future();std::atomic<bool> cancel_waiter{false};
    auto holding=std::async(std::launch::async,[&]{bool blocked=false;a.stream([&](const H5adEntry&){if(!blocked){blocked=true;entered.set_value();release_future.wait();}});});
    entered_future.wait();
    auto waiting=std::async(std::launch::async,[&]{queued.set_value();try{b.stream([](const H5adEntry&){},&cancel_waiter);}catch(const WorkflowCancelled&){return true;}return false;});
    queued_future.wait();const bool initially_waiting=waiting.wait_for(std::chrono::milliseconds(50))==std::future_status::timeout;cancel_waiter=true;
    const bool cancelled_while_held=waiting.wait_for(std::chrono::seconds(1))==std::future_status::ready;release.set_value();holding.get();const bool cancelled=waiting.get();
    check(initially_waiting && cancelled_while_held && cancelled,"Queued HDF5 call did not cancel while another native stream held the library");
    const auto copy=fs::temp_directory_path()/("qpx-h5ad-change-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".h5ad");fs::copy_file(root/"array.h5ad",copy);H5adReader changing(copy.u8string(),options());{std::ofstream out(copy,std::ios::binary|std::ios::app);out.put('x');}rejects([&]{changing.require_current();});fs::remove(copy);
    std::cout<<"Native H5AD independent dense/CSR/CSC domain/axis/precision/guard/cancel checks pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

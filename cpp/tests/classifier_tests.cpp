#include "qupathxx/classifier.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
using namespace qpx;
void check(bool b) {if(!b) throw std::runtime_error("classifier check failed");}
template<class F> void rejects(F f) {try {f();} catch(const std::exception&) {return;}throw std::runtime_error("missing classifier rejection");}
void u32(std::vector<std::uint8_t>& b,std::uint32_t n) {for(unsigned i=0;i<4;++i) b.push_back(static_cast<std::uint8_t>(n>>(8*i)));}
void u64(std::vector<std::uint8_t>& b,std::uint64_t n) {u32(b,static_cast<std::uint32_t>(n));u32(b,static_cast<std::uint32_t>(n>>32));}
void str(std::vector<std::uint8_t>& b,const std::string& s) {u32(b,static_cast<std::uint32_t>(s.size()));b.insert(b.end(),s.begin(),s.end());}
// Independently authored two-tree model: first tree splits x<=1; second
// always votes B. An equal vote must choose the first declared class A.
std::vector<std::uint8_t> fixture() {
    std::vector<std::uint8_t> b;u32(b,0x46585051);u32(b,1);u32(b,2);u32(b,2);u32(b,2);u32(b,0);u64(b,1);u64(b,4*1024*1024);u64(b,4);
    str(b,std::string(64,'a'));str(b,"independent-test/1");u32(b,1);str(b,"area");str(b,"um2");u32(b,2);str(b,"A");str(b,"B");u32(b,2);u32(b,0);u32(b,3);u32(b,4);
    for(auto n:std::vector<std::vector<std::uint32_t>>{{0,1,2,0,0x3f800000},{0,0,0,0,0},{0,0,0,1,0},{0,0,0,1,0}}) for(auto x:n) u32(b,x);
    return b;
}
int main() {try {
    const std::vector<ClassifierFeature> schema{{"area","um2"}};
    auto bytes=fixture();auto m=ForestModel::from_bytes(bytes);check(m.bytes()==bytes && m.node_count()==4 && m.tree_count()==2);
    auto p=m.predict({std::nextafter(1.f,0.f),1.f,std::nextafter(1.f,2.f)},schema);
    check(p[0].votes==std::vector<std::uint32_t>({1,1}) && p[1].class_index==0 && p[1].vote_fraction==.5 && p[2].class_index==1 && p[2].vote_fraction==1);
    check(m.sha256()==ForestModel::from_bytes(bytes).sha256());
    rejects([&]{m.predict({1},{{"area","px2"}});});rejects([&]{m.predict({},schema);});rejects([&]{m.predict({std::numeric_limits<float>::quiet_NaN()},schema);});rejects([&]{m.predict({std::numeric_limits<float>::max()},schema);});
    for(std::size_t n=0;n<bytes.size();++n) {auto bad=bytes;bad.resize(n);rejects([&]{ForestModel::from_bytes(bad);});}
    auto bad=bytes;bad.push_back(0);rejects([&]{ForestModel::from_bytes(bad);});
    const auto nodes=bytes.size()-80;
    for(auto mutation:std::vector<std::pair<std::size_t,std::uint32_t>>{{nodes+4,0},{nodes+4,99},{nodes+8,1},{nodes,1},{nodes+16,0x7fc00000},{nodes+20+12,2},{nodes+60+4,3}}) {
        auto b=bytes;for(unsigned i=0;i<4;++i) b[mutation.first+i]=static_cast<std::uint8_t>(mutation.second>>(8*i));rejects([&]{ForestModel::from_bytes(b);});
    }
    std::vector<float> x;std::vector<std::uint32_t> y;for(int i=0;i<120;++i) {x.push_back(float(i));y.push_back(i<40?0:i<80?1:2);}
    ForestParameters params;params.trees=24;params.max_depth=5;
    rejects([&]{train_forest({1,2,3,4},{0,0,1,7},schema,{"A","B"},params);});
    auto limited=params;limited.work_bytes=1024*1024;rejects([&]{train_forest(x,y,schema,{"A","B","C"},limited);});
    if(has_classifier_training()) {
        auto f=train_forest(x,y,schema,{"A","B","C"},params);check(f.sha256()==train_forest(x,y,schema,{"A","B","C"},params).sha256());
        auto reloaded=ForestModel::from_bytes(f.bytes());auto a=f.predict(x,schema);auto b=reloaded.predict(x,schema);unsigned correct=0;for(std::size_t i=0;i<a.size();++i) {check(a[i].votes==b[i].votes);correct+=a[i].class_index==y[i];}check(correct>=115);
        std::string first,second;std::exception_ptr error1,error2;
        std::thread one([&]{try {first=train_forest(x,y,schema,{"A","B","C"},params).sha256();}catch(...) {error1=std::current_exception();}});
        std::thread two([&]{try {second=train_forest(x,y,schema,{"A","B","C"},params).sha256();}catch(...) {error2=std::current_exception();}});one.join();two.join();if(error1) std::rethrow_exception(error1);if(error2) std::rethrow_exception(error2);check(first==f.sha256() && second==first);
    } else rejects([&]{train_forest(x,y,schema,{"A","B","C"},params);});
    std::cout<<"classifier checks passed; training="<<has_classifier_training()<<'\n';return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}

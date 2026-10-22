#include "qupathxx/classifier.hpp"
#include "forest_fixture.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
namespace fs=std::filesystem;
using namespace qpx;
void check(bool b) {if(!b) throw std::runtime_error("classifier IO check failed");}
template<class F> void rejects(F f) {try {f();} catch(const std::exception&) {return;}throw std::runtime_error("missing classifier IO rejection");}
struct Files {
    fs::path root=fs::temp_directory_path()/fs::u8path("qpx-forest-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(std::random_device{}()));
    Files() {if(!fs::create_directory(root)) throw std::runtime_error("classifier temporary directory collision");}
    ~Files() {std::error_code e;for(const auto* name:{"model.qpxforest","image.ppm","large.qpxforest","cancelled.qpxforest"}) fs::remove(root/name,e);fs::remove(root,e);}
};
int main() {try {
    Files files;auto path=(files.root/"model.qpxforest").u8string();auto model=ForestModel::from_bytes(qpx_test::fixture());
    save_forest_model(model,path);check(load_forest_model(path).sha256()==model.sha256());
    rejects([&]{save_forest_model(model,path);});check(load_forest_model(path).sha256()==model.sha256());
    save_forest_model(model,path,true);check(load_forest_model(path).sha256()==model.sha256());
    const auto image=files.root/"image.ppm";{std::ofstream out(image,std::ios::binary);out<<"P6\n1 1\n255\nabc";}
    auto before=fs::file_size(image);rejects([&]{save_forest_model(model,image.u8string(),true);});check(fs::file_size(image)==before);
    rejects([&]{load_forest_model(image.u8string());});rejects([&]{load_forest_model(files.root.u8string());});
    const auto large=files.root/"large.qpxforest";{std::ofstream out(large,std::ios::binary);}fs::resize_file(large,64u*1024*1024+1);
    rejects([&]{load_forest_model(large.u8string());});
    std::atomic<bool> cancel{true};const auto cancelled=files.root/"cancelled.qpxforest";
    rejects([&]{save_forest_model(model,cancelled.u8string(),false,&cancel);});check(!fs::exists(cancelled));
    rejects([&]{load_forest_model(path,&cancel);});
    check(std::distance(fs::directory_iterator(files.root),fs::directory_iterator{})==3);
    std::cout<<"classifier IO checks passed\n";return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}

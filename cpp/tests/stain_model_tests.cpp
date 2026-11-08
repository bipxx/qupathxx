#include "qupathxx/stain_model_io.hpp"
#include "qupathxx/stain_estimation.hpp"
#include "qupathxx/interchange.hpp"
#include "../stain_model_io_internal.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
using namespace qpx;
namespace fs=std::filesystem;
namespace {
void check(bool value,const char* text) {if(!value)throw std::runtime_error(text);}
template<class E=std::invalid_argument,class F> void reject(F fn) {
    bool rejected=false;try {fn();}catch(const E&) {rejected=true;}
    check(rejected,"expected stain model rejection");
}
std::string replace(std::string text,const std::string& from,const std::string& to) {
    const auto at=text.find(from);check(at!=std::string::npos,"test mutation target missing");text.replace(at,from.size(),to);return text;
}
std::string read(const fs::path& path) {
    std::ifstream stream(path,std::ios::binary);check(bool(stream),"test input missing");return {std::istreambuf_iterator<char>(stream),{}};
}
}
int main(int argc,char** argv) {
    try {
        check(argc==2,"expected old estimate fixture");
        const auto model=StainTransform::from_normalized({ColorVector{1,0,0},ColorVector{0,1,0},ColorVector{0,0,-1}},{250,245,230});
        const auto text=stain_model_json(model);
        check(text.find("qpx.stain-model/1")!=std::string::npos && text.find(stain_model_sha256(model))!=std::string::npos,"model wire identity missing");
        const auto root=fs::current_path()/("stain-model-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directory(root);const auto path=root/fs::u8path(u8"model-\u8272.json");save_stain_model(model,path.u8string());
        check(read(path)==text,"model file bytes differ");
        int guard_checks=0;
        reject<std::runtime_error>([&]{detail::save_stain_model_guarded(model,path.u8string(),[&]{if(++guard_checks==2)throw std::runtime_error("injected precommit guard");});});
        check(guard_checks==2 && read(path)==text,"precommit guard did not preserve the old model");
        const auto occupied=root/"occupied";fs::create_directory(occupied);
        reject<std::runtime_error>([&]{save_stain_model(model,occupied.u8string());});check(fs::is_directory(occupied),"failed write replaced directory");
        for(const auto& entry:fs::directory_iterator(root))check(entry.path().filename().u8string().find(".qpx-tmp-")==std::string::npos,"atomic model temporary leaked");
#ifdef QPX_STAIN_MODEL_DISABLED
        const bool enabled=false;
#else
        const bool enabled=has_interchange();
#endif
        if(!enabled) {
            reject<std::runtime_error>([&]{stain_model_from_json(text);});
            reject<std::runtime_error>([&]{load_stain_model(path.u8string());});
            fs::remove(path);fs::remove(occupied);fs::remove(root);
            std::cout<<"Stain model writer works; disabled parser/load capability rejected without JSON dependency\n";return 0;
        }
        const auto loaded=load_stain_model(path.u8string());check(loaded.stains()==model.stains() && loaded.white()==model.white(),"full signed basis/white changed");
        const auto exact=StainTransform::from_normalized({ColorVector{.6,.8,0},ColorVector{.8,-.6,0},ColorVector{0,0,-1}},{250,245,230});
        const auto round=stain_model_from_json(stain_model_json(exact));check(round.stains()==exact.stains() && stain_model_sha256(round)==stain_model_sha256(exact),"normalized doubles were changed");
        const auto legacy=load_stain_model(argv[1]);check(stain_model_sha256(legacy)=="5d983c4a32c0a795f99c20b4199516be041b277b087297e7610cb12ee1033e21","old estimate model changed");
        const auto as_estimate=replace(text,"qpx.stain-model/1","qpx.stain-estimate/1");
        const auto estimate=replace(as_estimate,"{","{\"algorithm\":\"qpx.od-plane-stains/1\",\"historical_note\":\"not adopted as result\",");
        check(stain_model_sha256(stain_model_from_json(estimate))==stain_model_sha256(model),"model-only estimate extraction differs");
        reject([&]{stain_model_from_json("{");});
        reject([&]{stain_model_from_json(replace(text,"{","{\"schema\":\"qpx.stain-model/1\","));});
        reject([&]{stain_model_from_json(replace(text,"qpx.stain-model/1","qpx.stain-model/2"));});
        reject([&]{stain_model_from_json(replace(text,"{","{\"unknown\":0,"));});
        reject([&]{stain_model_from_json(replace(text,"[250,245,230]","[\"250\",245,230]"));});
        reject([&]{stain_model_from_json(replace(text,"[250,245,230]","[true,245,230]"));});
        reject([&]{stain_model_from_json(replace(text,"[250,245,230]","[1e999,245,230]"));});
        reject([&]{stain_model_from_json(replace(text,"[250,245,230]","[0,245,230]"));});
        reject([&]{stain_model_from_json(replace(text,"[1,0,0]","[2,0,0]"));});
        reject([&]{stain_model_from_json(replace(text,"[1,0,0]","[1,0]"));});
        reject([&]{stain_model_from_json(replace(text,"[0,0,-1]","[0,0,1]"));});
        reject([&]{stain_model_from_json(replace(text,stain_model_sha256(model),std::string(64,'0')));});
        reject([&]{stain_model_from_json(replace(estimate,"qpx.od-plane-stains/1","qpx.other/1"));});
        reject([&]{load_stain_model(occupied.u8string());});
        reject([&]{load_stain_model((root/"missing").u8string());});
        reject<std::length_error>([&]{stain_model_from_json(std::string(16*1024*1024+1,' '));});
        const auto large=root/"large.json";{std::ofstream stream(large,std::ios::binary);stream.seekp(16*1024*1024);stream.put(' ');}
        reject<std::length_error>([&]{load_stain_model(large.u8string());});
        check(read(path)==text,"rejected inputs changed the saved model");
        fs::remove(large);fs::remove(path);fs::remove(occupied);fs::remove(root);
        std::cout<<"Stain model exact basis/hash/white, old export, malformed/duplicate/schema/shape/finite/size/file/atomic boundaries passed\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}

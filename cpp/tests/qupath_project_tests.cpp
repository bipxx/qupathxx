#include "qupathxx/qupath_project.hpp"
#include "qupathxx/tma.hpp"
#include "../qupath_saved_data.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iostream>
using namespace qpx;
void check(bool value) {if(!value)throw std::runtime_error("QuPath project check failed");}
template<class F>void rejects(F f){try{f();}catch(const std::exception&){return;}throw std::runtime_error("missing import refusal");}
#include "class_project_checks.hpp"
#include "qupath_polygon_checks.hpp"
int main() {
    try {
        if(!has_project() || !has_interchange()) {rejects([]{preview_qupath_project("missing.qpproj");});std::cout<<"QuPath project capability disabled and refused\n";return 0;}
        polygon_walk_checks();
        const auto fixture=std::filesystem::path(QPX_PROJECT_SOURCE_ROOT)/"tests/fixtures/qupath-project-0.7.0/project/data/1/data.qpdata";
        std::ifstream input(fixture,std::ios::binary);check(bool(input));
        const std::string data(std::istreambuf_iterator<char>(input),{});
        const auto decoded=detail::decode_qupath_saved_data(data);
        check(decoded.issues.empty() && decoded.pixel_width_um==.25 && decoded.pixel_height_um==.5);
        check(decoded.geojson.find("56f6bd47-e267-456a-a2d0-cfb01922135c")!=std::string::npos);
        for(const auto n:{std::size_t(0),std::size_t(4),std::size_t(27),data.size()/2,data.size()-1})rejects([&]{detail::decode_qupath_saved_data(data.substr(0,n));});
        auto appended=data;appended.push_back(char(0x70));rejects([&]{detail::decode_qupath_saved_data(appended);});
        std::atomic<bool> cancelled{true};rejects([&]{detail::decode_qupath_saved_data(data,&cancelled);});
        std::cout<<"Native QuPath saved-data geometry, calibration, truncation, trailing bytes and cancellation passed\n";
        const auto tma_fixture=std::filesystem::path(QPX_PROJECT_SOURCE_ROOT)/"tests/fixtures/qupath-saved-tma-0.7.0/project/data";
        auto saved=[&](int entry) {std::ifstream f(tma_fixture/std::to_string(entry)/"data.qpdata",std::ios::binary);check(bool(f));return std::string(std::istreambuf_iterator<char>(f),{});};
        const auto tma=detail::decode_qupath_saved_data(saved(1));check(tma.issues.empty() && tma.tma_cores.size()==6);
        std::size_t missing=0;for(const auto& [uuid,core]:tma.tma_cores) {check(uuid.size()==36 && core->grid_info()->rows()==2 && core->grid_info()->columns()==3);missing+=core->missing();}check(missing==2);
        const auto legacy=detail::decode_qupath_saved_data(saved(2));bool legacy_case=false;for(const auto& [uuid,core]:legacy.tma_cores)legacy_case|=core->case_id()==u8"legacy \u03b1";check(legacy_case);
        for(int entry=3;entry<=8;++entry)rejects([&]{detail::decode_qupath_saved_data(saved(entry));});
        std::cout<<"Native saved TMA grid: identity slots, missing/case state, legacy fallback and membership refusal passed\n";
        const auto root=std::filesystem::temp_directory_path()/("qpx-import-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(root);
        {std::ofstream f(root/"source.ppm",std::ios::binary);f<<"P6\n2 2\n255\n";for(int i=0;i<12;++i) f.put(char(i));}
        {std::ofstream f(root/"project.qpproj");f<<R"({"version":"0.7.0","images":[{"entryID":1,"imageName":"native","serverBuilder":{"builderType":"uri","providerClassName":"qupath.lib.images.servers.imageio.ImageIoImageServerBuilder","uri":"file:source.ppm","args":[],"metadata":{"width":2,"height":2}}}]})";}
        const auto preview=preview_qupath_project((root/"project.qpproj").u8string());
        check(preview->images().size()==1 && preview->images()[0].available && preview->issues().empty() && !preview->consumed());
        const auto project=preview->import_new((root/"native.qpxproj").u8string());
        check(preview->consumed() && project->images().size()==1);
        check(Project::open((root/"native.qpxproj").u8string())->document(1)->metadata().width==2);
        rejects([&]{preview->import_new((root/"twice.qpxproj").u8string());});check(!std::filesystem::exists(root/"twice.qpxproj"));
        auto stale=preview_qupath_project((root/"project.qpproj").u8string());
        {std::ofstream f(root/"source.ppm",std::ios::binary|std::ios::app);f.put('x');}
        rejects([&]{stale->import_new((root/"stale.qpxproj").u8string());});check(!std::filesystem::exists(root/"stale.qpxproj") && !stale->consumed());
        class_project_checks(root);
        std::filesystem::remove_all(root);
        std::cout<<"QuPath project native preview, source guards, new publication and replay refusal passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}

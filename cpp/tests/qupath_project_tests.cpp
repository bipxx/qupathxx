#include "qupathxx/qupath_project.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iostream>
using namespace qpx;
void check(bool value) {if(!value)throw std::runtime_error("QuPath project check failed");}
template<class F>void rejects(F f){try{f();}catch(const std::exception&){return;}throw std::runtime_error("missing import refusal");}
int main() {
    try {
        if(!has_project() || !has_interchange()) {rejects([]{preview_qupath_project("missing.qpproj");});std::cout<<"QuPath project capability disabled and refused\n";return 0;}
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
        std::filesystem::remove_all(root);
        std::cout<<"QuPath project native preview, source guards, new publication and replay refusal passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}

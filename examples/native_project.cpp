#include "qupathxx/analysis.hpp"
#include "qupathxx/project.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>

namespace {
int run(const std::vector<std::string>& args) {
    if(args.size()==1 && (args[0]=="--help" || args[0]=="-h")) {
        std::cout << "Usage: qupathxx_project_example INPUT_PROJECT NEW_PROJECT\n"
                     "       qupathxx_project_example --demo NEW_PROJECT\n";
        return 0;
    }
    if(args.size()!=2) throw std::invalid_argument("Use --help for usage");
    if(args[0]=="--demo") {
        qpx::Project demo;
        demo.add_document(std::make_shared<qpx::Document>(
            qpx::Image(2,1,{255,255,255,10,20,30})),"Synthetic RGB");
        demo.save_new(args[1]);
        std::cout << "Created synthetic project: " << args[1] << '\n';
        return 0;
    }
    auto project=qpx::Project::open(args[0]);
    const auto images=project->images();
    if(images.empty()) throw std::invalid_argument("Project has no images");
    auto document=project->document(images.front().id);
    const auto& metadata=document->metadata();
    const auto object=document->add_rectangle({0,0,
        static_cast<double>(std::min<std::int64_t>(metadata.width,512)),
        static_cast<double>(std::min<std::int64_t>(metadata.height,512))},"Native example ROI");
    // Optical-density means do not use stain separation; the required transform
    // is a fixed illustrative basis, not an estimated biological stain model.
    const qpx::StainTransform transform({{1,0,0},{0,1,0}});
    const qpx::AnalysisRequest request(*document,object,transform,qpx::AnalysisKind::OpticalDensityMeans);
    const auto result=qpx::analyze(request);
    const auto record=document->keep_analysis(request,result);
    project->save_new(args[1]);

    auto reopened=qpx::Project::open(args[1]);
    auto restored=reopened->document(images.front().id);
    const auto replay=qpx::AnalysisRequest::replay(*restored,record);
    if(!restored->verify_analysis(record,replay,qpx::analyze(replay)))
        throw std::runtime_error("Saved analysis did not replay");
    std::cout << "analysis_id=" << record << " samples=" << result.samples
              << " output=" << args[1] << '\n';
    return 0;
}
int checked(const std::vector<std::string>& args) {
    try {return run(args);}
    catch(const std::exception& error) {std::cerr << error.what() << '\n';return 1;}
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
    std::vector<std::string> args;
    for(int i=1;i<argc;++i) args.push_back(std::filesystem::path(argv[i]).u8string());
    return checked(args);
}
#else
int main(int argc,char** argv) {return checked({argv+1,argv+argc});}
#endif

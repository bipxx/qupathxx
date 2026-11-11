#include <qupathxx/analysis.hpp>
#include <qupathxx/file_fingerprint.hpp>
#include <qupathxx/input_digest.hpp>
#include <qupathxx/project.hpp>
#include <qupathxx/provenance.hpp>
#include <qupathxx/reader_plugins.hpp>
#include <qupathxx/sdk.hpp>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int run(int argc,char** argv) {
    require(argc>=4,"Usage: consumer create MANIFEST IMAGE NEW_PROJECT | reopen MANIFEST PROJECT");
    const std::string mode=argv[1];std::shared_ptr<qpx::Project> project;
    const auto environment=qpx::build_environment();
    require(environment->source_tree_sha256==qpx::sdk::source_sha256,"Installed SDK header and linked core identities differ");
    qpx::AnalysisResult result;std::uint64_t record=0;
    if(mode=="create") {
        require(argc==5,"create requires a manifest, image and new project");
        qpx::configure_reader_plugins(argv[2]);project=std::make_shared<qpx::Project>();
        const auto id=project->add_image(argv[3],"org.qupathxx.p6");const auto document=project->document(id);
        require(document->metadata().width==2 && document->metadata().height==2,"Handoff input must be the authored 2x2 P6 image");
        const auto roi=document->add_rectangle({0,0,2,2});
        const qpx::StainTransform stains({{1,0,0},{0,1,0}});
        const qpx::AnalysisRequest request(*document,roi,stains,qpx::AnalysisKind::OpticalDensityMeans);
        result=qpx::analyze(request);require(result.input_sha256==qpx::input_sha256(request),"Native retained/replay input digest differs");
        record=document->keep_analysis(request,result);
        project->apply_source_review(project->review_source_verification(id,*qpx::fingerprint_file(argv[3])));
        project->save_new(argv[4]);
    } else {
        require(mode=="reopen" && argc==4,"reopen requires a manifest and project");
        project=qpx::Project::open(argv[3]); // No plugin load or catalog freeze.
        qpx::configure_reader_plugins(argv[2]);const auto document=project->document(1);
        const auto records=document->analysis_records();require(records.size()==1,"Expected one retained RGB result");record=records.front().id;
        const auto replay=qpx::AnalysisRequest::replay(*document,record);result=qpx::analyze(replay);
        require(document->verify_analysis(record,replay,result),"Reopened plugin result did not verify under its exact implementation");
    }
    require(project->images().size()==1 && result.samples==4,"Native saved workflow changed image/sample counts");
    const auto document=project->document(1);require(document->server()->reader_id()=="org.qupathxx.p6","Saved source provider changed");
    require(result.input_sha256.size()==64 && record!=0,"Native report identity is invalid");
    std::cout<<"{\"schema\":\"qpx.plugin-project-handoff/1\",\"mode\":\""<<mode
        <<"\",\"provider\":\"org.qupathxx.p6\",\"images\":1,\"records\":1,\"verified\":true,\"samples\":"
        <<result.samples<<",\"input_sha256\":\""<<result.input_sha256<<"\",\"source_sha256\":\""<<environment->source_tree_sha256<<"\"}\n";
    return 0;
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){try{std::vector<std::string> text;for(int i=0;i<argc;++i)text.push_back(std::filesystem::path(argv[i]).u8string());std::vector<char*> args;for(auto& value:text)args.push_back(value.data());return run(argc,args.data());}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
#else
int main(int argc,char** argv){try{return run(argc,argv);}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
#endif

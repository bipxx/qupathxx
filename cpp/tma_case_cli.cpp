#include "tma_case_cli.hpp"
#include "tma_case_export.hpp"
#include "qupathxx/project.hpp"
#include "batch_checkpoint.hpp"
#include "reader_file_guard.hpp"
#include "atomic_export.hpp"
#include <charconv>
#include <iostream>
#include <set>

namespace qpx::detail {
std::optional<int> tma_case_cli(const std::vector<std::string>& args,const std::atomic<bool>& cancelled) {
    if(args.size()<2 || args[1]!="--export-tma-cases") return {};
    if(args.size()<4) throw std::invalid_argument("usage: --export-tma-cases PROJECT OUTPUT.csv [--image ID] [--measurements INPUT.csv --value-column NAME] [--weight-column NAME] [--allow-unversioned] [--csv-bytes N]");
    std::string input,value,weight;bool unversioned=false;std::uint64_t image_id=0;
    std::size_t limit=64*1024*1024;std::set<std::string> seen;
    auto integer=[](const std::string& text) {
        std::uint64_t result=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),result);
        if(text.empty() || parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || !result) throw std::invalid_argument("expected positive unsigned integer");
        return result;
    };
    for(std::size_t i=4;i<args.size();++i) {
        const auto& flag=args[i];if(!seen.insert(flag).second) throw std::invalid_argument("duplicate TMA case export option: "+flag);
        auto next=[&]()->const std::string& {if(++i==args.size()) throw std::invalid_argument("missing value for "+flag);return args[i];};
        if(flag=="--image") image_id=integer(next());
        else if(flag=="--measurements") {input=next();if(input.empty()) throw std::invalid_argument("empty TMA measurement path");}
        else if(flag=="--value-column") {value=next();if(value.empty()) throw std::invalid_argument("empty TMA value column");}
        else if(flag=="--weight-column") {weight=next();if(weight.empty()) throw std::invalid_argument("empty TMA weight column");}
        else if(flag=="--csv-bytes") {const auto n=integer(next());if(n>256*1024*1024) throw std::invalid_argument("TMA CSV byte limit exceeds 256 MiB");limit=static_cast<std::size_t>(n);}
        else if(flag=="--allow-unversioned") unversioned=true;
        else throw std::invalid_argument("unknown TMA case export option: "+flag);
    }
    if(input.empty()!=value.empty() || (input.empty() && !weight.empty())) throw std::invalid_argument("TMA measurements require --measurements and --value-column together");
    try {
        export_checkpoint(&cancelled);
        const auto source=export_path(args[2]),output=export_path(args[3]);ReaderFileGuard observed(source);
        guard_file_output(source,observed.version()->identity,output);
        const auto project=Project::open(source);const auto images=project->images();
        if(!image_id) {if(images.size()!=1) throw std::invalid_argument("select one TMA image with --image when the project does not contain exactly one image");image_id=images.front().id;}
        for(const auto& image:images) if(!image.embedded && !image.source_path.empty()) guard_file_output(export_path(image.source_path),"",output);
        const auto document=project->document(image_id);const auto snapshot=document->tma_grid();
        if(!snapshot) throw std::invalid_argument("selected image has no TMA grid");
        observed.verify();
        const auto bytes=save_tma_case_summary_file(*snapshot,output,input,value,weight,limit,unversioned,&cancelled,&observed);
        std::cout<<"{\"schema\":\"qpx.tma-case-export/1\",\"state\":\"saved\",\"image_id\":"<<image_id
                 <<",\"document_revision\":"<<snapshot->document_revision()<<",\"bytes\":"<<bytes<<",\"output\":"<<batch_json_quote(output)<<"}\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"{\"schema\":\"qpx.batch-error/1\",\"error\":"<<batch_json_quote(e.what())<<"}\n";
        return cancelled.load()?130:1;
    }
}
}

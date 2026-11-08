#include "native_workflow_cli.hpp"
#include "qupathxx/density_workflow.hpp"
#include "qupathxx/nucls_import.hpp"
#include "qupathxx/raw_classifier_workflow.hpp"
#include <charconv>
#include <cmath>
#include <iostream>
namespace qpx::detail {
namespace {
std::uint64_t integer(const std::string& text) {
    std::uint64_t result=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),result);
    if(text.empty() || parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size())throw std::invalid_argument("Expected unsigned integer: "+text);
    return result;
}
double real(const std::string& text) {
    std::size_t end=0;const auto result=std::stod(text,&end);
    if(end!=text.size() || !std::isfinite(result))throw std::invalid_argument("Expected finite number: "+text);
    return result;
}
}
std::optional<int> native_workflow_cli(const std::vector<std::string>& args,const std::atomic<bool>& cancelled) {
    if(args.size()<2)return {};
    const auto& command=args[1];
    if(command=="--raw-classifier-workflow") {
        if(args.size()<5)throw std::invalid_argument("--raw-classifier-workflow PROJECT NEW_DIRECTORY IMAGE_ID [--channels N]");
        std::uint32_t channels=1;
        if(args.size()!=5) {
            if(args.size()!=7 || args[5]!="--channels")throw std::invalid_argument("Expected --channels N");
            const auto value=integer(args[6]);if(value>65536)throw std::invalid_argument("channel count exceeds65536");channels=static_cast<std::uint32_t>(value);
        }
        std::cout<<run_raw_classifier_workflow(args[2],args[3],integer(args[4]),channels,&cancelled)<<'\n';return 0;
    }
    if(command!="--project-density" && command!="--density-manifest" && command!="--import-nucls")return {};
    if(command=="--project-density") {
        if(args.size()<4)throw std::invalid_argument("--project-density MANIFEST NEW_DIRECTORY [--resume-from DIRECTORY --recompute --stop-on-error --csv-bytes N --project-bytes N --checkpoint-bytes N]");
        ProjectDensityRunOptions options;
        for(std::size_t i=4;i<args.size();++i) {
            const auto& flag=args[i];
            if(flag=="--recompute"){options.recompute=true;continue;}
            if(flag=="--stop-on-error"){options.stop_on_error=true;continue;}
            if(i+1==args.size())throw std::invalid_argument("Missing value for "+flag);
            const auto& value=args[++i];
            if(flag=="--resume-from")options.resume_from=value;
            else if(flag=="--csv-bytes")options.csv_bytes=integer(value);
            else if(flag=="--project-bytes")options.project_bytes=integer(value);
            else if(flag=="--checkpoint-bytes")options.checkpoint_bytes=integer(value);
            else throw std::invalid_argument("Unknown project density option: "+flag);
        }
        const auto manifest=load_project_density_manifest(args[2],&cancelled);
        const auto report=run_project_density(manifest,args[3],options,&cancelled);
        std::cout<<report.json<<'\n';return report.state=="completed"?0:report.state=="cancelled"?130:1;
    }
    if(command=="--density-manifest") {
        if(args.size()<3)throw std::invalid_argument("--density-manifest PROJECT [--output NEW_MANIFEST --image ID --window-class NAME --tissue-class NAME --exclusion-class NAME --cell-class NAME --all-cells --body-centroids --polygonize-curves]");
        ProjectDensityRecipe recipe;recipe.all_cells=false;std::string output;
        for(std::size_t i=3;i<args.size();++i) {
            const auto& flag=args[i];
            if(flag=="--all-cells"){recipe.all_cells=true;continue;}
            if(flag=="--body-centroids"){recipe.options.prefer_nucleus=false;continue;}
            if(flag=="--polygonize-curves"){recipe.options.polygonize_curves=true;continue;}
            if(i+1==args.size())throw std::invalid_argument("Missing value for "+flag);
            const auto& value=args[++i];
            if(flag=="--output")output=value;
            else if(flag=="--image")recipe.images.push_back(integer(value));
            else if(flag=="--window-class")recipe.classes[0].push_back(value);
            else if(flag=="--tissue-class")recipe.classes[1].push_back(value);
            else if(flag=="--exclusion-class")recipe.classes[2].push_back(value);
            else if(flag=="--cell-class")recipe.classes[3].push_back(value);
            else throw std::invalid_argument("Unknown density manifest option: "+flag);
        }
        recipe.all_images=recipe.images.empty();
        const auto report=generate_project_density_manifest(args[2],recipe,output,&cancelled);
        std::cout<<report.json<<'\n';return report.preview.valid()?0:1;
    }
    if(args.size()<4)throw std::invalid_argument("--import-nucls IMAGES ANNOTATIONS --mpp-x N --mpp-y N --representation outlines|centroids [--name STEM --class-column NAME --output NEW_PROJECT]");
    NuclsImportOptions options;options.representation.clear();std::string output;
    for(std::size_t i=4;i<args.size();++i) {
        const auto& flag=args[i];
        if(i+1==args.size())throw std::invalid_argument("Missing value for "+flag);
        const auto& value=args[++i];
        if(flag=="--mpp-x")options.mpp_x=real(value);
        else if(flag=="--mpp-y")options.mpp_y=real(value);
        else if(flag=="--representation")options.representation=value;
        else if(flag=="--class-column")options.class_column=value;
        else if(flag=="--output")output=value;
        else if(flag=="--name"){if(!options.names)options.names.emplace();options.names->push_back(value);}
        else throw std::invalid_argument("Unknown NuCLS option: "+flag);
    }
    const auto report=import_nucls(args[2],args[3],options,output,&cancelled);
    std::cout<<report.json<<'\n';return report.cancelled?130:report.valid?0:1;
}
}

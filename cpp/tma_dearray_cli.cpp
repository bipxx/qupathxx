#include "tma_dearray_cli.hpp"
#include "qupathxx/tma_dearray.hpp"
#include "qupathxx/project.hpp"
#include "atomic_export.hpp"
#include "reader_file_guard.hpp"
#include "batch_checkpoint.hpp"
#include <charconv>
#include <cmath>
#include <iostream>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
namespace qpx::detail {
namespace {
std::uint64_t positive(const std::string& text) {
    std::uint64_t value=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(!value || parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size())throw std::invalid_argument("TMA option requires a positive unsigned integer");
    return value;
}
double real(const std::string& text) {
    std::istringstream stream(text);stream.imbue(std::locale::classic());double value=0;stream>>std::noskipws>>value;
    if(!stream || stream.peek()!=EOF || !std::isfinite(value))throw std::invalid_argument("TMA option requires a finite decimal number");
    return value;
}
}
std::optional<int> tma_dearray_cli(const std::vector<std::string>& args,const std::atomic<bool>& cancelled) {
    if(args.size()<2 || args[1]!="--detect-tma")return {};
    const char* usage="usage: --detect-tma PROJECT OUTPUT.qptma --image ID --rows N --columns N --core-width PX --core-height PX [--polarity dark|light --missing-density N --preview-width N --preview-height N --source-pixels N --output-pixels N --work-bytes N --qptma-bytes N]";
    if(args.size()<4)throw std::invalid_argument(usage);
    TmaDearrayOptions options;std::uint64_t image_id=0;std::size_t text_limit=8*1024*1024;std::set<std::string> seen;
    for(std::size_t i=4;i<args.size();++i) {
        const auto flag=args[i];if(!seen.insert(flag).second || ++i==args.size())throw std::invalid_argument(usage);const auto& value=args[i];
        if(flag=="--name")options.name=value;
        else if(flag=="--polarity") {if(value!="dark" && value!="light")throw std::invalid_argument("TMA polarity must be dark or light");options.polarity=value=="dark"?TmaPolarity::DarkOnLight:TmaPolarity::LightOnDark;}
        else if(flag=="--core-width")options.core_width=real(value);
        else if(flag=="--core-height")options.core_height=real(value);
        else if(flag=="--missing-density")options.missing_density=real(value);
        else if(flag=="--min-valid-fraction")options.min_valid_fraction=real(value);
        else if(flag=="--bounds-scale")options.bounds_scale=real(value);
        else if(flag=="--min-component-area")options.min_component_area=real(value);
        else if(flag=="--max-component-area")options.max_component_area=real(value);
        else if(flag=="--min-component-extent")options.min_component_extent=real(value);
        else if(flag=="--max-fit-error")options.max_fit_error=real(value);
        else if(flag=="--max-rotation")options.max_rotation_degrees=real(value);
        else if(flag=="--image" || flag=="--rows" || flag=="--columns" || flag=="--preview-width" || flag=="--preview-height" ||
                flag=="--source-pixels" || flag=="--output-pixels" || flag=="--work-bytes" || flag=="--max-candidates" || flag=="--pair-checks" || flag=="--qptma-bytes") {
            const auto n=positive(value);if(n>std::numeric_limits<std::size_t>::max())throw std::length_error("TMA option exceeds host size limit");
            if(flag=="--image")image_id=n;
            else if(flag=="--rows")options.rows=static_cast<std::size_t>(n);
            else if(flag=="--columns")options.columns=static_cast<std::size_t>(n);
            else if(flag=="--preview-width" || flag=="--preview-height") {if(n>4096)throw std::invalid_argument("TMA preview bound exceeds 4096");if(flag=="--preview-width")options.thumbnail.max_width=static_cast<std::int32_t>(n);else options.thumbnail.max_height=static_cast<std::int32_t>(n);}
            else if(flag=="--source-pixels")options.thumbnail.source_pixel_limit=n;
            else if(flag=="--output-pixels")options.thumbnail.output_pixel_limit=n;
            else if(flag=="--work-bytes")options.work_bytes=static_cast<std::size_t>(n);
            else if(flag=="--max-candidates")options.max_candidates=static_cast<std::size_t>(n);
            else if(flag=="--pair-checks")options.pair_checks=n;
            else {if(n>64*1024*1024)throw std::length_error("TMA QPTMA output exceeds 64 MiB limit");text_limit=static_cast<std::size_t>(n);}
        }else throw std::invalid_argument(usage);
    }
    for(const auto* required:{"--image","--rows","--columns","--core-width","--core-height"})if(!seen.count(required))throw std::invalid_argument(usage);
    options.validate();
    try {
        export_checkpoint(&cancelled);const auto source=export_path(args[2]),output=export_path(args[3]);
        ReaderFileGuard project_file(source);guard_file_output(source,project_file.version()->identity,output);
        const auto project=Project::open_without_journal_recovery(source);const auto images=project->images();const ProjectImage* selected=nullptr;
        for(const auto& image:images)if(image.id==image_id)selected=&image;
        if(!selected)throw std::invalid_argument("selected TMA image does not exist");
        std::unique_ptr<ReaderFileGuard> selected_file;
        if(!selected->embedded && !selected->source_path.empty())selected_file=std::make_unique<ReaderFileGuard>(export_path(selected->source_path));
        auto protect=[&] {project_file.verify();guard_file_output(source,project_file.version()->identity,output);
            if(selected_file)selected_file->verify();
            for(const auto& image:images)if(!image.embedded && !image.source_path.empty())guard_file_output(export_path(image.source_path),"",output);};
        protect();const auto document=project->document(image_id);const auto result=detect_tma_grid(*document->server(),options,&cancelled);
        const auto text=qptma_text(result->grid,text_limit,&cancelled);AtomicExportFile file(output,text_limit,&cancelled);file.write(text.data(),text.size());protect();file.commit(false);
        std::cout<<"{\"schema\":\"qpx.tma-dearray-preview/1\",\"state\":\"saved\",\"image_id\":"<<image_id<<",\"rows\":"<<result->grid.rows<<",\"columns\":"<<result->grid.columns
            <<",\"candidates\":"<<result->candidates<<",\"input_sha256\":"<<batch_json_quote(result->input_sha256)<<",\"recipe_sha256\":"<<batch_json_quote(result->recipe_sha256)
            <<",\"output\":"<<batch_json_quote(output)<<"}\n";return 0;
    }catch(const std::exception& error) {std::cerr<<"{\"schema\":\"qpx.batch-error/1\",\"error\":"<<batch_json_quote(error.what())<<"}\n";return cancelled.load()?130:1;}
}
}

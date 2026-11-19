// SPDX-License-Identifier: AGPL-3.0-only
#include "qupathxx/project_batch.hpp"
#include "strict_json.hpp"
#include "utf8_text.hpp"
#include "workflow_input.hpp"
#include <limits>
#include <set>

namespace qpx {
namespace {
using Json=nlohmann::json;
using Path=std::filesystem::path;

void fields(const Json& value,std::initializer_list<const char*> expected) {
    if(!value.is_object() || value.size()!=expected.size())
        throw std::invalid_argument("Unexpected pixel batch recipe fields");
    for(const auto* field:expected)
        if(!value.contains(field))
            throw std::invalid_argument(std::string("Missing pixel batch recipe field: ")+field);
}

std::uint64_t positive_integer(const Json& value) {
    if(!value.is_number_unsigned() || !value.get<std::uint64_t>())
        throw std::invalid_argument("Pixel batch recipe requires a positive unsigned integer");
    return value.get<std::uint64_t>();
}

std::size_t allocation_size(const Json& value) {
    const auto size=positive_integer(value);
    if(size>std::numeric_limits<std::size_t>::max())
        throw std::length_error("Pixel batch recipe size exceeds address space");
    return static_cast<std::size_t>(size);
}

std::int64_t axis_integer(const Json& value) {
    if(!value.is_number_integer() || (value.is_number_unsigned() &&
       value.get<std::uint64_t>() > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())))
        throw std::invalid_argument("Pixel batch recipe axis requires a signed-range integer");
    return value.get<std::int64_t>();
}

ScientificGridAxis raw_axis(const Json& axis) {
    if(!axis.is_array() || axis.size()!=4)
        throw std::invalid_argument("Raw pixel batch axis requires origin, extent, samples and context");
    return {axis_integer(axis[0]),axis_integer(axis[1]),axis_integer(axis[2]),axis_integer(axis[3])};
}

double vote_fraction(const Json& value) {
    if(!value.is_number())
        throw std::invalid_argument("Pixel batch recipe vote fraction must be numeric");
    return value.get<double>();
}

void configure_raw_recipe(const Json& raw, ProjectBatchOptions& options) {
    fields(raw,{"kind","grid","stream"});
    const auto& kind=raw.at("kind");
    if(kind=="raw-pixels") options.kind=ProjectBatchKind::RawPixels;
    else if(kind=="raw-contours") options.kind=ProjectBatchKind::RawPixelContours;
    else throw std::invalid_argument("Raw pixel batch recipe kind must be raw-pixels or raw-contours");

    const auto& grid=raw.at("grid");
    const auto& mode=grid.at("mode");
    if(mode=="explicit") {
        fields(grid,{"mode","x","y"});
        options.raw_pixel_x=raw_axis(grid.at("x"));
        options.raw_pixel_y=raw_axis(grid.at("y"));
    } else {
        fields(grid,{"mode"});
        if(mode=="model") options.raw_pixel_model_grid=true;
        else if(mode!="unit") throw std::invalid_argument("Raw pixel batch grid mode must be model, unit or explicit");
    }

    const auto& stream=raw.at("stream");
    fields(stream,{"tile_width","tile_height","work_bytes","min_vote_fraction","max_samples","max_windows"});
    auto& settings=options.raw_pixels;
    settings.tile_width=axis_integer(stream.at("tile_width"));
    settings.tile_height=axis_integer(stream.at("tile_height"));
    settings.work_bytes=allocation_size(stream.at("work_bytes"));
    settings.min_vote_fraction=vote_fraction(stream.at("min_vote_fraction"));
    settings.max_samples=positive_integer(stream.at("max_samples"));
    settings.max_windows=positive_integer(stream.at("max_windows"));
    settings.validate();
}

void configure_rgb_recipe(const Json& pixels, ProjectBatchOptions& options) {
    fields(pixels,{"block_size","tile_size","work_bytes","min_vote_fraction"});
    options.kind=ProjectBatchKind::Pixels;
    options.pixels.block_size=allocation_size(pixels.at("block_size"));
    options.pixels.prediction.tile_size=allocation_size(pixels.at("tile_size"));
    options.pixels.prediction.work_bytes=allocation_size(pixels.at("work_bytes"));
    options.pixels.prediction.min_vote_fraction=vote_fraction(pixels.at("min_vote_fraction"));
    options.pixels.validate();
}

detail::WorkflowInput pinned_input(const Json& value,const Path& parent,std::uint64_t limit,
        bool project,const std::atomic<bool>* cancelled) {
    fields(value,{"path","sha256"});
    const auto path=value.at("path").get<std::string>();
    detail::bounded_utf8(path,32768);
    const auto expected=value.at("sha256").get<std::string>();
    if(path.empty() || path.find('\0')!=std::string::npos || expected.size()!=64 ||
       expected.find_first_not_of("0123456789abcdef")!=std::string::npos)
        throw std::invalid_argument("Pixel batch input needs a path and exact lowercase SHA-256");
    detail::WorkflowInput input((parent/std::filesystem::u8path(path)).u8string(),limit,project,cancelled);
    if(input.sha256()!=expected)
        throw WorkflowInputChanged(std::string(project?"Project":"Pixel model")+" file SHA-256 differs from recipe");
    return input;
}
}

ProjectBatchReport run_project_pixel_batch_recipe(const std::string& recipe_path,
        const std::string& output_directory,const std::atomic<bool>* cancelled,
        const std::function<void(const ProjectBatchImageResult&)>& on_image) {
    const detail::WorkflowInput recipe(recipe_path,4u*1024*1024,false,cancelled);
    const auto contents=detail::read_workflow_input(recipe,cancelled);
    detail::StrictJson strict("project pixel batch recipe");
    if(!Json::sax_parse(contents,&strict))throw std::invalid_argument("Invalid project pixel batch recipe");
    const auto spec=Json::parse(contents);
    const auto schema=spec.at("schema");
    const bool raw=schema=="qpx.project-raw-pixel-batch-recipe/1";
    if(raw) fields(spec,{"schema","project","model","selection","raw","failure","resume"});
    else {
        fields(spec,{"schema","project","model","selection","pixels","failure","resume"});
        if(schema!="qpx.project-pixel-batch-recipe/1")
            throw std::invalid_argument("Unsupported project pixel batch recipe version");
    }

    const auto& selected=spec.at("selection");
    if(!selected.is_array() || selected.empty() || selected.size()>10000)
        throw std::invalid_argument("Pixel batch recipe requires 1 to 10000 selected images");
    std::vector<ProjectBatchSelection> selection;
    std::set<std::uint64_t> images;
    std::size_t total_objects=0;
    for(const auto& row:selected) {
        workflow_checkpoint(cancelled);
        fields(row,{"image","objects"});
        const auto image=positive_integer(row.at("image"));
        if(!images.insert(image).second)throw std::invalid_argument("Duplicate pixel batch image selection");
        const auto& objects=row.at("objects");
        if(!objects.is_array() || objects.empty() || objects.size()>100000-total_objects)
            throw std::invalid_argument("Pixel batch recipe requires explicit ROI IDs within the batch limit");
        total_objects+=objects.size();
        ProjectBatchSelection item{image,{}};
        std::set<std::uint64_t> ids;
        for(const auto& object:objects) {
            const auto id=positive_integer(object);
            if(!ids.insert(id).second)throw std::invalid_argument("Duplicate pixel batch ROI selection");
            item.object_ids.push_back(id);
        }
        selection.push_back(std::move(item));
    }

    ProjectBatchOptions options;
    const auto failure=spec.at("failure").get<std::string>();
    if(failure=="continue")options.failure=ProjectBatchFailure::Continue;
    else if(failure!="stop")throw std::invalid_argument("Pixel batch recipe failure policy must be stop or continue");
    options.resume=spec.at("resume").get<bool>();
    if(raw) configure_raw_recipe(spec.at("raw"),options);
    else configure_rgb_recipe(spec.at("pixels"),options);

    const auto parent=std::filesystem::u8path(recipe.path()).parent_path();
    const auto source=pinned_input(spec.at("project"),parent,1024ull*1024*1024,true,cancelled);
    const auto model=pinned_input(spec.at("model"),parent,64ull*1024*1024+(raw?128*1024:4096),false,cancelled);
    const auto transform=[&] {
        if(!raw) {
            options.pixel_model=load_pixel_classifier(model.path(),cancelled);
            return options.pixel_model->transform();
        }
        options.raw_pixel_model=load_raw_pixel_classifier(model.path(),cancelled);
        if(options.raw_pixel_model->sha256()!=model.sha256())
            throw WorkflowInputChanged("Loaded raw pixel model SHA-256 differs from recipe");
        // The raw kernel uses its model's own channel and feature recipe. The
        // shared batch envelope still records the ordinary CLI stain default.
        return StainTransform({{.65,.70,.29},{.07,.99,.11}});
    }();
    auto project=Project::open_without_journal_recovery(source.path());
    ProjectBatchRequest request(*project,transform,selection,std::move(options));
    project.reset();
    source.verify(true,cancelled);
    model.verify(true,cancelled);
    recipe.verify(true,cancelled);
    return run_project_batch(request,output_directory,cancelled,on_image);
}
}

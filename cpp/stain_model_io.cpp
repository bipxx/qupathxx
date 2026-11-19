#include "qupathxx/stain_model_io.hpp"
#include "qupathxx/stain_estimation.hpp"
#include "qupathxx/project.hpp"
#include "atomic_export.hpp"
#include "project_database_output.hpp"
#include "project_export_guard.hpp"
#include "stain_model_io_internal.hpp"
#include "workflow_input.hpp"
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#ifdef QUPATHXX_HAS_INTERCHANGE
#include "strict_json.hpp"
#endif
namespace qpx {
namespace {
constexpr std::size_t model_byte_limit=16*1024*1024;
#ifdef QUPATHXX_HAS_INTERCHANGE
ColorVector model_vector(const nlohmann::json& value) {
    if(!value.is_array() || value.size()!=3) throw std::invalid_argument("stain model vector requires three numbers");
    ColorVector result;
    for(std::size_t c=0;c<3;++c) {
        if(!value[c].is_number()) throw std::invalid_argument("stain model components must be numbers");
        result[c]=value[c].get<double>();
        if(!std::isfinite(result[c])) throw std::invalid_argument("stain model components must be finite");
    }
    return result;
}
#endif
}
std::string stain_model_json(const StainTransform& model) {
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(std::numeric_limits<double>::max_digits10);
    const auto vector=[&](const ColorVector& v) {out<<'[';for(std::size_t i=0;i<3;++i) {if(i)out<<',';out<<v[i];}out<<']';};
    out<<"{\"schema\":\"qpx.stain-model/1\",\"white\":";vector(model.white());out<<",\"stains\":[";
    for(std::size_t i=0;i<3;++i) {if(i)out<<',';vector(model.stains()[i]);}
    out<<"],\"model_sha256\":\""<<stain_model_sha256(model)<<"\"}\n";
    return out.str();
}
StainTransform stain_model_from_json(const std::string& text) {
#ifndef QUPATHXX_HAS_INTERCHANGE
    throw std::runtime_error("stain model import requires QUPATHXX_BUILD_INTERCHANGE=ON");
#else
    if(text.size()>model_byte_limit) throw std::length_error("stain model JSON exceeds 16 MiB");
    try {
        detail::StrictJson strict("stain model JSON");nlohmann::json::sax_parse(text,&strict);
        const auto root=nlohmann::json::parse(text);
        if(!root.is_object() || !root.at("schema").is_string()) throw std::invalid_argument("invalid stain model schema");
        const auto schema=root.at("schema").get<std::string>();
        if(schema=="qpx.stain-model/1") {
            if(root.size()!=4) throw std::invalid_argument("unexpected stain model member");
        } else if(schema=="qpx.stain-estimate/1") {
            if(root.at("algorithm")!="qpx.od-plane-stains/1") throw std::invalid_argument("unsupported stain estimate algorithm");
        } else throw std::invalid_argument("unsupported stain model schema");
        const auto white=model_vector(root.at("white"));const auto& values=root.at("stains");
        if(!values.is_array() || values.size()!=3) throw std::invalid_argument("stain model requires three normalized vectors");
        std::array<ColorVector,3> basis;
        for(std::size_t i=0;i<3;++i) basis[i]=model_vector(values[i]);
        const auto model=StainTransform::from_normalized(basis,white);
        if(!root.at("model_sha256").is_string() || root.at("model_sha256").get<std::string>()!=stain_model_sha256(model))
            throw std::invalid_argument("stain model digest mismatch");
        return model;
    } catch(const nlohmann::json::exception& e) {
        throw std::invalid_argument(std::string("invalid stain model JSON: ")+e.what());
    }
#endif
}
void detail::save_stain_model_guarded(const StainTransform& model,const std::string& path,
    const std::function<void()>& verify_output,bool replace,const std::atomic<bool>* cancelled) {
    const auto text=stain_model_json(model);verify_output();detail::AtomicExportFile file(path,model_byte_limit,cancelled);
    file.write(text.data(),text.size());verify_output();file.commit(replace);
}
void save_stain_model(const StainTransform& model,const std::string& path) {
    detail::save_stain_model_guarded(model,path,[]{});
}
StainTransform load_stain_model(const std::string& path) {
#ifndef QUPATHXX_HAS_INTERCHANGE
    throw std::runtime_error("stain model import requires QUPATHXX_BUILD_INTERCHANGE=ON");
#else
    const auto input=std::filesystem::u8path(detail::export_path(path));
    std::error_code error;
    if(!std::filesystem::is_regular_file(input,error) || error) throw std::invalid_argument("stain model input must be a regular file");
    const auto size=std::filesystem::file_size(input,error);
    if(error) throw std::runtime_error("cannot inspect stain model file");
    if(size>model_byte_limit) throw std::length_error("stain model file exceeds 16 MiB");
    std::ifstream stream(input,std::ios::binary);if(!stream) throw std::runtime_error("cannot open stain model file");
    std::string text;std::array<char,4096> buffer;
    while(stream) {
        stream.read(buffer.data(),buffer.size());const auto n=std::size_t(stream.gcount());
        if(n>model_byte_limit-text.size()) throw std::length_error("stain model file exceeds 16 MiB");
        text.append(buffer.data(),n);
    }
    if(stream.bad()) throw std::runtime_error("cannot read stain model file");
    return stain_model_from_json(text);
#endif
}

std::string export_project_stain_model(const std::string& project_path,std::uint64_t image,
    const std::string& new_model,const std::atomic<bool>* cancelled) {
    using namespace detail;
    const WorkflowInput input(project_path,1024ull*1024*1024,true,cancelled);
    const auto destination=export_path(new_model);
    const auto project=Project::open_without_journal_recovery(input.path());
    const auto& settings=project->document(image)->image_analysis_settings();
    if(!settings || !settings->rgb8_supported())
        throw std::invalid_argument("Selected image "+std::to_string(image)+" requires saved RGB8 stains");
    const auto model=settings->transform();
    const auto images=project->images();
    save_stain_model_guarded(model,destination,[&] {
        input.verify(false,cancelled);
        guard_project_database_output(input.path(),"",destination);
        guard_project_export_output(*project,images,destination,cancelled);
    },false,cancelled);
    return stain_model_sha256(model);
}

std::string apply_project_stain_model(const std::string& project_path,std::uint64_t image,
    const std::string& model_path,const std::string& new_project,const std::string& profile_name,
    const std::atomic<bool>* cancelled) {
    using namespace detail;
    const WorkflowInput input(project_path,1024ull*1024*1024,true,cancelled);
    const WorkflowInput model(model_path,model_byte_limit,false,cancelled);
    const auto destination=export_path(new_project);
    const auto transform=stain_model_from_json(read_workflow_input(model,cancelled));
    const auto project=Project::open_without_journal_recovery(input.path());
    const auto document=project->document(image);
    const auto images=project->images();
    const auto verify=[&] {
        input.verify(false,cancelled);
        model.verify(true,cancelled);
        guard_project_database_output(input.path(),"",destination);
        for(const auto* suffix:{"","-journal","-wal","-shm"})
            guard_file_output(model.path(),"",destination+suffix);
        guard_project_export_output(*project,images,destination,cancelled);
    };
    verify();
    const auto& current=document->image_analysis_settings();
    auto type=current?current->image_type():"UNSET";
    if(type=="UNSET") type="BRIGHTFIELD_OTHER";
    auto settings=ImageAnalysisSettings::from_normalized(std::move(type),transform.stains(),transform.white(),
        profile_name,{"Stain 1","Stain 2","Stain 3"},false);
    const auto digest=stain_model_sha256(transform);
    if(settings.model_sha256()!=digest) throw std::logic_error("Saved image settings changed the stain model identity");
    document->set_image_analysis_settings(std::move(settings));
    project->save_new(destination,512ull*1024*1024,1024ull*1024*1024,cancelled,verify);
    return digest;
}
}

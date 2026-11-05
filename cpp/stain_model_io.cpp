#include "qupathxx/stain_model_io.hpp"
#include "qupathxx/stain_estimation.hpp"
#include "atomic_export.hpp"
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
void save_stain_model(const StainTransform& model,const std::string& path) {
    const auto text=stain_model_json(model);detail::AtomicExportFile file(path,model_byte_limit);
    file.write(text.data(),text.size());file.commit();
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
}

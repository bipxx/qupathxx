#pragma once
#include "qupathxx/expression_reduction.hpp"
#include "qupathxx/workflow.hpp"
#include "qupathxx/image_data_properties.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>

namespace qpx {
struct ExpressionReductionResult::Data {
    ExpressionReductionOptions options;
    std::string source,file,value_source,domain,backend,id;
    std::uint64_t source_features=0;
    std::vector<std::uint64_t> features;
    std::vector<std::string> feature_ids,observations;
    std::vector<double> means,embedding,loadings,singular,energy,centres;
    std::vector<std::uint32_t> labels;
    double total_energy=0,inertia=0;
    std::uint32_t iterations=0;
    bool converged=false;
    std::size_t peak=0,batches=0;
    std::uint64_t operations=0,distances=0;
};
struct ExpressionReductionCodec {
    static std::vector<std::uint8_t> encode(const ExpressionReductionResult&,std::size_t,const std::atomic<bool>*);
    static std::shared_ptr<const ExpressionReductionResult> decode(const std::vector<std::uint8_t>&,std::size_t,const std::atomic<bool>*);
    static void identify(ExpressionReductionResult&,std::size_t,const std::atomic<bool>*);
};
}
namespace qpx::detail::reduction {
using J=nlohmann::json;
constexpr std::size_t archive_limit=256u*1024*1024,header_limit=16u*1024*1024;
inline void work_limit(std::size_t n){if(n<32u*1024*1024 || n>1024ull*1024*1024)throw std::invalid_argument("Expression reduction work must be32..1024MiB");}
inline void digest(const std::string& s){if(s.size()!=64 || s.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::invalid_argument("Invalid expression reduction digest");}
inline void text(const std::string& s,std::size_t n=256,bool required=true){bounded_utf8(s,n);if((required && s.empty()) || s.find_first_of("\t\r\n")!=std::string::npos)throw std::invalid_argument("Invalid expression reduction text");}
inline void charge(std::size_t& n,std::size_t count,std::size_t width,std::size_t work){if(count>work/width)throw WorkflowCaptureLimit("Expression reduction allocation exceeds work budget");workflow_capture_charge(n,count*width,work);}
inline std::uint64_t bits(double v){std::uint64_t b;std::memcpy(&b,&v,8);return b;}
inline double real(std::uint64_t b){double v;std::memcpy(&v,&b,8);if(!std::isfinite(v))throw std::invalid_argument("Nonfinite expression reduction value");return v;}
inline std::uint64_t integer(const J& j,std::uint64_t maximum){if(!j.is_number_integer() || j<0 || j>maximum)throw std::invalid_argument("Invalid reduction integer");return j.get<std::uint64_t>();}
J parameters(const ExpressionReductionOptions&);
ExpressionReductionOptions parameters(const J&);
void require_parent(const ExpressionReductionResult&,const ExpressionMatrixSource&,std::size_t,const std::atomic<bool>*);
// Scoped hooks mirror the existing matrix-association boundaries; no live
// Document or undo state is mutated during serialization.
std::shared_ptr<const ImageDataProperties> properties_for_save(std::shared_ptr<const ImageDataProperties>,
    const std::filesystem::path&,const std::filesystem::path&,const std::atomic<bool>*);
std::shared_ptr<const ImageDataProperties> properties_after_open(std::shared_ptr<const ImageDataProperties>,
    const std::filesystem::path&,bool unsaved_recovery=false);
void guard_project_output(std::shared_ptr<const ImageDataProperties>,const std::filesystem::path&,const std::string&);
}

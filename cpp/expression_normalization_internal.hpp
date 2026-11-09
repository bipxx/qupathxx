#pragma once
#include "qupathxx/expression_normalization.hpp"
#include "qupathxx/workflow.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
namespace qpx::detail::normalization {
inline void work_limit(std::size_t n){if(n<8u*1024*1024 || n>1024ull*1024*1024)throw std::invalid_argument("Expression QC work must be8..1024MiB");}
inline void text(Sha256& h,const std::string& s){h.integer(s.size());h.update(s.data(),s.size());}
inline void digest(const std::string& s){if(s.size()!=64 || s.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::invalid_argument("Invalid expression QC digest");}
inline std::uint64_t bits(double v){std::uint64_t n;std::memcpy(&n,&v,8);return n;}
inline void charge(std::size_t& n,std::size_t count,std::size_t width,std::size_t work){if(count>work/width)throw WorkflowCaptureLimit("Expression QC allocation exceeds work budget");workflow_capture_charge(n,count*width,work);}
inline void raw(const ExpressionMatrixSource& source){const auto& m=source.metadata();if(m.domain!=ExpressionValueDomain::Counts || m.value_domain!="raw-counts")throw std::invalid_argument("QC/normalization requires explicit raw counts, not continuous or normalized values");if(!m.features || m.features>1000000 || !m.observations || m.observations>1000000)throw std::invalid_argument("QC source dimensions exceed contract");digest(m.source_sha256);digest(source.file_sha256());}
inline std::size_t group_bytes(const std::vector<ExpressionFeatureGroup>& groups){std::size_t n=groups.capacity()*sizeof(ExpressionFeatureGroup);for(const auto& g:groups)n+=g.name.capacity()+g.features.capacity()*sizeof(std::uint64_t);return n;}
std::string recipe(const ExpressionQcResult&,ExpressionNormalizationOptions);
struct Recipe {
    std::string source,file,qc;
    std::uint64_t features=0,observations=0;
    ExpressionQcOptions qc_options;
    ExpressionNormalizationOptions options;
};
Recipe parse_recipe(const std::string&);
}

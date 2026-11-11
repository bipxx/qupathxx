#pragma once
#include "qupathxx/expression_aggregation.hpp"
#include "qupathxx/workflow.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace qpx::detail::aggregation {
constexpr std::size_t recipe_limit=768u*1024;
inline void work_limit(std::size_t n) {
    if(n<16u*1024*1024 || n>1024ull*1024*1024)throw std::invalid_argument("Expression aggregation work must be16..1024MiB");
}
inline void text(Sha256& h,const std::string& s){h.integer(s.size());h.update(s.data(),s.size());}
inline void digest(const std::string& s){if(s.size()!=64 || s.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::invalid_argument("Invalid expression aggregation digest");}
inline std::uint64_t bits(double v){std::uint64_t b;std::memcpy(&b,&v,8);return b;}
inline void label(const std::string& s,bool required=false){bounded_utf8(s,256);if((required && s.empty()) || s.find_first_of("\t\r\n")!=std::string::npos)throw std::invalid_argument("Invalid expression cohort label");}
inline std::size_t cohort_bytes(const std::vector<ExpressionCohort>& c){std::size_t n=c.capacity()*sizeof(ExpressionCohort);for(const auto& g:c)n+=g.id.capacity()+g.sample.capacity()+g.replicate.capacity()+g.group.capacity()+g.source_sha256.capacity()+g.origin.capacity()+g.observations.capacity()*8;return n;}
inline void charge(std::size_t& n,std::size_t count,std::size_t width,std::size_t work){if(count>work/width)throw WorkflowCaptureLimit("Expression aggregation allocation exceeds budget");workflow_capture_charge(n,count*width,work);}
void validate(const ExpressionMatrixMetadata&,const std::string& file,const std::vector<ExpressionCohort>&,const ExpressionAggregationOptions&);
struct Recipe {
    std::string source,file,result;
    std::uint64_t features=0,observations=0;
    std::vector<ExpressionCohort> cohorts;
    ExpressionAggregationOptions options;
};
std::string encode(const Recipe&);
Recipe parse(const std::string&);
}

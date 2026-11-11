#pragma once
#include "qupathxx/expression_differential.hpp"
#include "expression_aggregation_internal.hpp"
namespace qpx::detail::differential {
inline void work_limit(std::size_t n){if(n<32u*1024*1024 || n>1024ull*1024*1024)throw std::invalid_argument("Expression statistics work must be32..1024MiB");}
inline void options(const ExpressionDifferentialOptions& o){
    work_limit(o.work_bytes);aggregation::label(o.group_a,true);aggregation::label(o.group_b,true);
    if(o.group_a==o.group_b || !std::isfinite(o.library_target) || o.library_target<1 || o.library_target>1e12)throw std::invalid_argument("DE requires distinct groups and a library target in1..1e12");
}
constexpr const char* method="welch-log1p-pseudobulk/1";
constexpr const char* backend="Boost.Math-1.88.0-standalone/double";
struct Recipe {
    ExpressionDifferentialOptions options;
    std::string aggregation, aggregation_sha256, source, result;
};
std::string encode(const Recipe&);
Recipe parse(const std::string&);
}

#pragma once
#include "qupathxx/h5ad.hpp"
#include "sha256.hpp"
namespace qpx::detail {
// Pure recipe identity, shared by the decoder and sidecar verification. Options
// are validated at their native input boundary; no HDF5 runtime is needed here.
inline std::string h5ad_input_identity(const std::string& file,const H5adOptions& p){
    Sha256 h;const auto put=[&](const std::string& s){h.integer(s.size());h.update(s.data(),s.size());};
    put("qpx.h5ad-input/1");put(file);put(p.matrix);put(p.spatial);put(p.value_domain);put(p.frame.sha256());h.integer(std::uint64_t(*p.domain));return h.finish();
}
}

#pragma once
#include "qupathxx/scientific_raw_sampling.hpp"
namespace qpx::detail {
std::string scientific_raw_input_sha256(const RawImageMetadata&,const ScientificGrid&,const std::string& reader,const std::string& decoded,const std::atomic<bool>* cancelled=nullptr);
}

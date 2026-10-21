#pragma once
#include "qupathxx/tissue.hpp"
namespace qpx {
struct ProvenanceCodec {
    static std::vector<std::uint8_t> encode(const AnalysisProvenance&);
    static std::shared_ptr<const AnalysisProvenance> decode(const std::uint8_t*,std::size_t);
    static void attach(const std::shared_ptr<const TissueResult>&,std::shared_ptr<const AnalysisProvenance>);
};
}

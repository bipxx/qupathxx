#pragma once
#include "qupathxx/cells.hpp"
namespace qpx {
// Read/hash the same bounded pixel stream without running the analysis kernel.
std::string input_sha256(const AnalysisRequest&,const std::atomic<bool>* cancelled=nullptr);
std::string input_sha256(const TissueRequest&,const std::atomic<bool>* cancelled=nullptr);
std::string input_sha256(const CellRequest&,const std::atomic<bool>* cancelled=nullptr);
}

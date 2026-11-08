#pragma once
#include "qupathxx/stains.hpp"
#include <string>
namespace qpx {
// Model-only exchange: no estimator result or source provenance is adopted.
std::string stain_model_json(const StainTransform&);
StainTransform stain_model_from_json(const std::string&);
void save_stain_model(const StainTransform&,const std::string& path);
StainTransform load_stain_model(const std::string& path);
}

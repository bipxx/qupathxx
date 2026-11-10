#pragma once
#include "qupathxx/fluorescence_cell_workflow.hpp"
#include "qupathxx/tissue.hpp"
namespace qpx::detail {
std::shared_ptr<const TissueContours> trace_fluorescence_label_mask(const FluorescenceCellRun&,bool body,TissueContourOptions,const std::atomic<bool>*);
}

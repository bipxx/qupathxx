#pragma once
#include "qupathxx/tma.hpp"
namespace qpx::detail {
class ReaderFileGuard;
// An optional project guard remains owned by the synchronous CLI caller.
std::size_t save_tma_case_summary_file(const TmaGrid&,const std::string& output_path,
    const std::string& input_path,const std::string& value_column,const std::string& weight_column,
    std::size_t byte_limit,bool allow_unversioned,const std::atomic<bool>* cancelled,
    const ReaderFileGuard* snapshot_source=nullptr);
std::size_t save_tma_cohort_case_summary_file(const TmaCohort&,const std::string& output_path,
    const std::string& input_path,const std::string& value_column,const std::string& weight_column,
    std::size_t byte_limit,bool allow_unversioned,const std::atomic<bool>* cancelled,
    const ReaderFileGuard* snapshot_source=nullptr);
}

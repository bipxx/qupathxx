#pragma once
#include "tma_internal.hpp"
namespace qpx::detail {
class ReaderFileGuard;
inline constexpr std::size_t tma_measurement_file_limit=4096;
// An optional project guard remains owned by the synchronous CLI caller.
std::size_t save_tma_case_summary_file(const TmaGrid&,const std::string& output_path,
    const std::vector<std::string>& input_paths,const std::string& value_column,const std::string& weight_column,
    std::size_t byte_limit,bool allow_unversioned,const std::atomic<bool>* cancelled,
    const ReaderFileGuard* snapshot_source=nullptr,TmaCaseInputScope scope={});
std::size_t save_tma_cohort_case_summary_file(const TmaCohort&,const std::string& output_path,
    const std::vector<std::string>& input_paths,const std::string& value_column,const std::string& weight_column,
    std::size_t byte_limit,bool allow_unversioned,const std::atomic<bool>* cancelled,
    const ReaderFileGuard* snapshot_source=nullptr,TmaCaseInputScope scope={});
}

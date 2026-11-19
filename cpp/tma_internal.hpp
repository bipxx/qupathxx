#pragma once
#include "qupathxx/tma.hpp"
#include <string_view>

namespace qpx::detail {
void validate_tma_cohort(const TmaCohort&,std::size_t,const std::atomic<bool>*);
struct TmaCaseInputScope {
    std::string_view project_identity;
    std::uint64_t image_id=0;
    bool match_columns_by_name=false;
};
std::string tma_scoped_case_summary_csv(const TmaGrid&,const std::vector<std::string_view>& tables,
    const std::string& value,const std::string& weight,std::size_t,bool,const std::atomic<bool>*,TmaCaseInputScope);
std::string tma_scoped_case_summary_csv(const TmaCohort&,const std::vector<std::string_view>& tables,
    const std::string& value,const std::string& weight,std::size_t,bool,const std::atomic<bool>*,TmaCaseInputScope);
struct TmaIndex {
    std::shared_ptr<const TmaGridInfo> info;
    std::map<std::size_t,std::uint64_t> slots;
    std::size_t grid_bytes=0;
    std::size_t payload_bytes() const {return sizeof(TmaIndex)+slots.size()*80+grid_bytes;}
};
}
namespace qpx {
struct TmaCodec {
    static std::shared_ptr<const TmaGridInfo> grid(const TmaGridOptions&);
    static std::vector<std::uint8_t> encode(const TmaGridInfo&);
    static std::shared_ptr<const TmaGridInfo> decode(const void*,std::size_t);
    static std::shared_ptr<const TmaCoreInfo> core(std::shared_ptr<const TmaGridInfo>,std::size_t row,std::size_t column,bool missing,std::string case_id);
};
}

#pragma once
#include "qupathxx/core.hpp"
#include <array>
#include <map>
#include <tuple>
namespace qpx::detail {
// Presentation cache only. Native revisions own invalidation. Poll one additional
// image per drawn Project frame so non-active edits refresh within one cycle.
class GuiKeptCounts {
    using Counts=std::array<std::size_t,3>;
    struct Entry {const Document* owner=nullptr;std::uint64_t revision=~0ULL;Counts counts{};};
    std::map<std::uint64_t,Entry> cached_;
    std::tuple<std::string,std::uint64_t,std::uint64_t> stamp_;
    bool initialized_=false;std::uint64_t cursor_=0;Counts total_{};
    void refresh(std::uint64_t id,const Document* doc) {
        auto& entry=cached_[id];if(entry.owner==doc && (!doc || entry.revision==doc->revision()))return;
        for(std::size_t i=0;i<3;++i)total_[i]-=entry.counts[i];
        entry={doc,doc?doc->revision():0,doc?Counts{doc->analysis_record_count(),doc->tissue_record_count(),doc->cell_record_count()}:Counts{}};
        for(std::size_t i=0;i<3;++i)total_[i]+=entry.counts[i];
    }
public:
    template<class Entries>Counts update(const Entries& entries,const std::string& identity,std::uint64_t generation,std::uint64_t revision,std::uint64_t active) {
        const auto stamp=std::make_tuple(identity,generation,revision);
        if(!initialized_ || stamp!=stamp_) {
            initialized_=true;stamp_=stamp;cached_.clear();total_={};cursor_=0;
            for(const auto& [id,entry]:entries)refresh(id,entry.document.get());
        } else if(!entries.empty()) {
            if(const auto found=entries.find(active);found!=entries.end())refresh(found->first,found->second.document.get());
            auto next=entries.upper_bound(cursor_);if(next==entries.end())next=entries.begin();
            refresh(next->first,next->second.document.get());cursor_=next->first;
        }
        return total_;
    }
};
}

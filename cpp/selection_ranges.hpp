#pragma once
#include "qupathxx/core.hpp"
#include <algorithm>
namespace qpx::detail {
// Display order is explicit; never select document-ID intervals behind a filter.
inline void select_ordered_range(ObjectSelection& selection,const std::vector<std::uint64_t>& ordered,
    std::uint64_t anchor,std::uint64_t target,bool additive=false) {
    auto last=std::find(ordered.begin(),ordered.end(),target);
    if(last==ordered.end())throw std::out_of_range("selection target is not in the displayed list");
    auto first=std::find(ordered.begin(),ordered.end(),anchor);
    auto ids=additive?selection.ids():std::vector<std::uint64_t>{};
    if(first==ordered.end())ids.push_back(target);
    else {if(first>last)std::swap(first,last);ids.insert(ids.end(),first,last+1);}
    selection.replace(ids,target);
}
}

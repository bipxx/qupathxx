#include "qupathxx/core.hpp"
#include <algorithm>
#include <stdexcept>
#include <limits>

namespace qpx {
void ObjectSelection::sync() const {
    if(structure_revision_==document_.structure_revision()) return;
    bool changed=false;
    for(auto it=ids_.begin();it!=ids_.end();) {
        if(!document_.objects().count(*it)) {
            if(!changed && revision_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("selection revision exhausted");
            changed=true;it=ids_.erase(it);
        } else ++it;
    }
    if(!ids_.count(primary_)) primary_=ids_.empty()?0:*ids_.rbegin();
    if(changed) ++revision_;
    structure_revision_=document_.structure_revision();
}
std::vector<std::uint64_t> ObjectSelection::ids() const { sync(); return {ids_.begin(),ids_.end()}; }
std::uint64_t ObjectSelection::primary() const { sync(); return primary_; }
std::size_t ObjectSelection::size() const { sync(); return ids_.size(); }
bool ObjectSelection::contains(std::uint64_t id) const { sync(); return ids_.count(id)!=0; }
void ObjectSelection::publish(std::set<std::uint64_t> ids,std::uint64_t primary) {
    sync();
    if(ids!=ids_ || primary!=primary_) {
        if(revision_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("selection revision exhausted");
        ++revision_;
    }
    ids_.swap(ids); primary_=primary; structure_revision_=document_.structure_revision();
}
void ObjectSelection::replace(const std::vector<std::uint64_t>& ids,std::uint64_t primary) {
    std::set<std::uint64_t> next;
    for(auto id:ids) {
        if(!document_.objects().count(id)) throw std::out_of_range("selected object does not exist");
        next.insert(id);
    }
    if(primary && !next.count(primary)) throw std::invalid_argument("primary object must belong to selection");
    if(!primary && !ids.empty()) primary=ids.back();
    publish(std::move(next),primary);
}
void ObjectSelection::toggle(std::uint64_t id) {
    if(!document_.objects().count(id)) throw std::out_of_range("selected object does not exist");
    sync();
    if(revision_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("selection revision exhausted");
    if(ids_.count(id)) {
        ids_.erase(id);
        if(primary_==id) primary_=ids_.empty()?0:*ids_.rbegin();
    } else { ids_.insert(id); primary_=id; }
    ++revision_;
}
void ObjectSelection::clear() {publish({},0);}
void ObjectSelection::select_all() {
    sync(); std::set<std::uint64_t> next;
    for(const auto& [id,a]:document_.objects()) next.insert(id);
    auto primary=primary_?primary_:(next.empty()?0:*next.rbegin());
    publish(std::move(next),primary);
}
void ObjectSelection::select_range(std::uint64_t anchor,std::uint64_t target,bool additive) {
    if(!document_.objects().count(anchor) || !document_.objects().count(target)) throw std::out_of_range("range endpoint does not exist");
    sync(); auto next=additive?ids_:std::set<std::uint64_t>{};
    for(auto it=document_.objects().lower_bound(std::min(anchor,target));it!=document_.objects().end() && it->first<=std::max(anchor,target);++it)
        next.insert(it->first);
    publish(std::move(next),target);
}
void ObjectSelection::select_region(Rect bounds,bool additive) {
    // A window selects objects fully enclosed by their combined body/nucleus
    // envelope. Spatial broad-phase avoids scanning unrelated objects.
    auto hits=document_.query_region(bounds); sync();
    auto next=additive?ids_:std::set<std::uint64_t>{};
    std::uint64_t primary=additive?primary_:0;
    for(auto id:hits) {
        auto b=document_.objects().at(id).bounds();
        if(b.x>=bounds.x && b.y>=bounds.y && b.x+b.width<=bounds.x+bounds.width && b.y+b.height<=bounds.y+bounds.height) {
            next.insert(id); primary=id;
        }
    }
    publish(std::move(next),primary);
}
}

#include "qupathxx/core.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace qpx {
namespace {
bool same_rect(const Rect& a,const Rect& b) {
    return a.x==b.x && a.y==b.y && a.width==b.width && a.height==b.height;
}
bool same_object(const std::optional<Annotation>& a,const std::optional<Annotation>& b) {
    if (a.has_value()!=b.has_value()) return false;
    if (!a) return true;
    return a->id==b->id && a->name==b->name && a->class_name==b->class_name && a->parent_id==b->parent_id &&
        a->roi.x==b->roi.x && a->roi.y==b->roi.y && a->roi.width==b->roi.width && a->roi.height==b->roi.height;
}
bool encloses(const Rect& a,const Rect& b) {
    return b.x>=a.x && b.y>=a.y && b.x+b.width<=a.x+a.width && b.y+b.height<=a.y+a.height;
}
void text(const std::string& value) {
    if (value.size()>4096 || value.find('\0')!=std::string::npos) throw std::invalid_argument("object text must be at most 4096 bytes and contain no NUL");
}
}
ObjectEdit ObjectEdit::create(Rect roi,std::string name,std::uint64_t parent,std::string classification) {
    return {ObjectEditKind::Create,{0,std::move(name),roi,parent,std::move(classification),0}};
}
ObjectEdit ObjectEdit::replace(std::uint64_t id,Rect roi,std::string name,std::uint64_t parent,std::string classification) {
    return {ObjectEditKind::Replace,{id,std::move(name),roi,parent,std::move(classification),0}};
}
ObjectEdit ObjectEdit::remove(std::uint64_t id) { return {ObjectEditKind::Remove,{id,"",{},0,"",0}}; }

struct Document::Staging {
    Document& document;
    ObjectPatch before,after;
    std::map<std::uint64_t,std::set<std::uint64_t>> child_sets;
    explicit Staging(Document& d) : document(d) {}
    std::optional<Annotation> get(std::uint64_t id) const {
        auto changed=after.find(id); if (changed!=after.end()) return changed->second;
        auto existing=document.objects_.find(id);
        return existing==document.objects_.end()?std::nullopt:std::optional<Annotation>(existing->second);
    }
    const std::set<std::uint64_t>& children(std::uint64_t id) const {
        auto changed=child_sets.find(id); if (changed!=child_sets.end()) return changed->second;
        auto existing=document.children_.find(id); static const std::set<std::uint64_t> empty;
        return existing==document.children_.end()?empty:existing->second;
    }
    std::set<std::uint64_t>& edit_children(std::uint64_t parent) {
        auto found=child_sets.find(parent);
        if (found==child_sets.end()) found=child_sets.emplace(parent,children(parent)).first;
        return found->second;
    }
    void write(std::uint64_t id,std::optional<Annotation> value) {
        auto previous=get(id);
        if (!before.count(id)) before.emplace(id,previous);
        const auto old_parent=previous?previous->parent_id:0, new_parent=value?value->parent_id:0;
        if (old_parent!=new_parent) {
            if (old_parent) edit_children(old_parent).erase(id);
            if (new_parent) edit_children(new_parent).insert(id);
        }
        after.insert_or_assign(id,std::move(value));
    }
    void remove_subtree(std::uint64_t id) {
        if (!get(id)) throw std::out_of_range("annotation does not exist");
        std::vector<std::uint64_t> work{id}; std::set<std::uint64_t> visited;
        for (std::size_t i=0;i<work.size();++i) {
            auto current=work[i]; if (!visited.insert(current).second) continue;
            for (auto child:children(current)) work.push_back(child);
        }
        for (auto current:visited) write(current,std::nullopt);
    }
    void normalize() {
        for (auto it=after.begin();it!=after.end();) {
            if (same_object(before.at(it->first),it->second)) { before.erase(it->first); it=after.erase(it); }
            else ++it;
        }
    }
    void validate_object(const Annotation& a) const {
        a.roi.validate(); text(a.name); text(a.class_name);
        const auto limit=static_cast<double>(std::numeric_limits<std::int64_t>::max());
        if (!a.id || a.roi.x<0 || a.roi.y<0 || a.roi.x+a.roi.width>=limit || a.roi.y+a.roi.height>=limit ||
            static_cast<long double>(a.roi.x+a.roi.width)>document.metadata().width ||
            static_cast<long double>(a.roi.y+a.roi.height)>document.metadata().height)
            throw std::invalid_argument("ROI must lie inside the image");
        if (a.parent_id) {
            auto parent=get(a.parent_id);
            if (!parent) throw std::invalid_argument("parent annotation does not exist");
            if (!encloses(parent->roi,a.roi)) throw std::invalid_argument("parent ROI must contain its child");
        }
    }
    void validate() const {
        for (const auto& [id,value]:after) {
            if (!value) continue;
            validate_object(*value);
            std::set<std::uint64_t> visited{id};
            auto parent=value->parent_id;
            while (parent) {
                if (!visited.insert(parent).second) throw std::invalid_argument("object hierarchy contains a cycle");
                if (visited.size()>256) throw std::invalid_argument("object hierarchy exceeds 256 levels");
                auto ancestor=get(parent);
                if (!ancestor) throw std::invalid_argument("parent annotation does not exist");
                parent=ancestor->parent_id;
            }
            const auto& previous=before.at(id);
            // Geometry edits validate unchanged immediate children; names/classes
            // do not require an O(number of children) containment traversal.
            if (!previous || !same_rect(previous->roi,value->roi)) for (auto child:children(id)) {
                auto child_value=get(child);
                if (child_value) validate_object(*child_value);
            }
            if ((!previous || previous->parent_id!=value->parent_id) && !children(id).empty()) {
                std::vector<std::pair<std::uint64_t,std::size_t>> descendants;
                for (auto child:children(id)) descendants.emplace_back(child,visited.size()+1);
                for (std::size_t i=0;i<descendants.size();++i) {
                    const auto [child,depth]=descendants[i];
                    if (depth>256) throw std::invalid_argument("object hierarchy exceeds 256 levels");
                    for (auto grandchild:children(child)) descendants.emplace_back(grandchild,depth+1);
                }
            }
        }
    }
};
std::vector<Annotation> Document::annotations() const {
    std::vector<Annotation> result; result.reserve(objects_.size());
    for (const auto& [id,a]:objects_) result.push_back(a);
    return result;
}
std::vector<std::uint64_t> Document::children(std::uint64_t parent) const {
    if (!parent) {
        std::vector<std::uint64_t> roots;
        for (const auto& [id,a]:objects_) if (!a.parent_id) roots.push_back(id);
        return roots;
    }
    find(parent); auto it=children_.find(parent);
    return it==children_.end()?std::vector<std::uint64_t>{}:std::vector<std::uint64_t>(it->second.begin(),it->second.end());
}
void Document::commit(Staging& staged,Calibration calibration) {
    staged.validate();
    if (revision_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("document revision exhausted");
    // Prepare every allocating node before touching canonical state.
    std::map<std::uint64_t,Annotation> nodes;
    for (auto& [id,value]:staged.after) if (value) {
        value->revision=revision_+1; nodes.emplace(id,std::move(*value));
    }
    std::map<std::uint64_t,std::set<std::uint64_t>> child_nodes;
    for (auto& [parent,ids]:staged.child_sets) if (!ids.empty()) child_nodes.emplace(parent,std::move(ids));
    // std::map node transfer with integral comparison does not allocate.
    for (const auto& [id,value]:staged.after) objects_.erase(id);
    objects_.merge(nodes);
    for (const auto& [parent,ids]:staged.child_sets) children_.erase(parent);
    children_.merge(child_nodes);
    pixel_width_um_=calibration.x; pixel_height_um_=calibration.y; ++revision_;
}
void Document::trim_history() {
    while (undo_.size()+redo_.size()>history_steps_ || history_bytes_>history_budget_) {
        auto& stack=undo_.empty()?redo_:undo_;
        if (stack.empty()) break;
        history_bytes_-=stack.front().bytes; stack.erase(stack.begin());
    }
}
void Document::clear_history() { undo_.clear(); redo_.clear(); history_bytes_=0; }
void Document::set_history_limits(std::size_t steps,std::size_t bytes) {
    if (steps>1000000) throw std::invalid_argument("history step limit exceeds 1000000");
    history_steps_=steps; history_budget_=bytes; trim_history();
}
void Document::save_edit(Staging& staged,Calibration calibration,std::string label) {
    text(label); staged.normalize();
    if (staged.after.empty() && calibration.x==pixel_width_um_ && calibration.y==pixel_height_um_) return;
    std::size_t bytes=sizeof(EditRecord)+label.size();
    for (auto* patch:{&staged.before,&staged.after}) for (const auto& [id,a]:*patch)
        bytes+=sizeof(std::uint64_t)+sizeof(std::optional<Annotation>)+(a?a->name.size()+a->class_name.size():0);
    const bool recording=history_steps_ && history_budget_;
    if (recording && bytes>history_budget_) throw std::length_error("edit exceeds history payload budget; raise limits or explicitly disable recording");
    EditRecord record;
    if (recording) {
        record={staged.before,staged.after,{pixel_width_um_,pixel_height_um_},calibration,std::move(label),bytes};
        undo_.reserve(undo_.size()+1);
    }
    commit(staged,calibration);
    for (const auto& entry:redo_) history_bytes_-=entry.bytes;
    redo_.clear();
    if (recording) { history_bytes_+=record.bytes; undo_.push_back(std::move(record)); trim_history(); }
    else clear_history();
}
std::vector<std::uint64_t> Document::execute(const std::vector<ObjectEdit>& edits,std::string label) {
    Staging staged(*this); auto next=next_id_; std::vector<std::uint64_t> created;
    for (const auto& edit:edits) {
        if (edit.kind==ObjectEditKind::Create) {
            if (next==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("object identity exhausted");
            auto value=edit.value; value.id=next++; value.revision=0; staged.write(value.id,value); created.push_back(value.id);
        } else if (edit.kind==ObjectEditKind::Replace) {
            if (!staged.get(edit.value.id)) throw std::out_of_range("annotation does not exist");
            staged.write(edit.value.id,edit.value);
        } else if (edit.kind==ObjectEditKind::Remove) staged.remove_subtree(edit.value.id);
        else throw std::invalid_argument("invalid object edit kind");
    }
    created.erase(std::remove_if(created.begin(),created.end(),[&](auto id){return !staged.get(id);}),created.end());
    save_edit(staged,{pixel_width_um_,pixel_height_um_},std::move(label));
    if (!staged.after.empty()) next_id_=next;
    return created;
}
std::uint64_t Document::add_rectangle(Rect roi,std::string name) {
    return execute({ObjectEdit::create(roi,std::move(name))},"Add rectangle").at(0);
}
bool Document::remove_annotation(std::uint64_t id) {
    if (!objects_.count(id)) return false;
    execute({ObjectEdit::remove(id)},"Delete subtree"); return true;
}
void Document::update_rectangle(std::uint64_t id,Rect roi) {
    auto a=find(id); execute({ObjectEdit::replace(id,roi,a.name,a.parent_id,a.class_name)},"Edit rectangle");
}
void Document::rename_annotation(std::uint64_t id,std::string name) {
    auto a=find(id); execute({ObjectEdit::replace(id,a.roi,std::move(name),a.parent_id,a.class_name)},"Rename annotation");
}
void Document::classify_annotation(std::uint64_t id,std::string classification) {
    auto a=find(id); execute({ObjectEdit::replace(id,a.roi,a.name,a.parent_id,std::move(classification))},"Classify annotation");
}
void Document::reparent_annotation(std::uint64_t id,std::uint64_t parent) {
    auto a=find(id); execute({ObjectEdit::replace(id,a.roi,a.name,parent,a.class_name)},"Reparent annotation");
}
void Document::set_calibration(double x,double y) {
    if (!std::isfinite(x) || !std::isfinite(y) || x<=0 || y<=0) throw std::invalid_argument("calibration must be finite and positive");
    Staging staged(*this); save_edit(staged,{{x},{y}},"Set calibration");
}
void Document::clear_calibration() { Staging staged(*this); save_edit(staged,{},"Clear calibration"); }
void Document::apply_record(const EditRecord& record,bool forward) {
    Staging staged(*this); const auto& patch=forward?record.after:record.before;
    for (const auto& [id,value]:patch) staged.write(id,value);
    commit(staged,forward?record.after_calibration:record.before_calibration);
}
bool Document::undo() {
    if (undo_.empty()) return false;
    redo_.reserve(redo_.size()+1); apply_record(undo_.back(),false);
    redo_.push_back(std::move(undo_.back())); undo_.pop_back(); return true;
}
bool Document::redo() {
    if (redo_.empty()) return false;
    undo_.reserve(undo_.size()+1); apply_record(redo_.back(),true);
    undo_.push_back(std::move(redo_.back())); redo_.pop_back(); return true;
}
}

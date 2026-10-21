#include "qupathxx/core.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace qpx {
namespace {
bool same_object(const std::optional<Annotation>& a,const std::optional<Annotation>& b) {
    if (a.has_value()!=b.has_value()) return false;
    if (!a) return true;
    return a->id==b->id && a->name==b->name && a->class_name==b->class_name && a->parent_id==b->parent_id &&
        a->geometry.equals(b->geometry) && a->kind==b->kind &&
        a->nucleus.has_value()==b->nucleus.has_value() && (!a->nucleus || a->nucleus->equals(*b->nucleus));
}
void text(const std::string& value) {
    if (value.size()>4096 || value.find('\0')!=std::string::npos) throw std::invalid_argument("object text must be at most 4096 bytes and contain no NUL");
}
}
ObjectEdit ObjectEdit::create(Rect roi,std::string name,std::uint64_t parent,std::string classification) {
    return create_geometry(Roi::rectangle(roi),std::move(name),parent,std::move(classification));
}
ObjectEdit ObjectEdit::replace(std::uint64_t id,Rect roi,std::string name,std::uint64_t parent,std::string classification) {
    return replace_geometry(id,Roi::rectangle(roi),std::move(name),parent,std::move(classification));
}
ObjectEdit ObjectEdit::remove(std::uint64_t id) { return {ObjectEditKind::Remove,{id,"",{},0,"",0}}; }
ObjectEdit ObjectEdit::create_geometry(Roi geometry,std::string name,std::uint64_t parent,std::string classification,ObjectKind kind,std::optional<Roi> nucleus) {
    return {ObjectEditKind::Create,{0,std::move(name),std::move(geometry),parent,std::move(classification),0,kind,std::move(nucleus)}};
}
ObjectEdit ObjectEdit::replace_geometry(std::uint64_t id,Roi geometry,std::string name,std::uint64_t parent,std::string classification,ObjectKind kind,std::optional<Roi> nucleus) {
    return {ObjectEditKind::Replace,{id,std::move(name),std::move(geometry),parent,std::move(classification),0,kind,std::move(nucleus)}};
}

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
        text(a.name); text(a.class_name);
        if(a.kind!=ObjectKind::Annotation && a.kind!=ObjectKind::Detection && a.kind!=ObjectKind::Cell)
            throw std::invalid_argument("invalid object kind");
        if(a.nucleus && a.kind!=ObjectKind::Cell) throw std::invalid_argument("only cells may have a nucleus ROI");
        if(a.kind==ObjectKind::Cell && (!a.geometry.is_area() || (a.nucleus && !a.nucleus->is_area())))
            throw std::invalid_argument("cell and nucleus ROIs must have area");
        const auto roi=a.bounds();
        const auto limit=static_cast<double>(std::numeric_limits<std::int64_t>::max());
        if (!a.id || roi.x<0 || roi.y<0 || roi.x+roi.width>=limit || roi.y+roi.height>=limit ||
            static_cast<long double>(roi.x+roi.width)>document.metadata().width ||
            static_cast<long double>(roi.y+roi.height)>document.metadata().height)
            throw std::invalid_argument("ROI must lie inside the image");
        if (a.parent_id) {
            auto parent=get(a.parent_id);
            if (!parent) throw std::invalid_argument("parent annotation does not exist");
            if (!parent->geometry.covers(a.geometry)) throw std::invalid_argument("parent ROI must contain its child");
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
            if (!previous || !previous->geometry.equals(value->geometry)) for (auto child:children(id)) {
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
    std::optional<std::map<std::uint64_t,std::optional<Rect>>> spatial_delta;
    bool spatial_reset=false;
    if(spatial_ && !spatial_rebuild_) for(const auto& [id,value]:staged.after) {
        auto old=objects_.find(id);
        const auto previous=old==objects_.end()?std::optional<Rect>{}:std::optional<Rect>{old->second.bounds()};
        const auto current=value?std::optional<Rect>{value->bounds()}:std::optional<Rect>{};
        bool unchanged=previous.has_value()==current.has_value();
        if(previous && current) unchanged=previous->x==current->x && previous->y==current->y && previous->width==current->width && previous->height==current->height;
        if(unchanged) continue;
        if(!spatial_delta) spatial_delta=spatial_dirty_;
        spatial_delta->insert_or_assign(id,current);
        if(spatial_delta->size()>256) { spatial_delta.reset(); spatial_reset=true; break; }
    }
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
    if(spatial_reset) { spatial_.reset(); spatial_dirty_.clear(); spatial_rebuild_=true; }
    else if(spatial_delta) spatial_dirty_.swap(*spatial_delta);
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
        bytes+=sizeof(std::uint64_t)+sizeof(std::optional<Annotation>)+(a?a->name.size()+a->class_name.size()+a->geometry.payload_bytes()+(a->nucleus?a->nucleus->payload_bytes():0):0);
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
std::uint64_t Document::add_roi(Roi roi,std::string name) { return execute({ObjectEdit::create_geometry(std::move(roi),std::move(name))},"Add ROI").at(0); }
std::uint64_t Document::add_detection(Roi roi,std::string name,std::uint64_t parent,std::string classification) {
    return execute({ObjectEdit::create_geometry(std::move(roi),std::move(name),parent,std::move(classification),ObjectKind::Detection)},"Add detection").at(0);
}
std::uint64_t Document::add_cell(Roi roi,std::optional<Roi> nucleus,std::string name,std::uint64_t parent,std::string classification) {
    return execute({ObjectEdit::create_geometry(std::move(roi),std::move(name),parent,std::move(classification),ObjectKind::Cell,std::move(nucleus))},"Add cell").at(0);
}
void Document::update_nucleus(std::uint64_t id,std::optional<Roi> nucleus) {
    auto a=find(id);
    if(a.kind!=ObjectKind::Cell) throw std::invalid_argument("only cells may have a nucleus ROI");
    execute({ObjectEdit::replace_geometry(id,a.geometry,a.name,a.parent_id,a.class_name,a.kind,std::move(nucleus))},"Edit nucleus");
}
bool Document::remove_annotation(std::uint64_t id) {
    if (!objects_.count(id)) return false;
    execute({ObjectEdit::remove(id)},"Delete subtree"); return true;
}
void Document::update_rectangle(std::uint64_t id,Rect roi) {
    auto a=find(id); execute({ObjectEdit::replace_geometry(id,Roi::rectangle(roi),a.name,a.parent_id,a.class_name,a.kind,a.nucleus)},"Edit rectangle");
}
void Document::update_roi(std::uint64_t id,Roi roi) {
    auto a=find(id); execute({ObjectEdit::replace_geometry(id,std::move(roi),a.name,a.parent_id,a.class_name,a.kind,a.nucleus)},"Edit ROI");
}
void Document::rename_annotation(std::uint64_t id,std::string name) {
    auto a=find(id); execute({ObjectEdit::replace_geometry(id,a.geometry,std::move(name),a.parent_id,a.class_name,a.kind,a.nucleus)},"Rename annotation");
}
void Document::classify_annotation(std::uint64_t id,std::string classification) {
    auto a=find(id); execute({ObjectEdit::replace_geometry(id,a.geometry,a.name,a.parent_id,std::move(classification),a.kind,a.nucleus)},"Classify annotation");
}
void Document::reparent_annotation(std::uint64_t id,std::uint64_t parent) {
    auto a=find(id); execute({ObjectEdit::replace_geometry(id,a.geometry,a.name,parent,a.class_name,a.kind,a.nucleus)},"Reparent annotation");
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

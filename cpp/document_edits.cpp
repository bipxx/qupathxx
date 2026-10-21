#include "object_snapshot.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/cells.hpp"
#include <algorithm>
#include <array>
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
        a->nucleus.has_value()==b->nucleus.has_value() && (!a->nucleus || a->nucleus->equals(*b->nucleus)) && a->cell_feature_link==b->cell_feature_link;
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
    ClassPatch before_classes,after_classes;
    std::map<std::string,std::size_t> counts;
    std::map<std::uint64_t,std::set<std::uint64_t>> child_sets;
    explicit Staging(Document& d) : document(d) {}
    std::optional<ObjectClass> get_class(const std::string& name) const {
        auto changed=after_classes.find(name); if(changed!=after_classes.end()) return changed->second;
        auto existing=document.classes_.find(name);
        return existing==document.classes_.end()?std::nullopt:std::optional<ObjectClass>(existing->second);
    }
    bool has_class(const std::string& name) const {
        auto changed=after_classes.find(name);
        return changed==after_classes.end()?document.classes_.count(name)!=0:changed->second.has_value();
    }
    void write_class(const std::string& name,std::optional<ObjectClass> value) {
        if(!before_classes.count(name)) before_classes.emplace(name,get_class(name));
        after_classes.insert_or_assign(name,std::move(value));
    }
    std::size_t count(const std::string& name) const {
        auto current=counts.find(name);
        return current==counts.end()?document.class_count(name):current->second;
    }
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
        const auto old_class=previous?previous->class_name:"",new_class=value?value->class_name:"";
        if(old_class!=new_class) {
            if(!old_class.empty()) counts.insert_or_assign(old_class,count(old_class)-1);
            if(!new_class.empty()) counts.insert_or_assign(new_class,count(new_class)+1);
        }
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
        // Existing string classifications remain valid: first use creates a
        // registry entry in the same transaction. Explicit removals are never
        // silently recreated and fail validation if still referenced.
        for(const auto& [id,a]:after) if(a && !a->class_name.empty() && !has_class(a->class_name) && !after_classes.count(a->class_name))
            write_class(a->class_name,ObjectClass{a->class_name});
        for(auto it=after_classes.begin();it!=after_classes.end();) {
            const auto& old=before_classes.at(it->first); const auto& current=it->second;
            if(old.has_value()==current.has_value() && (!old || (old->name==current->name && old->color==current->color))) {
                before_classes.erase(it->first); it=after_classes.erase(it);
            } else ++it;
        }
    }
    void validate_object(const Annotation& a) const {
        text(a.name); text(a.class_name);
        if(!a.class_name.empty() && !has_class(a.class_name)) throw std::invalid_argument("object class is not registered");
        if(a.kind!=ObjectKind::Annotation && a.kind!=ObjectKind::Detection && a.kind!=ObjectKind::Cell)
            throw std::invalid_argument("invalid object kind");
        if(a.nucleus && a.kind!=ObjectKind::Cell) throw std::invalid_argument("only cells may have a nucleus ROI");
        if(a.kind==ObjectKind::Cell && (!a.geometry.is_area() || (a.nucleus && !a.nucleus->is_area())))
            throw std::invalid_argument("cell and nucleus ROIs must have area");
        if(a.cell_feature_link && (a.kind!=ObjectKind::Cell || !a.cell_feature_link->record_id || a.cell_feature_link->record_id>document.revision_ || !a.cell_feature_link->label || a.cell_feature_link->label>1000000))
            throw std::invalid_argument("invalid cell feature link");
        if(a.cell_feature_link) {
            const auto run=document.cells_.find(a.cell_feature_link->record_id);
            if(run!=document.cells_.end() && a.cell_feature_link->label>run->second.value->result->features().size()) throw std::invalid_argument("invalid cell feature label");
        }
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
        for(const auto& [name,value]:after_classes) {
            text(name);
            if(name.empty()) throw std::invalid_argument("class name must not be empty");
            if(value && (value->name!=name || value->color>0xffffff)) throw std::invalid_argument("class color must be a 24-bit RGB value");
            if(!value && count(name)) throw std::invalid_argument("cannot remove a class still assigned to objects");
        }
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
std::size_t Document::object_payload(const Annotation& a) {
    return sizeof(Annotation)+64+a.name.size()+a.class_name.size()+a.geometry.payload_bytes()+(a.nucleus?a.nucleus->payload_bytes():0);
}
std::shared_ptr<Document> Document::restore(std::shared_ptr<ImageServer> server,
    const std::vector<Annotation>& objects,const std::vector<ObjectClass>& classes,
    std::optional<double> x,std::optional<double> y,std::uint64_t next_id,std::uint64_t revision) {
    if(!next_id || (x && (!std::isfinite(*x) || *x<=0)) || (y && (!std::isfinite(*y) || *y<=0)))
        throw std::invalid_argument("invalid persisted calibration or next object ID");
    // The candidate is private until all validation succeeds. Build canonical
    // nodes once instead of allocating before/after edit patches and committing
    // a synthetic edit just to reconstruct an unpublished document.
    auto result=std::make_shared<Document>(std::move(server));result->revision_=revision;
    for(const auto& c:classes) {
        text(c.name);
        if(c.name.empty()) throw std::invalid_argument("class name must not be empty");
        if(c.color>0xffffff) throw std::invalid_argument("class color must be a 24-bit RGB value");
        if(!result->classes_.emplace(c.name,c).second) throw std::invalid_argument("duplicate persisted class");
    }
    for(const auto& a:objects) {
        if(!a.id || a.id>=next_id || !a.revision || a.revision>revision || (a.cell_feature_link && a.cell_feature_link->record_id>revision) || !result->objects_.emplace(a.id,a).second)
            throw std::invalid_argument("invalid persisted object identity or revision");
    }
#ifdef QUPATHXX_HAS_PROJECT
    // The candidate is private; build immutable save blocks once on the loader
    // worker rather than deferring a full traversal to the first UI capture.
    for(auto first=result->objects_.begin();first!=result->objects_.end();) {
        const auto key=(first->first-1)/256;auto last=first;std::size_t count=0;
        while(last!=result->objects_.end() && (last->first-1)/256==key) {++last;++count;}
        auto block=std::make_shared<ObjectBlock>();block->values.reserve(count);block->bytes=sizeof(ObjectBlock)+64;
        for(auto it=first;it!=last;++it) {block->values.push_back(it->second);block->bytes+=object_payload(it->second);}
        result->object_bytes_+=block->bytes;result->object_blocks_.emplace(key,std::move(block));first=last;
    }
#endif
    Staging validator(*result);
    std::map<std::uint64_t,std::size_t> depths;
    for(const auto& [id,a]:result->objects_) {
        // Share object/class/ROI/parent containment rules with live edits.
        validator.validate_object(a);
        if(!a.class_name.empty()) ++result->class_counts_[a.class_name];
        if(!a.parent_id) continue;
        result->children_[a.parent_id].insert(id);
        std::array<std::uint64_t,256> chain;
        std::size_t length=0,depth=0;
        auto current=id;
        while(current) {
            auto known=depths.find(current);
            if(known!=depths.end()) {depth=known->second; break;}
            if(std::find(chain.begin(),chain.begin()+length,current)!=chain.begin()+length)
                throw std::invalid_argument("object hierarchy contains a cycle");
            if(length==chain.size()) throw std::invalid_argument("object hierarchy exceeds 256 levels");
            chain[length++]=current;
            auto node=result->objects_.find(current);
            if(node==result->objects_.end()) throw std::invalid_argument("parent annotation does not exist");
            current=node->second.parent_id;
        }
        if(depth+length>chain.size()) throw std::invalid_argument("object hierarchy exceeds 256 levels");
        while(length) depths.emplace(chain[--length],++depth);
    }
    result->pixel_width_um_=x; result->pixel_height_um_=y;
    result->next_id_=next_id; result->revision_=revision;
    result->structure_revision_=objects.empty()?0:revision;
    return result;
}
ObjectClass Document::object_class(const std::string& name) const {
    auto found=classes_.find(name); if(found==classes_.end()) throw std::out_of_range("class does not exist");
    return found->second;
}
std::size_t Document::class_count(const std::string& name) const {
    auto found=class_counts_.find(name); return found==class_counts_.end()?0:found->second;
}
void Document::set_class(std::string name,std::uint32_t color) {
    Staging staged(*this); staged.write_class(name,ObjectClass{name,color});
    save_edit(staged,{pixel_width_um_,pixel_height_um_},"Set class");
}
void Document::remove_class(const std::string& name) {
    object_class(name); Staging staged(*this); staged.write_class(name,std::nullopt);
    save_edit(staged,{pixel_width_um_,pixel_height_um_},"Remove class");
}
void Document::rename_class(const std::string& old_name,std::string new_name) {
    auto current=object_class(old_name); if(old_name==new_name) return;
    if(classes_.count(new_name)) throw std::invalid_argument("class name already exists");
    Staging staged(*this); current.name=new_name;
    staged.write_class(new_name,current); staged.write_class(old_name,std::nullopt);
    for(const auto& [id,a]:objects_) if(a.class_name==old_name) {
        auto value=a; value.class_name=new_name; staged.write(id,std::move(value));
    }
    save_edit(staged,{pixel_width_um_,pixel_height_um_},"Rename class");
}
void Document::classify_objects(const std::vector<std::uint64_t>& ids,std::string classification) {
    text(classification); Staging staged(*this);
    for(auto id:ids) { auto value=find(id); value.class_name=classification; staged.write(id,std::move(value)); }
    save_edit(staged,{pixel_width_um_,pixel_height_um_},"Classify objects");
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
    bool membership_changed=false;
    for(const auto& [id,value]:staged.after)
        membership_changed |= staged.before.at(id).has_value()!=value.has_value();
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
    std::map<std::string,ObjectClass> class_nodes;
    for(const auto& [name,value]:staged.after_classes) if(value) class_nodes.emplace(name,*value);
    std::map<std::string,std::size_t> count_nodes;
    for(const auto& [name,count]:staged.counts) if(count) count_nodes.emplace(name,count);
#ifdef QUPATHXX_HAS_PROJECT
    std::map<std::uint64_t,std::shared_ptr<const ObjectBlock>> block_nodes;
    std::vector<std::uint64_t> changed_blocks;
    auto object_bytes=object_bytes_;
    for(auto first=staged.after.begin();first!=staged.after.end();) {
        const auto key=(first->first-1)/256;auto last=first;
        while(last!=staged.after.end() && (last->first-1)/256==key) ++last;
        changed_blocks.push_back(key);auto block=std::make_shared<ObjectBlock>();
        const auto old=object_blocks_.find(key);
        const std::vector<Annotation> empty;const auto& previous=old==object_blocks_.end()?empty:old->second->values;
        block->bytes=old==object_blocks_.end()?sizeof(ObjectBlock)+64:old->second->bytes;
        if(old!=object_blocks_.end()) object_bytes-=old->second->bytes;
        auto count=previous.size();
        for(auto it=first;it!=last;++it) {if(staged.before.at(it->first)) --count;if(it->second) ++count;}
        if(count>256) throw std::logic_error("object snapshot block count is inconsistent");
        block->values.reserve(count);auto prior=previous.begin();
        for(auto it=first;it!=last;++it) {
            const auto id=it->first;
            while(prior!=previous.end() && prior->id<id) block->values.push_back(*prior++);
            if(prior!=previous.end() && prior->id==id) {block->bytes-=object_payload(*prior);++prior;}
            const auto next=nodes.find(id);
            if(next!=nodes.end()) {block->values.push_back(next->second);block->bytes+=object_payload(next->second);}
        }
        while(prior!=previous.end()) block->values.push_back(*prior++);
        if(block->values.size()!=count) throw std::logic_error("object snapshot block membership is inconsistent");
        block_nodes.emplace(key,std::move(block));first=last;
    }
    for(auto it=block_nodes.begin();it!=block_nodes.end();) {
        if(it->second->values.empty()) it=block_nodes.erase(it);
        else {if(it->second->bytes>std::numeric_limits<std::size_t>::max()-object_bytes) throw std::overflow_error("object snapshot payload exhausted");object_bytes+=it->second->bytes;++it;}
    }
#endif
    // std::map node transfer with integral comparison does not allocate.
    for (const auto& [id,value]:staged.after) objects_.erase(id);
    objects_.merge(nodes);
#ifdef QUPATHXX_HAS_PROJECT
    for(auto key:changed_blocks) object_blocks_.erase(key);
    object_blocks_.merge(block_nodes);object_bytes_=object_bytes;
#endif
    for (const auto& [parent,ids]:staged.child_sets) children_.erase(parent);
    children_.merge(child_nodes);
    for(const auto& [name,value]:staged.after_classes) classes_.erase(name);
    classes_.merge(class_nodes);
    for(const auto& [name,count]:staged.counts) class_counts_.erase(name);
    class_counts_.merge(count_nodes);
    if(spatial_reset) { spatial_.reset(); spatial_dirty_.clear(); spatial_rebuild_=true; }
    else if(spatial_delta) spatial_dirty_.swap(*spatial_delta);
    if(pixel_width_um_!=calibration.x || pixel_height_um_!=calibration.y) calibration_revision_=revision_+1;
    pixel_width_um_=calibration.x; pixel_height_um_=calibration.y; ++revision_;
    if(membership_changed) structure_revision_=revision_;
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
    if (staged.after.empty() && staged.after_classes.empty() && calibration.x==pixel_width_um_ && calibration.y==pixel_height_um_) return;
    std::size_t bytes=sizeof(EditRecord)+label.size();
    for (auto* patch:{&staged.before,&staged.after}) for (const auto& [id,a]:*patch)
        bytes+=sizeof(std::uint64_t)+sizeof(std::optional<Annotation>)+(a?a->name.size()+a->class_name.size()+a->geometry.payload_bytes()+(a->nucleus?a->nucleus->payload_bytes():0):0);
    for(auto* patch:{&staged.before_classes,&staged.after_classes}) for(const auto& [name,value]:*patch)
        bytes+=sizeof(std::string)+name.size()+sizeof(std::optional<ObjectClass>)+(value?value->name.size():0);
    const bool recording=history_steps_ && history_budget_;
    if (recording && bytes>history_budget_) throw std::length_error("edit exceeds history payload budget; raise limits or explicitly disable recording");
    EditRecord record;
    if (recording) {
        record={staged.before,staged.after,{pixel_width_um_,pixel_height_um_},calibration,
            staged.before_classes,staged.after_classes,std::move(label),bytes};
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
            const auto previous=staged.get(edit.value.id);if (!previous) throw std::out_of_range("annotation does not exist");
            auto value=edit.value;
            if(value.kind==ObjectKind::Cell && !value.cell_feature_link) value.cell_feature_link=previous->cell_feature_link;
            staged.write(value.id,std::move(value));
        } else if (edit.kind==ObjectEditKind::Remove) staged.remove_subtree(edit.value.id);
        else throw std::invalid_argument("invalid object edit kind");
    }
    created.erase(std::remove_if(created.begin(),created.end(),[&](auto id){return !staged.get(id);}),created.end());
    save_edit(staged,{pixel_width_um_,pixel_height_um_},std::move(label));
    if (!staged.after.empty()) next_id_=next;
    return created;
}
std::vector<std::uint64_t> Document::append_objects(const std::vector<Annotation>& objects,const std::vector<ObjectClass>& classes) {
    Staging staged(*this); auto next=next_id_;
    std::map<std::uint64_t,std::uint64_t> mapping; std::vector<std::uint64_t> created;
    for(const auto& a:objects) {
        if(!a.id || mapping.count(a.id)) throw std::invalid_argument("import object IDs must be unique and nonzero");
        if(next==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("object identity exhausted");
        mapping.emplace(a.id,next); created.push_back(next++);
    }
    for(const auto& c:classes) {
        auto existing=staged.get_class(c.name);
        if(existing && existing->color!=c.color) throw std::invalid_argument("import class color conflicts with existing definition");
        if(!existing) staged.write_class(c.name,c);
    }
    for(const auto& a:objects) {
        auto value=a; value.id=mapping.at(a.id); value.revision=0;value.cell_feature_link.reset();
        if(value.parent_id) {
            auto parent=mapping.find(value.parent_id);
            if(parent==mapping.end()) throw std::invalid_argument("import parent is not in the imported batch");
            value.parent_id=parent->second;
        }
        staged.write(value.id,std::move(value));
    }
    save_edit(staged,{pixel_width_um_,pixel_height_um_},"Import annotations");
    if(!objects.empty()) next_id_=next;
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
void Document::remove_objects(const std::vector<std::uint64_t>& ids) {
    // Validate all requested IDs first, then skip descendants removed by an
    // earlier selected ancestor. Duplicate and nested selections are safe.
    std::set<std::uint64_t> unique;
    for(auto id:ids) { find(id); unique.insert(id); }
    Staging staged(*this);
    for(auto id:unique) if(staged.get(id)) staged.remove_subtree(id);
    save_edit(staged,{pixel_width_um_,pixel_height_um_},"Delete objects");
}
void Document::update_rectangle(std::uint64_t id,Rect roi) {
    auto a=find(id); execute({ObjectEdit::replace_geometry(id,Roi::rectangle(roi),a.name,a.parent_id,a.class_name,a.kind,a.nucleus)},"Edit rectangle");
}
void Document::update_roi(std::uint64_t id,Roi roi) {
    publish_roi(id,std::move(roi),"Edit ROI");
}
void Document::publish_roi(std::uint64_t id,Roi geometry,std::string label) {
    const auto& a=find(id);
    execute({ObjectEdit::replace_geometry(id,std::move(geometry),a.name,a.parent_id,a.class_name,a.kind,a.nucleus)},std::move(label));
}
void Document::edit_vertex(std::uint64_t id,std::size_t path,std::size_t vertex,Point point) {
    publish_roi(id,find(id).geometry.with_vertex(path,vertex,point),"Move vertex");
}
void Document::insert_vertex(std::uint64_t id,std::size_t path,std::size_t vertex,Point point) {
    publish_roi(id,find(id).geometry.inserted_vertex(path,vertex,point),"Insert vertex");
}
void Document::remove_vertex(std::uint64_t id,std::size_t path,std::size_t vertex) {
    publish_roi(id,find(id).geometry.without_vertex(path,vertex),"Remove vertex");
}
void Document::add_hole(std::uint64_t id,std::size_t part,Ring hole) {
    publish_roi(id,find(id).geometry.with_hole(part,std::move(hole)),"Add hole");
}
void Document::remove_hole(std::uint64_t id,std::size_t part,std::size_t hole) {
    publish_roi(id,find(id).geometry.without_hole(part,hole),"Remove hole");
}
void Document::apply_boolean(std::uint64_t target,const std::vector<std::uint64_t>& operands,BooleanOp operation) {
    std::optional<Roi> geometry=find(target).geometry;
    if(!geometry->is_area()) throw std::invalid_argument("Boolean operations require area ROIs");
    if(operands.empty()) throw std::invalid_argument("Boolean edit requires operands");
    std::set<std::uint64_t> unique;
    // Resolve every input before computation. Ordered operands define the fold;
    // repeats and the target itself are rejected instead of silently subtracted.
    for(auto id:operands) {
        if(!find(id).geometry.is_area()) throw std::invalid_argument("Boolean operations require area ROIs");
        if(id==target || !unique.insert(id).second) throw std::invalid_argument("Boolean operands must be distinct from each other and target");
    }
    for(auto id:operands) {
        if(geometry) geometry=geometry->boolean_operation(find(id).geometry,operation);
        else if(operation==BooleanOp::SymmetricDifference || operation==BooleanOp::Union) geometry=find(id).geometry.as_polygon();
    }
    if(!geometry) throw std::invalid_argument("Boolean result has no area; remove the object explicitly");
    publish_roi(target,std::move(*geometry),"Boolean ROI edit");
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
    for(const auto& [name,value]:(forward?record.after_classes:record.before_classes)) staged.write_class(name,value);
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

#include "object_edit_state.hpp"
#include "classification_edit.hpp"
#include "class_registry.hpp"
#include "object_snapshot.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace qpx {
namespace {
void text(const std::string& s) {if(s.size()>4096 || s.find('\0')!=std::string::npos) throw std::invalid_argument("class text must be at most 4096 bytes and contain no NUL");}
template<class T> void swap_nodes(std::map<std::string,T>& live,std::map<std::string,T>& alternate,const std::vector<std::string>& keys) {
    // All maps use std::allocator and std::less<string>. Node transfer and
    // these comparisons allocate nothing; all allocating work precedes this.
    for(const auto& key:keys) {
        auto a=live.extract(key),b=alternate.extract(key);
        if(b) live.insert(std::move(b));
        if(a) alternate.insert(std::move(a));
    }
}
}
namespace {
using ClassChanges=std::map<std::string,std::optional<ObjectClass>>;
struct PreparedClassification {std::shared_ptr<detail::ClassificationEdit> value;std::size_t bytes;};
PreparedClassification prepare_classifications(const Document& document,
        const std::vector<std::pair<std::uint64_t,std::string>>& input,
        std::size_t base_bytes,bool reset_probability,ClassChanges changes={}) {
    const auto less=[](const auto& a,const auto& b){return a.first<b.first;};
    std::vector<std::pair<std::uint64_t,std::string>> sorted;const auto* assignments=&input;
    if(!std::is_sorted(input.begin(),input.end(),less)) {sorted=input;std::sort(sorted.begin(),sorted.end(),less);assignments=&sorted;}
    auto edit=std::make_shared<detail::ClassificationEdit>();
    std::size_t bytes=base_bytes+sizeof(detail::ClassificationEdit);
    auto charge=[&](std::size_t n) {if(n>std::numeric_limits<std::size_t>::max()-bytes)throw std::overflow_error("classification history payload exhausted");bytes+=n;};
    if(input.size()>std::numeric_limits<std::size_t>::max()/sizeof(detail::ClassificationEdit::Row))throw std::overflow_error("classification selection size exhausted");
    charge(input.size()*sizeof(detail::ClassificationEdit::Row));edit->rows.reserve(input.size());
    std::map<std::string,std::int64_t> delta;
    std::uint64_t previous=0;bool first=true;
    for(const auto& [id,name]:*assignments) {
        text(name);if(!first && previous==id)throw std::invalid_argument("duplicate classification object ID");first=false;previous=id;
        const auto found=document.objects().find(id);if(found==document.objects().end())throw std::out_of_range("classification object does not exist");
        const auto& object=found->second;
        const auto properties=reset_probability?detail::reclassified_properties(object.properties,!name.empty()):object.properties;
        if(object.class_name==name && properties==object.properties)continue;
        edit->rows.push_back({id,name});
        if(properties!=object.properties) {edit->property_rows.push_back({edit->rows.size()-1,properties});charge(sizeof(detail::ClassificationEdit::PropertyRow)+(properties?properties->payload_bytes():0)+(object.properties?object.properties->payload_bytes():0));}
        charge(std::max(object.class_name.capacity(),edit->rows.back().alternate.capacity()));
        if(!object.class_name.empty())--delta[object.class_name];
        if(!name.empty())++delta[name];
        if(!name.empty() && !document.classes().count(name) && !changes.count(name))changes.emplace(name,ObjectClass{name});
    }
    edit->count_keys.reserve(delta.size());edit->expected_counts.reserve(delta.size());
    for(const auto& [name,change]:delta) {
        const auto count=document.class_count(name);std::size_t next=count;
        if(change<0) {const auto remove=static_cast<std::uint64_t>(-change);if(remove>count)throw std::logic_error("invalid classification count");next-=static_cast<std::size_t>(remove);}
        else {const auto add=static_cast<std::uint64_t>(change);if(add>std::numeric_limits<std::size_t>::max()-count)throw std::overflow_error("classification count exhausted");next+=static_cast<std::size_t>(add);}
        edit->count_keys.push_back(name);edit->expected_counts.push_back(count);if(next)edit->counts.emplace(name,next);
        charge(2*name.size()+2*sizeof(std::string)+sizeof(std::size_t)+96);
    }
    // Alternate map nodes support additions, removals and replacements. Account
    // for both directions' owning class strings, never unchanged ROI/properties.
    for(const auto& [name,value]:changes) {
        const auto before=document.classes().find(name);const bool exists=before!=document.classes().end();
        if((!exists && !value) || (exists && value && before->second==*value))continue;
        edit->class_keys.push_back(name);edit->expected_classes.push_back(exists);
        if(value)edit->classes.emplace(name,*value);
        const auto strings=[](const ObjectClass& c){return c.name.capacity()+c.parent.capacity();};
        charge(2*name.size()+2*sizeof(std::string)+sizeof(ObjectClass)+96+
            std::max(exists?strings(before->second):std::size_t{},value?strings(*value):std::size_t{}));
    }
    if(edit->rows.empty() && edit->class_keys.empty())return {nullptr,0};
    return {std::move(edit),bytes};
}
}
Document::ClassificationHistory::ClassificationHistory(const ClassificationHistory& other):value(other.value?std::make_shared<detail::ClassificationEdit>(*other.value):nullptr) {}
Document::ClassificationHistory& Document::ClassificationHistory::operator=(const ClassificationHistory& other) {
    if(this!=&other) {ClassificationHistory copy(other);value.swap(copy.value);}return *this;
}
void Document::apply_classifications(const std::vector<std::pair<std::uint64_t,std::string>>& input,std::string label) {
    text(label);if(input.size()>1000000)throw std::length_error("classification selection exceeds one million objects");
    auto prepared=prepare_classifications(*this,input,sizeof(EditRecord)+label.size(),true);
    if(!prepared.value)return;
    const bool recording=history_steps_ && history_budget_;
    if(recording && prepared.bytes>history_budget_)throw std::length_error("edit exceeds history payload budget; raise limits or explicitly disable recording");
    EditRecord record;record.label=std::move(label);record.bytes=prepared.bytes;record.classifications.value=std::move(prepared.value);
    if(recording)undo_.reserve(undo_.size()+1);
    toggle_classifications(*record.classifications.value);
    for(const auto& entry:redo_)history_bytes_-=entry.bytes;
    redo_.clear();
    if(recording) {history_bytes_+=record.bytes;undo_.push_back(std::move(record));trim_history();}
    else clear_history();
}
void Document::rename_class(const std::string& old_name,std::string new_name) {
    const auto current=object_class(old_name);if(old_name==new_name)return;
    auto renamed=current;renamed.name=new_name;detail::validate_class_definition(renamed);
    std::map<std::string,std::string> names;
    for(const auto& [name,value]:classes_) {
        const auto chain=detail::class_chain(value,[&](const std::string& key)->const ObjectClass* {const auto at=classes_.find(key);return at==classes_.end()?nullptr:&at->second;});
        if(std::any_of(chain.begin(),chain.end(),[&](const auto* node){return node->name==old_name;}))names.emplace(name,new_name+name.substr(old_name.size()));
    }
    for(const auto& [before,after]:names)if(classes_.count(after) && !names.count(after))throw std::invalid_argument("class name already exists");
    auto candidate=classes_;for(const auto& [before,after]:names)candidate.erase(before);
    for(const auto& [before,after]:names) {
        auto value=object_class(before);value.name=after;
        if(const auto parent=names.find(value.parent);parent!=names.end())value.parent=parent->second;
        candidate.insert_or_assign(after,std::move(value));
    }
    std::vector<ObjectClass> definitions;definitions.reserve(candidate.size());for(const auto& [key,value]:candidate)definitions.push_back(value);
    candidate=detail::normalized_class_registry(definitions);
    ClassChanges changes;
    for(const auto& [key,value]:classes_)if(!candidate.count(key))changes.emplace(key,std::nullopt);
    for(const auto& [key,value]:candidate) {const auto before=classes_.find(key);if(before==classes_.end() || !(before->second==value))changes.insert_or_assign(key,value);}
    std::vector<std::pair<std::uint64_t,std::string>> assignments;
    for(const auto& [id,object]:objects_)if(const auto name=names.find(object.class_name);name!=names.end())assignments.emplace_back(id,name->second);
    // Administrative rename preserves historical probability and properties.
    // Reuse the same field transaction, snapshot publication and undo toggle.
    const std::string label="Rename class";
    auto prepared=prepare_classifications(*this,assignments,sizeof(EditRecord)+label.size(),false,std::move(changes));
    if(!prepared.value)return;
    const bool recording=history_steps_ && history_budget_;
    if(recording && prepared.bytes>history_budget_)throw std::length_error("edit exceeds history payload budget; raise limits or explicitly disable recording");
    EditRecord record;record.label=label;record.bytes=prepared.bytes;record.classifications.value=std::move(prepared.value);
    if(recording)undo_.reserve(undo_.size()+1);
    toggle_classifications(*record.classifications.value);
    for(const auto& entry:redo_)history_bytes_-=entry.bytes;
    redo_.clear();
    if(recording) {history_bytes_+=record.bytes;undo_.push_back(std::move(record));trim_history();}
    else clear_history();
}
void Document::toggle_classifications(detail::ClassificationEdit& edit) {
    if(revision_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("document revision exhausted");
    const auto next=revision_+1;
    for(std::size_t i=0;i<edit.count_keys.size();++i) if(class_count(edit.count_keys[i])!=edit.expected_counts[i]) throw std::logic_error("classification history count mismatch");
    for(std::size_t i=0;i<edit.class_keys.size();++i) if(bool(classes_.count(edit.class_keys[i]))!=edit.expected_classes[i]) throw std::logic_error("classification history registry mismatch");
    std::vector<Annotation*> objects;objects.reserve(edit.rows.size());
    for(const auto& row:edit.rows) {auto found=objects_.find(row.id);if(found==objects_.end()) throw std::out_of_range("classification object does not exist");objects.push_back(&found->second);}
#ifdef QUPATHXX_HAS_PROJECT
    struct Replacement {std::shared_ptr<const ObjectBlock>* slot;std::shared_ptr<const ObjectBlock> next;};
    std::vector<Replacement> replacements;replacements.reserve((edit.rows.size()+255)/256+1);
    auto object_bytes=object_bytes_;std::size_t property_cursor=0;
    for(std::size_t first=0;first<edit.rows.size();) {
        const auto key=(edit.rows[first].id-1)/256;auto last=first+1;
        while(last<edit.rows.size() && (edit.rows[last].id-1)/256==key) ++last;
        auto found=object_blocks_.find(key);if(found==object_blocks_.end()) throw std::logic_error("classification snapshot block is missing");
        auto block=std::make_shared<ObjectBlock>(*found->second);std::size_t at=0;
        for(auto row=first;row<last;++row) {
            const auto id=edit.rows[row].id;while(at<block->values.size() && block->values[at].id<id) ++at;
            if(at==block->values.size() || block->values[at].id!=id || block->values[at].class_name!=objects[row]->class_name) throw std::logic_error("classification snapshot block differs from live object");
            auto& object=block->values[at];block->bytes-=object_payload(object);
            object.class_name=edit.rows[row].alternate;object.revision=next;
            if(property_cursor<edit.property_rows.size() && edit.property_rows[property_cursor].row==row)object.properties=edit.property_rows[property_cursor++].alternate;
            if(object_payload(object)>std::numeric_limits<std::size_t>::max()-block->bytes) throw std::overflow_error("classification snapshot payload exhausted");
            block->bytes+=object_payload(object);++at;
        }
        if(found->second->bytes>object_bytes) throw std::logic_error("classification snapshot byte count is invalid");
        object_bytes-=found->second->bytes;
        if(block->bytes>std::numeric_limits<std::size_t>::max()-object_bytes) throw std::overflow_error("classification snapshot payload exhausted");
        object_bytes+=block->bytes;replacements.push_back({&found->second,std::move(block)});first=last;
    }
#endif
    // Commit uses only string/owner swaps, integral writes and map-node transfers.
    // Geometry, hierarchy, reader callbacks and allocations stay out of it.
    for(std::size_t i=0;i<objects.size();++i) {objects[i]->class_name.swap(edit.rows[i].alternate);objects[i]->revision=next;}
    for(auto& row:edit.property_rows)objects[row.row]->properties.swap(row.alternate);
    swap_nodes(class_counts_,edit.counts,edit.count_keys);swap_nodes(classes_,edit.classes,edit.class_keys);
    for(std::size_t i=0;i<edit.count_keys.size();++i) edit.expected_counts[i]=class_count(edit.count_keys[i]);
    for(std::size_t i=0;i<edit.class_keys.size();++i) edit.expected_classes[i]=bool(classes_.count(edit.class_keys[i]));
#ifdef QUPATHXX_HAS_PROJECT
    for(auto& replacement:replacements) replacement.slot->swap(replacement.next);
    object_bytes_=object_bytes;
#endif
    revision_=next;render_revision_=next;
}
}

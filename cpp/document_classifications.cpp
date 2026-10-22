#include "classification_edit.hpp"
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
Document::ClassificationHistory::ClassificationHistory(const ClassificationHistory& other):value(other.value?std::make_shared<detail::ClassificationEdit>(*other.value):nullptr) {}
Document::ClassificationHistory& Document::ClassificationHistory::operator=(const ClassificationHistory& other) {
    if(this!=&other) {ClassificationHistory copy(other);value.swap(copy.value);}return *this;
}
void Document::apply_classifications(const std::vector<std::pair<std::uint64_t,std::string>>& input,std::string label) {
    text(label);if(input.size()>1000000) throw std::length_error("classification selection exceeds one million objects");
    const auto less=[](const auto& a,const auto& b){return a.first<b.first;};
    std::vector<std::pair<std::uint64_t,std::string>> sorted;const auto* assignments=&input;
    if(!std::is_sorted(input.begin(),input.end(),less)) {sorted=input;std::sort(sorted.begin(),sorted.end(),less);assignments=&sorted;}
    auto edit=std::make_shared<detail::ClassificationEdit>();edit->rows.reserve(input.size());
    std::map<std::string,std::int64_t> delta;
    std::size_t bytes=sizeof(EditRecord)+sizeof(detail::ClassificationEdit)+label.size()+input.size()*sizeof(detail::ClassificationEdit::Row);
    auto charge=[&](std::size_t n) {if(n>std::numeric_limits<std::size_t>::max()-bytes) throw std::overflow_error("classification history payload exhausted");bytes+=n;};
    std::uint64_t previous=0;bool first=true;
    for(const auto& [id,name]:*assignments) {
        text(name);if(!first && previous==id) throw std::invalid_argument("duplicate classification object ID");first=false;previous=id;
        const auto& object=find(id);if(object.class_name==name) continue;
        edit->rows.push_back({id,name});charge(std::max(object.class_name.capacity(),edit->rows.back().alternate.capacity()));
        if(!object.class_name.empty()) --delta[object.class_name];
        if(!name.empty()) ++delta[name];
        if(!name.empty() && !classes_.count(name)) edit->classes.emplace(name,ObjectClass{name});
    }
    if(edit->rows.empty()) return;
    edit->count_keys.reserve(delta.size());edit->expected_counts.reserve(delta.size());
    for(const auto& [name,change]:delta) {
        const auto count=class_count(name);std::size_t next=count;
        if(change<0) {const auto remove=static_cast<std::uint64_t>(-change);if(remove>count) throw std::logic_error("invalid classification count");next-=static_cast<std::size_t>(remove);}
        else {const auto add=static_cast<std::uint64_t>(change);if(add>std::numeric_limits<std::size_t>::max()-count) throw std::overflow_error("classification count exhausted");next+=static_cast<std::size_t>(add);}
        edit->count_keys.push_back(name);edit->expected_counts.push_back(count);if(next) edit->counts.emplace(name,next);
        charge(2*name.size()+2*sizeof(std::string)+sizeof(std::size_t)+96);
    }
    for(const auto& [name,value]:edit->classes) {edit->class_keys.push_back(name);edit->expected_classes.push_back(false);charge(3*name.size()+sizeof(ObjectClass)+sizeof(std::string)+96);}
    const bool recording=history_steps_ && history_budget_;
    if(recording && bytes>history_budget_) throw std::length_error("edit exceeds history payload budget; raise limits or explicitly disable recording");
    EditRecord record;record.label=std::move(label);record.bytes=bytes;record.classifications.value=edit;
    if(recording) undo_.reserve(undo_.size()+1);
    toggle_classifications(*edit);
    for(const auto& entry:redo_) history_bytes_-=entry.bytes;
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
    auto object_bytes=object_bytes_;
    for(std::size_t first=0;first<edit.rows.size();) {
        const auto key=(edit.rows[first].id-1)/256;auto last=first+1;
        while(last<edit.rows.size() && (edit.rows[last].id-1)/256==key) ++last;
        auto found=object_blocks_.find(key);if(found==object_blocks_.end()) throw std::logic_error("classification snapshot block is missing");
        auto block=std::make_shared<ObjectBlock>(*found->second);std::size_t at=0;
        for(auto row=first;row<last;++row) {
            const auto id=edit.rows[row].id;while(at<block->values.size() && block->values[at].id<id) ++at;
            if(at==block->values.size() || block->values[at].id!=id || block->values[at].class_name!=objects[row]->class_name) throw std::logic_error("classification snapshot block differs from live object");
            auto& object=block->values[at];block->bytes-=object.class_name.size();
            object.class_name=edit.rows[row].alternate;object.revision=next;
            if(object.class_name.size()>std::numeric_limits<std::size_t>::max()-block->bytes) throw std::overflow_error("classification snapshot payload exhausted");
            block->bytes+=object.class_name.size();++at;
        }
        if(found->second->bytes>object_bytes) throw std::logic_error("classification snapshot byte count is invalid");
        object_bytes-=found->second->bytes;
        if(block->bytes>std::numeric_limits<std::size_t>::max()-object_bytes) throw std::overflow_error("classification snapshot payload exhausted");
        object_bytes+=block->bytes;replacements.push_back({&found->second,std::move(block)});first=last;
    }
#endif
    // Commit uses only string swaps, integral writes and map-node transfers.
    // Geometry, hierarchy, reader callbacks and allocations stay out of it.
    for(std::size_t i=0;i<objects.size();++i) {objects[i]->class_name.swap(edit.rows[i].alternate);objects[i]->revision=next;}
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

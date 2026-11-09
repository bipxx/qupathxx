#include "gui_marker_rules.hpp"
#include "gui_layout.hpp"
#include "qupathxx/marker_rule_io.hpp"
#include "qupathxx/raw_analysis.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <future>
namespace qpx::detail {
namespace {
bool edit_text(const char* label,std::string& value){
    std::vector<char> text(std::max<std::size_t>(4096,value.size()+1024),0);
    std::copy(value.begin(),value.end(),text.begin());
    if(!ui::InputText(label,text.data(),text.size()))return false;
    value=text.data();return true;
}
const char* truth_name(MarkerTruth truth){return truth==MarkerTruth::Positive?"Positive":truth==MarkerTruth::Negative?"Negative":"Unknown";}
void counts(const char* title,const MarkerCounts& value){ui::Text("%s: %zu Positive / %zu Negative / %zu Unknown",title,value.positive,value.negative,value.unknown);}
}
struct GuiMarkerRules::Impl {
    bool open=false,selected_only=false,discard=false,applied=false;
    MarkerRule rule;int gate=0;
    std::string file,saved,error,state="idle";
    std::array<Rect,7> bounds{};
    std::optional<MarkerRuleRequest> request;
    std::optional<RawAnalysisRequest> source;
    std::shared_ptr<const MarkerRuleResult> result;
    std::future<std::shared_ptr<const MarkerRuleResult>> future;
    std::atomic<bool> cancel{false};
    std::uint64_t revision=0,selected_id=0;
    Impl(){rule.gates.push_back({"Marker"});rule.nodes.push_back({MarkerOperation::Gate,0,{}});}
    ~Impl(){cancel.store(true);if(future.valid())future.wait();}
    bool busy() const{return future.valid();}
    void invalidate(const char* reason="idle"){
        cancel.store(true);discard=true;result.reset();request.reset();source.reset();applied=false;state=reason;
    }
    template<class F>void attempt(F f){try{f();error.clear();}catch(const std::exception& e){error=e.what();}}
    void mark(Action a){auto p=ImGui::GetItemRectMin(),q=ImGui::GetItemRectMax();bounds[std::size_t(a)]={p.x,p.y,q.x-p.x,q.y-p.y};}
    void poll(Document& doc){
        if(request){
            bool current=false;
            try{current=source && source->matches(doc) && (revision==doc.revision() || (applied?verify_marker_rule_publication(doc,*result):request->matches(doc)));}catch(const std::exception& e){error=e.what();}
            if(!current)invalidate("stale");else revision=doc.revision();
        }
        if(future.valid() && future.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){
            try{auto ready=future.get();if(!discard && request){result=std::move(ready);state="ready";error.clear();}}
            catch(const std::exception& e){if(!discard){error=e.what();state="failed";}}
            if(discard && state=="cancelling")state="cancelled";
        }
    }
    void preview(Document& doc,std::uint64_t selected){
        auto ids=selected_only?std::vector<std::uint64_t>{selected}:marker_cell_ids_for_domain(doc,rule.domain);
        if(selected_only && !selected)throw std::invalid_argument("Select a native fluorescence cell");
        MarkerRuleRequest candidate(doc,ids,rule);
        RawRegionRequest region;region.region={0,0,0,1,1};region.channels=1;
        RawAnalysisRequest guard(doc,region);
        invalidate();cancel.store(false);discard=false;selected_id=selected;
        request=std::move(candidate);source=std::move(guard);revision=doc.revision();
        const auto snapshot=*request;
        future=std::async(std::launch::async,[this,snapshot]{return preview_marker_rule(snapshot,&cancel);});state="running";
    }
    void combine(MarkerOperation op){
        rule.nodes.clear();for(std::size_t i=0;i<rule.gates.size();++i)rule.nodes.push_back({MarkerOperation::Gate,std::uint32_t(i),{}});
        if(rule.gates.size()>1){MarkerNode root;root.operation=op;for(std::size_t i=0;i<rule.gates.size();++i)root.inputs.push_back(std::uint32_t(i));rule.nodes.push_back(std::move(root));}
    }
    void editor(){
        bool changed=edit_text("Rule name",rule.name);
        changed=edit_text("Positive class",rule.positive_class)||changed;
        changed=edit_text("Negative class",rule.negative_class)||changed;
        changed=edit_text("Unknown class",rule.unknown_class)||changed;
        ui::TextWrapped("Domain: %s. Thresholds use original channel indices and source sample units.",rule.domain.c_str());
        if(ui::Button("Combine all gates with AND")){combine(MarkerOperation::And);changed=true;}
        if(ui::Button("Combine all gates with OR")){combine(MarkerOperation::Or);changed=true;}
        ui::TextWrapped("Loaded nested graphs are preserved. Add/remove explicitly rebuilds an all-gates AND rule.");
        ImGui::BeginDisabled(rule.gates.size()>=64);
        if(ui::Button("Add gate (all AND)")){rule.gates.push_back({"Marker "+std::to_string(rule.gates.size()+1)});gate=int(rule.gates.size()-1);combine(MarkerOperation::And);changed=true;}ImGui::EndDisabled();
        if(!rule.gates.empty()){
            ui::InputInt("Gate index",&gate);gate=std::clamp(gate,0,int(rule.gates.size()-1));auto& g=rule.gates[std::size_t(gate)];
            changed=edit_text("Gate name",g.name)||changed;
            changed=ui::InputScalar("Original channel",ImGuiDataType_U32,&g.channel)||changed;
            int compartment=int(g.compartment),statistic=int(g.statistic),comparison=int(g.comparison);
            if(ui::Combo("Compartment",&compartment,"Nucleus\0Cell\0Cytoplasm\0")){g.compartment=MarkerCompartment(compartment);changed=true;}
            if(ui::Combo("Statistic",&statistic,"Mean\0Sum\0Minimum\0Maximum\0Sample SD\0")){g.statistic=MarkerStatistic(statistic);g.units=statistic==1?MarkerUnits::SourceSampleSum:MarkerUnits::SourceSample;if(statistic==4)g.minimum_finite_samples=std::max<std::uint64_t>(2,g.minimum_finite_samples);changed=true;}
            if(ui::Combo("Comparison",&comparison,"Greater\0Greater or equal\0Less\0Less or equal\0")){g.comparison=MarkerComparison(comparison);changed=true;}
            changed=ui::InputDouble("Threshold",&g.threshold,0,0,"%.17g")||changed;
            changed=ui::InputScalar("Minimum finite samples",ImGuiDataType_U64,&g.minimum_finite_samples)||changed;
            ui::TextUnformatted(g.units==MarkerUnits::SourceSampleSum?"Units: source-sample-sum":"Units: source-sample");
            ImGui::BeginDisabled(rule.gates.size()==1);if(ui::Button("Remove gate (all AND)")){rule.gates.erase(rule.gates.begin()+gate);gate=0;combine(MarkerOperation::And);changed=true;}ImGui::EndDisabled();
        }
        if(ui::CollapsingHeader("Rule graph")){for(std::size_t i=0;i<rule.nodes.size();++i){const auto& n=rule.nodes[i];if(n.operation==MarkerOperation::Gate)ui::Text("Node %zu: gate %u",i,n.gate);else{ui::Text("Node %zu: %s",i,n.operation==MarkerOperation::And?"AND":"OR");for(auto input:n.inputs){ImGui::SameLine();ui::Text("%u",input);}}}}
        if(changed)invalidate();
    }
    void draw(Document& doc,std::uint64_t selected,bool blocked){
        poll(doc);bounds={};if(!open)return;
        if(selected_only && request && selected!=selected_id)invalidate("stale");
        ui::SetNextToolWindow("Marker rules",{640,800});
        if(ImGui::Begin("Marker rules",&open)){
            ui::Text("Marker state: %s",state.c_str());
            ImGui::BeginDisabled(busy() || blocked);
            if(ui::Button("Preview markers"))attempt([&]{preview(doc,selected);});mark(Action::Preview);
            ImGui::BeginDisabled(!result || applied);
            if(ui::Button("Apply marker classes"))attempt([&]{apply_marker_rule(doc,*request,*result);applied=true;revision=doc.revision();state="applied";});mark(Action::Apply);ImGui::EndDisabled();
            if(ui::Button("Keep marker rule"))attempt([&]{saved=keep_marker_rule(doc,rule);});mark(Action::Keep);
            if(ui::Checkbox("Selected fluorescence cell only",&selected_only))invalidate();
            ImGui::EndDisabled();
            ImGui::BeginDisabled(!busy());if(ui::Button("Cancel marker preview")){invalidate("cancelling");}mark(Action::Cancel);ImGui::EndDisabled();
            if(result){counts("Phenotype",result->counts());for(std::size_t i=0;i<result->gate_counts().size();++i)counts(request->rule().gates[i].name.c_str(),result->gate_counts()[i]);
                ui::TextWrapped("Unknown means missing, nonfinite or insufficient saved observations; no image pixels are remeasured.");
                if(ui::CollapsingHeader("Assignments and cohorts")){
                    ImGuiListClipper clip;clip.Begin(int(result->rows().size()));while(clip.Step())for(int i=clip.DisplayStart;i<clip.DisplayEnd;++i){const auto& r=result->rows()[std::size_t(i)];const auto& recipe=request->rule();const auto& label=r.phenotype==MarkerTruth::Positive?recipe.positive_class:r.phenotype==MarkerTruth::Negative?recipe.negative_class:recipe.unknown_class;
                        ui::Text("%llu: %s / %s",(unsigned long long)r.object_id,truth_name(r.phenotype),label.c_str());}
                }
            }
            if(!error.empty())ui::TextWrapped("Marker rules: %s",error.c_str());
            ImGui::BeginDisabled(busy() || blocked);
            if(ui::CollapsingHeader("Load and retain rules",ImGuiTreeNodeFlags_DefaultOpen)){
                edit_text("Rule JSON path",file);if(ui::Button("Load rule JSON"))attempt([&]{auto loaded=load_marker_rule_file(file);invalidate();rule=std::move(loaded);gate=0;});mark(Action::LoadFile);
                try{auto ids=marker_rule_ids(doc);if(ui::BeginCombo("Saved marker rule",saved.empty()?"None":saved.c_str())){for(const auto& id:ids)if(ImGui::Selectable(id.c_str(),id==saved))saved=id;ui::EndCombo();}}catch(const std::exception& e){error=e.what();}
                ImGui::BeginDisabled(saved.empty());if(ui::Button("Load saved marker rule"))attempt([&]{auto loaded=load_marker_rule(doc,saved);invalidate();rule=std::move(loaded);gate=0;});mark(Action::LoadSaved);
                if(ui::Button("Remove saved marker rule"))attempt([&]{remove_marker_rule(doc,saved);saved.clear();});mark(Action::Remove);ImGui::EndDisabled();
            }
            if(ui::CollapsingHeader("Edit marker rule"))editor();
            ImGui::EndDisabled();
        }ImGui::End();
    }
};
GuiMarkerRules::GuiMarkerRules():impl_(std::make_unique<Impl>()){}
GuiMarkerRules::~GuiMarkerRules()=default;
void GuiMarkerRules::open(){impl_->open=true;}
void GuiMarkerRules::poll(Document& doc){impl_->poll(doc);}
void GuiMarkerRules::draw(Document& doc,std::uint64_t selected,bool blocked){impl_->draw(doc,selected,blocked);}
void GuiMarkerRules::set_rule(MarkerRule rule){rule.validate();impl_->invalidate();impl_->rule=std::move(rule);impl_->gate=0;}
void GuiMarkerRules::set_file(std::string file){impl_->file=std::move(file);}
void GuiMarkerRules::set_selected_only(bool value){impl_->invalidate();impl_->selected_only=value;}
Rect GuiMarkerRules::bounds(Action a)const{return impl_->bounds[std::size_t(a)];}
bool GuiMarkerRules::busy()const{return impl_->busy();}
const std::string& GuiMarkerRules::error()const{return impl_->error;}
const std::string& GuiMarkerRules::state()const{return impl_->state;}
std::shared_ptr<const MarkerRuleResult> GuiMarkerRules::result()const{return impl_->result;}
}

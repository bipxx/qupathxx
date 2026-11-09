#pragma once
#include "qupathxx/spatial_import.hpp"
#include "qupathxx/gene_view.hpp"
#include "qupathxx/project.hpp"
#include "gui_layout.hpp"
#include <optional>
#include <string_view>
namespace qpx::detail {
// Matrix IO/catalogue work uses the existing import worker; value/color work uses
// GeneViewSession. Both workers receive immutable native inputs, never Document.
class GuiBackedGenes {
    SpatialImportJob job_;
    std::optional<ExpressionMatrixOpenRequest> pending_open_;
    std::optional<ExpressionMatrixSpatialScope> scope_;
    std::string wanted_,cached_,running_,running_query_,error_;
    std::shared_ptr<const ExpressionMatrixSource> matrix_;
    std::weak_ptr<const ExpressionMatrixSource> seen_matrix_;
    std::weak_ptr<const ExpressionFeaturePage> seen_page_;
    std::array<std::shared_ptr<const ExpressionFeaturePage>,2> pages_;
    std::array<std::array<char,257>,2> queries_{};
    std::array<std::size_t,2> page_{};
    std::array<bool,2> pending_page_{};
    std::array<Rect,2> next_bounds_{};
    std::array<std::vector<std::pair<std::uint64_t,Rect>>,2> feature_bounds_;
    unsigned slot_=0;bool opening_=false;
    static std::string key(const ExpressionMatrixOpenRequest& r){return r.path+"\n"+r.association.file_sha256+"\n"+r.association.source_sha256;}
    void capture(const Document& document,GeneViewSession& session) {
        session.bind(ExpressionMatrixSpatialSnapshot::capture(document,matrix_,scope_->frame_id,scope_->plane));
    }
    void dispatch() {
        if(job_.busy())return;
        if(pending_open_) {
            running_=wanted_;opening_=true;auto request=std::move(*pending_open_);pending_open_.reset();job_.open_matrix(std::move(request));return;
        }
        if(!scope_ || !matrix_ || cached_!=wanted_)return;
        for(unsigned i=0;i<2;++i)if(pending_page_[i]) {
            const auto other=1u-i;
            if(pages_[other] && std::string_view(queries_[i].data())==queries_[other].data() && page_[i]==page_[other]){pages_[i]=pages_[other];pending_page_[i]=false;continue;}
            pending_page_[i]=false;slot_=i;running_=wanted_;running_query_=queries_[i].data();opening_=false;
            job_.matrix_catalogue(matrix_,running_query_,page_[i]*16,16);return;
        }
    }
public:
    void clear() {scope_.reset();wanted_.clear();pending_open_.reset();pending_page_={};pages_={};job_.cancel();error_.clear();}
    void refresh(){clear();matrix_.reset();cached_.clear();}
    void cancel(){pending_open_.reset();pending_page_={};job_.cancel();}
    bool busy()const{return job_.busy() || pending_open_ || pending_page_[0] || pending_page_[1];}
    const std::string& error()const{return error_;}
    bool ready()const{return scope_ && matrix_ && cached_==wanted_;}
    std::shared_ptr<const ExpressionFeaturePage> catalogue(unsigned index)const{return pages_.at(index);}
    Rect next_bounds(unsigned index)const{return next_bounds_.at(index);}
    Rect feature_bounds(unsigned index,std::uint64_t feature)const{for(const auto& item:feature_bounds_.at(index))if(item.first==feature)return item.second;return {};}
    void bind(const Project& project,std::uint64_t image,const ExpressionMatrixSpatialScope& scope,const Document& document,GeneViewSession& session) {
        auto request=expression_matrix_open_request(project,image,scope.matrix_source_sha256);
        const auto next=key(request);scope_=scope;error_.clear();
        if(wanted_!=next){job_.cancel();pages_={};page_={};pending_page_={true,true};}
        wanted_=next;
        if(matrix_ && cached_==wanted_){capture(document,session);for(unsigned i=0;i<2;++i)if(!pages_[i])pending_page_[i]=true;}
        else {session.clear();if(!(job_.busy() && opening_ && running_==wanted_))pending_open_=std::move(request);}
        dispatch();
    }
    void poll(const Document& document,GeneViewSession& session) {
        job_.poll();
        if(const auto value=job_.matrix_source();value && value!=seen_matrix_.lock()) {
            seen_matrix_=value;
            if(opening_ && running_==wanted_ && scope_) {
                matrix_=value;cached_=wanted_;error_.clear();
                try{capture(document,session);pending_page_={true,true};}catch(const std::exception& e){session.clear();error_=e.what();pending_page_={};}
            }
        }
        if(const auto value=job_.matrix_catalogue_result();value && value!=seen_page_.lock()) {
            seen_page_=value;
            if(!opening_ && running_==wanted_ && running_query_==queries_[slot_].data() && value->offset==page_[slot_]*16){pages_[slot_]=value;pending_page_[slot_]=false;error_.clear();}
        }
        if(!job_.busy() && job_.phase()==SpatialImportPhase::Failed && running_==wanted_)error_=job_.error();
        dispatch();
    }
    void choose(unsigned index,GeneViewOptions& options,bool& changed) {
        feature_bounds_[index].clear();next_bounds_[index]={};
        ImGui::PushID(int(index));ui::TextWrapped("%s: %s",index?"Gene B":"Gene A",options.genes[index].c_str());
        if(ui::InputTextWithHint("Search IDs / symbols","e.g. Gad1 or ENSMUSG",queries_[index].data(),queries_[index].size())) {
            page_[index]=0;pages_[index].reset();pending_page_[index]=true;
            if(job_.busy() && !opening_){pending_page_[slot_]=true;job_.cancel();}
        }
        if(const auto page=pages_[index]) {
            if(ImGui::BeginListBox("##gene-list",{ui::available(),std::min(ui::CollectionHeight(page->features.size()),ImGui::GetTextLineHeightWithSpacing()*6)})) {
                for(const auto& feature:page->features) {
                    ImGui::PushID(int(feature.index));
                    const auto label=feature.name==feature.id?feature.id:feature.name+" | "+feature.id;
                    const auto position=ImGui::GetCursorScreenPos();
                    if(ImGui::Selectable("##gene",options.genes[index]==feature.id)){options.genes[index]=feature.id;changed=true;}
                    const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();feature_bounds_[index].push_back({feature.index,{lo.x,lo.y,hi.x-lo.x,hi.y-lo.y}});
                    ImGui::SetItemTooltip("%s",feature.id.c_str());ImGui::PopID();
                    ImGui::GetWindowDrawList()->AddText(position,ImGui::GetColorU32(ImGuiCol_Text),label.c_str());
                }
                ImGui::EndListBox();
            }
            ImGui::BeginDisabled(page_[index]==0);if(ui::Button("Previous")){--page_[index];pages_[index].reset();pending_page_[index]=true;}ImGui::EndDisabled();ImGui::SameLine();
            ImGui::BeginDisabled((page_[index]+1)*16>=page->matches);if(ui::Button("Next")){++page_[index];pages_[index].reset();pending_page_[index]=true;}
            const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();next_bounds_[index]={lo.x,lo.y,hi.x-lo.x,hi.y-lo.y};ImGui::EndDisabled();
            ui::TextDisabled("%llu matching indexed features",static_cast<unsigned long long>(page->matches));
        } else ui::TextDisabled("Reading feature page...");
        ImGui::PopID();dispatch();
    }
};
}

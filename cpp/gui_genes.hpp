#pragma once
#include "qupathxx/gene_view.hpp"
#include "qupathxx/landmark_registration.hpp"
#include "qupathxx/core.hpp"
#include "gui_layout.hpp"
#include "gui_tiles.hpp"
#include "gui_backed_genes.hpp"
#include <cstring>
#include <limits>
#include <cmath>

namespace qpx::detail {
class GuiGeneDisplay {
    GeneViewSession session_;
    GuiBackedGenes backed_;
    std::map<std::string,ExpressionMatrixSpatialScope> backed_sources_;
    std::string project_path_;std::uint64_t project_image_=0;
    std::optional<RegistrationField> registered_;
    bool open_=false;
    const Document* document_=nullptr;
    std::shared_ptr<ImageServer> server_;
    std::uint64_t revision_=std::numeric_limits<std::uint64_t>::max();
    ObjectPlane plane_;
    std::vector<std::string> sources_;
    std::string source_,error_;
    std::array<std::array<char,256>,2> query_{};
    std::array<std::size_t,2> page_{};
    std::unique_ptr<GuiTiles> comparison_tiles_;
    Viewport comparison_view_;
    ImVec2 comparison_size_{};
    static ImU32 color(const std::array<std::uint8_t,4>& c) {return IM_COL32(c[0],c[1],c[2],c[3]);}
    void choose(unsigned index,GeneViewOptions& options,bool& changed) {
        if(backed_sources_.count(source_)){backed_.choose(index,options,changed);return;}
        ImGui::PushID(int(index));
        ui::TextWrapped("%s: %s",index?"Gene B":"Gene A",options.genes[index].c_str());
        if(ui::InputTextWithHint("Search IDs / symbols","e.g. Gad1 or ENSMUSG",query_[index].data(),query_[index].size()))page_[index]=0;
        const auto& dataset=*session_.dataset();const auto page=gene_feature_page(dataset,query_[index].data(),page_[index],16);
        if(ImGui::BeginListBox("##gene-list",{ui::available(),std::min(ui::CollectionHeight(page.indices.size()),ImGui::GetTextLineHeightWithSpacing()*6)})) {
            for(auto row:page.indices) {
                ImGui::PushID(int(row));const auto position=ImGui::GetCursorScreenPos();
                if(ImGui::Selectable("##gene",options.genes[index]==dataset.features()[row])) {options.genes[index]=dataset.features()[row];changed=true;}
                ImGui::SetItemTooltip("%s",dataset.features()[row].c_str());
                ImGui::GetWindowDrawList()->AddText(position,ImGui::GetColorU32(ImGuiCol_Text),dataset.features()[row].c_str());ImGui::PopID();
            }
            ImGui::EndListBox();
        }
        ImGui::BeginDisabled(page_[index]==0);if(ui::Button("Previous"))--page_[index];ImGui::EndDisabled();ImGui::SameLine();
        ImGui::BeginDisabled(page_[index]>=page.matches/16 || (page_[index]+1)*16>=page.matches);if(ui::Button("Next"))++page_[index];ImGui::EndDisabled();
        ui::TextDisabled("%zu matching imported features",page.matches);ImGui::PopID();
    }
    void legend(const GeneDisplayField& field) const {
        ui::TextWrapped("%s",field.feature.c_str());
        const char* units=field.legend.domain==ExpressionValueDomain::Counts?"counts":"continuous expression";
        ui::TextWrapped("%.6g to %.6g %s%s%s",field.legend.lower,field.legend.upper,
            field.legend.transform==GeneDisplayTransform::Log1p?"log1p(":"",units,field.legend.transform==GeneDisplayTransform::Log1p?")":"");
        const auto origin=ImGui::GetCursorScreenPos();const float width=ui::available(),height=10*ui::design::scale();
        auto* draw=ImGui::GetWindowDrawList();auto opaque=session_.options().display;opaque.opacity=1;
        for(unsigned i=0;i<32;++i)draw->AddRectFilled({origin.x+width*i/32,origin.y},{origin.x+width*(i+1)/32,origin.y+height},color(gene_display_palette_color(i/31.,opaque)));
        ImGui::Dummy({width,height});
        ImGui::ColorButton("##zero",ImGui::ColorConvertU32ToFloat4(color(opaque.zero_color)),ImGuiColorEditFlags_NoTooltip,{12,12});ImGui::SameLine();ui::Text("Zero: %zu",field.legend.zeros);
        ImGui::ColorButton("##missing",ImGui::ColorConvertU32ToFloat4(color(opaque.missing_color)),ImGuiColorEditFlags_NoTooltip,{12,12});ImGui::SameLine();ui::Text("Missing / unimported: %zu",field.legend.missing);
        ui::TextDisabled("Clipped low / high: %zu / %zu",field.legend.clipped_lower,field.legend.clipped_upper);
    }
    void points(ImDrawList* draw,const GeneDisplayField& field,const Viewport& view,ImVec2 offset,ImVec2 size,bool hovered) const {
        const auto radius=float(session_.options().radius*ui::design::scale());
        std::optional<std::size_t> hit;double closest=double(radius+3)*(radius+3);
        const auto mouse=ImGui::GetIO().MousePos;
        for(std::size_t i=0;i<field.rows.size();++i) {
            const auto row=field.rows[i];const auto position=registered_ && registered_->dataset.sha256()==field.dataset.sha256()?registered_->positions[row]:field.dataset.observations()[row].position;
            const auto point=view.image_to_screen(position);
            if(!std::isfinite(point.x) || !std::isfinite(point.y) || point.x< -radius || point.y< -radius || point.x>size.x+radius || point.y>size.y+radius)continue;
            const ImVec2 p{offset.x+float(point.x),offset.y+float(point.y)};draw->AddCircleFilled(p,radius,color(field.colors[i]));
            if(hovered) {const double dx=mouse.x-p.x,dy=mouse.y-p.y,distance=dx*dx+dy*dy;if(distance<closest) {closest=distance;hit=i;}}
        }
        if(hit) {
            const auto row=field.rows[*hit];ImGui::BeginTooltip();ui::TextWrapped("%s",field.dataset.observations()[row].id.c_str());ui::TextWrapped("%s",field.feature.c_str());
            if(!field.valid[*hit])ui::TextUnformatted("Missing / not imported");
            else {const auto at=row*field.dataset.features().size()+field.dataset.feature_index(field.feature);
                if(field.dataset.domain()==ExpressionValueDomain::Counts)ui::Text("Raw count: %llu",static_cast<unsigned long long>(field.dataset.counts()[at]));
                else ui::Text("Source value: %.12g",field.dataset.continuous()[at]);}
            ImGui::EndTooltip();
        }
    }
    void comparisons(const GeneDisplayPair& pair,const DisplaySettings& display) {
        if(!comparison_tiles_)comparison_tiles_=std::make_unique<GuiTiles>(server_);
        comparison_tiles_->set_display(display);ui::SetNextToolWindow("Gene comparison",{1000,650});
        bool show=true;if(ImGui::Begin("Gene comparison",&show)) {
            ui::TextWrapped("%s",pair.shared_range?"Shared scale; synchronized navigation":"Independent scales; synchronized navigation");
            const auto full=ImGui::GetContentRegionAvail();const float width=std::max(1.f,(full.x-ImGui::GetStyle().ItemSpacing.x)/2),height=std::max(100.f,full.y-105*ui::design::scale());
            const ImVec2 size{std::max(1.f,width-2*ImGui::GetStyle().WindowPadding.x-2*ImGui::GetStyle().ChildBorderSize),height};
            if(!comparison_size_.x)comparison_view_.fit(server_->metadata().width,server_->metadata().height,size.x,size.y);
            else if(size.x!=comparison_size_.x || size.y!=comparison_size_.y)comparison_view_.center_on(comparison_view_.screen_to_image({comparison_size_.x/2,comparison_size_.y/2}),size.x,size.y);
            comparison_size_=size;
            // Collect both canvas inputs before drawing either, so both halves
            // use the same updated camera in this frame.
            std::array<ImVec2,2> offsets;std::array<bool,2> hovered{};std::array<ImDrawList*,2> draws{};
            for(unsigned i=0;i<2;++i) {
                if(i)ImGui::SameLine();ImGui::PushID(int(i));ImGui::BeginChild("compare-pane",{width,0},ImGuiChildFlags_Borders);
                ImGui::TextUnformatted(pair.fields[i]->feature.c_str());ImGui::SetItemTooltip("%s",pair.fields[i]->feature.c_str());offsets[i]=ImGui::GetCursorScreenPos();
                ImGui::InvisibleButton("canvas",size,ImGuiButtonFlags_MouseButtonLeft);hovered[i]=ImGui::IsItemHovered();draws[i]=ImGui::GetWindowDrawList();
                if(hovered[i] && ImGui::GetIO().MouseWheel)comparison_view_.zoom_at({ImGui::GetIO().MousePos.x-offsets[i].x,ImGui::GetIO().MousePos.y-offsets[i].y},std::pow(1.2,ImGui::GetIO().MouseWheel));
                if(ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {comparison_view_.origin.x+=ImGui::GetIO().MouseDelta.x;comparison_view_.origin.y+=ImGui::GetIO().MouseDelta.y;}
                const auto& field=*pair.fields[i];
                ui::TextWrapped("Range: %.6g to %.6g %s",field.legend.lower,field.legend.upper,
                    field.legend.transform==GeneDisplayTransform::Log1p?(field.legend.domain==ExpressionValueDomain::Counts?"log1p(counts)":"log1p(expression)"):
                    (field.legend.domain==ExpressionValueDomain::Counts?"counts":"expression"));
                ui::Text("Zero / missing: %zu / %zu",field.legend.zeros,field.legend.missing);
                ImGui::EndChild();ImGui::PopID();
            }
            const auto plan=plan_view_tiles(server_->metadata(),comparison_view_,size.x,size.y,1,256,128);comparison_tiles_->update(plan);
            for(unsigned i=0;i<2;++i) {
                auto* draw=draws[i];draw->PushClipRect(offsets[i],{offsets[i].x+size.x,offsets[i].y+size.y},true);
                draw->AddRectFilled(offsets[i],{offsets[i].x+size.x,offsets[i].y+size.y},IM_COL32(18,21,27,255));
                for(const auto& tile:plan.tiles)if(auto texture=comparison_tiles_->texture(tile)) {
                    const auto a=comparison_view_.image_to_screen({tile.bounds.x,tile.bounds.y});const auto b=comparison_view_.image_to_screen({tile.bounds.x+tile.bounds.width,tile.bounds.y+tile.bounds.height});
                    draw->AddImage(static_cast<ImTextureID>(texture),{offsets[i].x+float(a.x),offsets[i].y+float(a.y)},{offsets[i].x+float(b.x),offsets[i].y+float(b.y)});
                }
                points(draw,*pair.fields[i],comparison_view_,offsets[i],size,hovered[i]);draw->PopClipRect();
            }
            if(!comparison_tiles_->error().empty())ui::TextWrapped("%s",comparison_tiles_->error().c_str());
        }ImGui::End();if(!show) {auto options=session_.options();options.compare=false;session_.configure(std::move(options));}
    }
public:
    void open(){open_=true;}
    void load_preference(const std::string& preference) {
        if(preference.empty())return;
        try {const auto restored=parse_gene_workspace_preference(preference);session_.configure(restored.view);source_=restored.source_id;open_=session_.options().enabled;}catch(const std::exception& e){error_=e.what();}
    }
    std::string preference() const {return gene_workspace_preference(session_.options(),source_);}
    const GeneViewSession& session() const {return session_;}
    std::shared_ptr<const ExpressionFeaturePage> backed_catalogue(unsigned index)const{return backed_.catalogue(index);}
    Rect catalogue_next_bounds(unsigned index)const{return backed_.next_bounds(index);}
    Rect catalogue_feature_bounds(unsigned index,std::uint64_t feature)const{return backed_.feature_bounds(index,feature);}
    bool settled() const {return !session_.busy() && !backed_.busy() && (!comparison_tiles_ || !comparison_tiles_->pending());}
    void update(Document& document,bool has_image,const Project* project=nullptr,std::uint64_t image=0) {
        session_.poll();if(!has_image) {if(document_)session_.clear();backed_.clear();backed_.poll(document,session_);registered_.reset();document_=nullptr;server_.reset();comparison_tiles_.reset();return;}
        if(!open_ && !session_.options().enabled){backed_.poll(document,session_);return;}
        static const std::string empty_path;
        const auto& project_path=project?project->path():empty_path;
        if(document_==&document && server_==document.server() && revision_==document.revision() && plane_==document.active_plane() && project_path_==project_path && project_image_==image){backed_.poll(document,session_);return;}
        const bool new_image=document_!=&document || server_!=document.server();
        if(new_image) {comparison_tiles_.reset();comparison_size_={};page_={};}
        document_=&document;server_=document.server();revision_=document.revision();plane_=document.active_plane();project_path_=project_path;project_image_=image;
        registered_.reset();try {
            sources_=ExpressionDataset::sources(document,plane_);
            backed_sources_.clear();
            for(auto scope:expression_matrix_spatial_scopes(document,plane_)){const auto id=scope.sha256();sources_.push_back(id);backed_sources_.emplace(id,std::move(scope));}
            if(std::find(sources_.begin(),sources_.end(),source_)==sources_.end())source_.clear();
            if(source_.empty() && sources_.size()==1)source_=sources_[0];
            if(source_.empty()) {session_.clear();backed_.clear();error_=sources_.empty()?"No imported spatial expression points on this plane.":"Choose an expression source.";backed_.poll(document,session_);return;}
            if(const auto scope=backed_sources_.find(source_);scope!=backed_sources_.end()) {
                if(!project || !image)throw std::invalid_argument("Open the containing project to read indexed expression");
                backed_.bind(*project,image,scope->second,document,session_);backed_.poll(document,session_);error_.clear();return;
            }
            backed_.clear();
            ExpressionCaptureOptions options;options.coordinate_frame="image-level-zero-px";options.plane=plane_;options.source_id=source_;
            auto dataset=ExpressionDataset::capture(document,options);registered_=active_registration_field(document,dataset);
            session_.bind(std::move(dataset));session_.poll();error_.clear();
        }catch(const std::exception& e){session_.clear();backed_.clear();error_=e.what();}
        backed_.poll(document,session_);
    }
    void draw_overlay(ImDrawList* draw,const Viewport& view,ImVec2 offset,ImVec2 size) const {
        const auto pair=session_.result();if(!session_.options().enabled || !pair)return;
        points(draw,*pair->fields[0],view,offset,size,ImGui::IsWindowHovered());
    }
    void draw(const DisplaySettings& display) {
        if(open_) {
            ui::SetNextToolWindow("Gene expression",{440,730});if(ImGui::Begin("Gene expression",&open_)) {
                ui::TextWrapped("Spot centers; glyph size is display only. Source counts are unchanged.");
                if(registered_)ui::TextWrapped("Accepted registration places expression glyphs. Source object geometry is unchanged.");
                const auto source_label=[&](const std::string& id){const auto at=backed_sources_.find(id);return at==backed_sources_.end()?id:"Indexed map "+std::to_string(std::distance(backed_sources_.begin(),at)+1)+" ("+std::to_string(at->second.observations)+" spots)";};
                if(sources_.size()>1 && ui::BeginCombo("Expression source",source_.empty()?"Choose source":source_label(source_).c_str())) {
                    for(const auto& source:sources_)if(ImGui::Selectable(source_label(source).c_str(),source_==source)) {source_=source;revision_=std::numeric_limits<std::uint64_t>::max();session_.clear();backed_.clear();}
                    ImGui::EndCombo();
                }
                auto options=session_.options();bool changed=ui::Checkbox("Show expression overlay",&options.enabled);
                if(session_.dataset() || backed_.ready()) {
                    choose(0,options,changed);changed=ui::Checkbox("Compare two genes",&options.compare)||changed;
                    if(options.compare) {choose(1,options,changed);changed=ui::Checkbox("Shared range",&options.shared_range)||changed;}
                    int transform=int(options.display.transform),range=int(options.display.range),palette=int(options.display.palette);
                    if(ui::Combo("Display transform",&transform,"Linear\0Log1p\0")) {options.display.transform=static_cast<GeneDisplayTransform>(transform);changed=true;}
                    if(ui::Combo("Display range",&range,"Min / max\0Percentile\0Manual\0")) {options.display.range=static_cast<GeneDisplayRange>(range);changed=true;}
                    if(range==int(GeneDisplayRange::Percentile)) {changed=ui::InputDouble("Lower percentile",&options.display.lower_percentile)||changed;changed=ui::InputDouble("Upper percentile",&options.display.upper_percentile)||changed;}
                    if(range==int(GeneDisplayRange::Manual)) {changed=ui::InputDouble("Lower display value",&options.display.lower)||changed;changed=ui::InputDouble("Upper display value",&options.display.upper)||changed;}
                    if(ui::Combo("Palette",&palette,"Blue to yellow\0Gray\0")) {options.display.palette=static_cast<GeneDisplayPalette>(palette);changed=true;}
                    changed=ui::InputDouble("Opacity",&options.display.opacity)||changed;changed=ui::InputDouble("Glyph radius (screen px)",&options.radius)||changed;
                }
                if(changed)try {session_.configure(std::move(options));if(session_.dataset())error_.clear();}catch(const std::exception& e){error_=e.what();}
                if(ui::Button("Refresh expression")) {revision_=std::numeric_limits<std::uint64_t>::max();backed_.refresh();session_.refresh();}
                if(session_.busy() || backed_.busy()) {ImGui::SameLine();if(ui::Button("Cancel display")){session_.cancel();backed_.cancel();}ui::TextDisabled("Updating expression...");}
                if(!error_.empty())ui::TextWrapped("%s",error_.c_str());if(!session_.error().empty())ui::TextWrapped("%s",session_.error().c_str());
                if(!backed_.error().empty())ui::TextWrapped("%s",backed_.error().c_str());
                if(const auto pair=session_.result())for(unsigned i=0;i<(session_.options().compare?2u:1u);++i) {ImGui::PushID(int(i));legend(*pair->fields[i]);ImGui::PopID();}
            }ImGui::End();
        }
        if(session_.options().enabled && session_.options().compare)if(const auto pair=session_.result())comparisons(*pair,display);
    }
};
}

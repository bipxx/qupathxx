#include "gui_tool_catalog.hpp"
#include "gui_registration.hpp"
#include "gui_layout.hpp"
#include "gui_tiles.hpp"
#include "gui_tool_scope.hpp"
#include "gui_workflow_ui.hpp"
#include "qupathxx/core.hpp"
#include <cmath>
#include <limits>

namespace qpx::detail {
struct GuiLandmarkRegistration::Impl {
    bool open=false,accepted=false;
    ToolScope scope;
    std::shared_ptr<ImageServer> server;
    bool sources_dirty=true,current=false;
    std::size_t source_refreshes=0;
    ObjectPlane plane;
    std::string image_id,source,error;
    std::vector<std::string> sources;
    std::optional<ExpressionDataset> dataset;
    std::vector<LandmarkPair> pairs;
    struct PlaneDraft {ObjectPlane plane;std::vector<LandmarkPair> pairs;std::string source;std::size_t selected;
        std::shared_ptr<const LandmarkFit> fit;std::optional<RegistrationRequest> request;std::optional<RegistrationField> preview;bool accepted;};
    std::vector<PlaneDraft> drafts;
    LandmarkFitOptions options;
    std::shared_ptr<const LandmarkFit> fit;
    std::optional<RegistrationRequest> request;
    std::optional<RegistrationField> preview;
    std::unique_ptr<GuiTiles> tiles;
    Viewport view;ImVec2 canvas_size{},offset{};
    std::array<Rect,3> bounds{};
    std::size_t selected=0,next_id=1;
    int editing=0,dragging=-1;
    std::array<char,257> id{};
    Impl() {const std::string initial="landmarks";std::copy(initial.begin(),initial.end(),id.begin());}
    void invalidate() {fit.reset();request.reset();preview.reset();accepted=false;error.clear();}
    void mark(Action action) {const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();bounds[std::size_t(action)]={a.x,a.y,b.x-a.x,b.y-a.y};}
    template<class F> void attempt(F action) {try {action();error.clear();}catch(const std::exception& e){error=e.what();}}
    void update(Document& doc,const std::string& identity) {
        const auto change=detail::update(scope,doc,0);
        const bool changed=change.image || change.source;
        const bool new_plane=change.plane,new_server=server!=doc.server();
        if(changed) {drafts.clear();pairs.clear();selected=0;source.clear();canvas_size={};}
        else if(new_plane) {
            auto old=std::find_if(drafts.begin(),drafts.end(),[&](const auto& d){return d.plane==plane;});
            PlaneDraft draft{plane,pairs,source,selected,fit,request,preview,accepted};if(old!=drafts.end())drafts.erase(old);drafts.push_back(std::move(draft));
            const auto next=std::find_if(drafts.begin(),drafts.end(),[&](const auto& d){return d.plane==doc.active_plane();});
            pairs=next==drafts.end()?std::vector<LandmarkPair>{}:next->pairs;
            source=next==drafts.end()?std::string{}:next->source;selected=next==drafts.end()?0:next->selected;
            fit=next==drafts.end()?nullptr:next->fit;request=next==drafts.end()?std::nullopt:next->request;
            preview=next==drafts.end()?std::nullopt:next->preview;accepted=next!=drafts.end() && next->accepted;
            if(next!=drafts.end()){next->fit.reset();next->request.reset();next->preview.reset();next->accepted=false;}
            // Lightweight pairs remain for every plane; captures have a bounded lifetime.
            std::size_t recent=0;for(auto i=drafts.rbegin();i!=drafts.rend();++i)if(i->plane!=doc.active_plane() && ++recent>3){i->fit.reset();i->request.reset();i->preview.reset();i->accepted=false;}
        }
        // The server is a renderer input, never the owner of the landmark draft.
        if(changed || new_plane || new_server) {tiles.reset();dragging=-1;}
        server=doc.server();plane=scope.plane;const bool new_identity=image_id!=identity;image_id=identity;
        if(changed) {invalidate();dataset.reset();}
        if(!open){sources_dirty=true;return;}
        if(!changed && !new_plane && !change.document && !new_server && !new_identity && !sources_dirty)return;
        sources_dirty=false;
        ++source_refreshes;
        attempt([&] {
            sources=ExpressionDataset::sources(doc,plane);
            if(std::find(sources.begin(),sources.end(),source)==sources.end())source.clear();
            if(source.empty() && sources.size()==1)source=sources.front();
            if(source.empty())throw std::invalid_argument(sources.empty()?"No spatial expression source on this plane.":"Choose an expression source.");
            ExpressionCaptureOptions capture;capture.coordinate_frame="image-level-zero-px";capture.source_id=source;capture.plane=plane;
            dataset=ExpressionDataset::capture(doc,capture);
        });
        current=false;
        if(request)try {require_current_registration(doc,*request);current=true;}catch(const std::exception&) {}
        if(!error.empty())dataset.reset();
        accepted=false;
        if(current && fit)try {const auto active=active_registration_id(doc.image_data_properties());if(active && *active==id.data()){const auto kept=stored_landmark_fit(doc.image_data_properties(),*active);accepted=kept && kept->sha256()==fit->sha256();}}catch(const std::exception& e){error=e.what();}
    }
    void calculate(Document& doc) {
        invalidate();if(!dataset)throw std::invalid_argument("Select an expression source first");
        auto result=std::make_shared<const LandmarkFit>(fit_landmarks(pairs,options));
        CoordinateFrame from;from.id=dataset->coordinate_frame();from.source_id=dataset->source_id();from.plane=dataset->plane();
        RegistrationRequest candidate(*dataset,from,image_coordinate_frame(doc,image_id,plane),result->transform());
        auto field=registration_field(candidate);request=std::move(candidate);preview=std::move(field);fit=std::move(result);current=true;
    }
    void load(Document& doc) {
        invalidate();const auto stored=stored_landmark_fit(doc.image_data_properties(),id.data());
        if(!stored)throw std::invalid_argument("No saved landmark evidence with this name");
        const auto definition=stored_registration(doc.image_data_properties(),id.data());
        if(!dataset || !definition)throw std::invalid_argument("Select the saved expression source first");
        require_current_registration(*definition,*dataset,image_coordinate_frame(doc,image_id,plane));
        RegistrationRequest candidate(*dataset,definition->source,definition->target,definition->transform);
        auto field=registration_field(candidate);pairs=stored->landmarks();options=stored->options();selected=0;
        request=std::move(candidate);preview=std::move(field);fit=stored;
        current=true;
        accepted=active_registration_id(doc.image_data_properties())==std::optional<std::string>(id.data());
    }
    Point screen(Point p) const {const auto s=view.image_to_screen(p);return {s.x+offset.x,s.y+offset.y};}
    void canvas(const DisplaySettings& display) {
        const auto available=ImGui::GetContentRegionAvail();const ImVec2 size{std::max(1.f,available.x),std::max(150.f,available.y)};
        if(!canvas_size.x)view.fit(server->metadata().width,server->metadata().height,size.x,size.y);
        else if(canvas_size.x!=size.x || canvas_size.y!=size.y)view.center_on(view.screen_to_image({canvas_size.x/2,canvas_size.y/2}),size.x,size.y);
        canvas_size=size;offset=ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("registration-canvas",size,ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonRight);
        const auto& io=ImGui::GetIO();const bool hovered=ImGui::IsItemHovered();
        if(hovered && io.MouseWheel)view.zoom_at({io.MousePos.x-offset.x,io.MousePos.y-offset.y},std::pow(1.2,io.MouseWheel));
        if(ImGui::IsItemActive() && ImGui::IsMouseDragging(1)) {view.origin.x+=io.MouseDelta.x;view.origin.y+=io.MouseDelta.y;}
        if(hovered && ImGui::IsMouseClicked(0)) {
            double best=144;dragging=-1;
            for(std::size_t i=0;i<pairs.size();++i) {const auto p=screen(editing==0?pairs[i].target:pairs[i].source);const auto dx=io.MousePos.x-p.x,dy=io.MousePos.y-p.y;
                if(dx*dx+dy*dy<best){best=dx*dx+dy*dy;dragging=int(i);selected=i;}}
        }
        if(dragging>=0 && ImGui::IsMouseDragging(0)) {
            const auto p=view.screen_to_image({io.MousePos.x-offset.x,io.MousePos.y-offset.y});
            auto& destination=editing==0?pairs[std::size_t(dragging)].target:pairs[std::size_t(dragging)].source;
            if(destination.x!=p.x || destination.y!=p.y) {destination=p;invalidate();}
        }
        if(!ImGui::IsMouseDown(0))dragging=-1;
        if(!tiles)tiles=std::make_unique<GuiTiles>(server);tiles->set_display(display);
        const auto plan=plan_view_tiles(server->metadata(),view,size.x,size.y,1,256,128);tiles->update(plan);
        auto* draw=ImGui::GetWindowDrawList();draw->PushClipRect(offset,{offset.x+size.x,offset.y+size.y},true);
        draw->AddRectFilled(offset,{offset.x+size.x,offset.y+size.y},IM_COL32(18,21,27,255));
        const auto vertex=[&](Point p) {const auto s=screen(p);return ImVec2{float(s.x),float(s.y)};};
        for(const auto& tile:plan.tiles)if(const auto texture=tiles->texture(tile))draw->AddImage(static_cast<ImTextureID>(texture),vertex({tile.bounds.x,tile.bounds.y}),vertex({tile.bounds.x+tile.bounds.width,tile.bounds.y+tile.bounds.height}));
        if(preview && current)for(const auto p:preview->positions)draw->AddCircleFilled(vertex(p),2,IM_COL32(50,230,110,180));
        for(std::size_t i=0;i<pairs.size();++i) {
            const auto& pair=pairs[i];if(!std::isfinite(pair.source.x) || !std::isfinite(pair.source.y) || !std::isfinite(pair.target.x) || !std::isfinite(pair.target.y))continue;
            const auto a=vertex(pair.source),b=vertex(pair.target);draw->AddLine(a,b,IM_COL32(150,150,150,150));
            draw->AddCircle(a,6,IM_COL32(40,210,255,255),0,i==selected?3.f:1.5f);
            draw->AddRect({b.x-6,b.y-6},{b.x+6,b.y+6},IM_COL32(255,150,40,255),0,0,i==selected?3.f:1.5f);
            draw->AddText({b.x+8,b.y},pair.held_out?IM_COL32(255,230,80,255):IM_COL32_WHITE,pair.id.c_str());
            if(fit && current)draw->AddLine(vertex(fit->transform().map(pair.source)),b,IM_COL32(255,80,200,255),2);
        }
        draw->PopClipRect();
    }
    enum class Work {Add,Remove,Preview,Load,Accept};
    ui::Availability availability(Work action,bool blocked) const {
        if((action==Work::Preview || action==Work::Load || action==Work::Accept) && blocked)return ui::WorkflowBlocked();
        if(action==Work::Remove)return {};
        if(action==Work::Accept) {
            if(!fit || !request)return {false,"Run a preview first."};
            if(!current)return {false,"Fit a new preview for the current expression source and image calibration."};
            return {};
        }
        if(!dataset)return {false,"Select an expression source first."};
        if(action==Work::Add && pairs.size()>=1024)return {false,"Remove a landmark before adding another."};
        return {};
    }
    void controls(Document& doc,bool blocked) {
        ui::TextWrapped("Landmarks use source pixels and image-level-zero target pixels. Cyan circles: source; orange squares: target; green: fitted expression positions.");
        if(sources.size()>1) if(ui::ComboScope combo{"Expression source",source.empty()?"Choose source":source.c_str()};combo) {
            for(const auto& choice:sources)if(ui::Selectable(ui::data_label(choice,std::to_string(&choice-sources.data())).c_str(),source==choice)) {source=choice;pairs.clear();sources_dirty=true;invalidate();}
        }
        int model=int(options.model);if(ui::Combo("Fit model",&model,"Translation\0Similarity\0Affine\0")){options.model=static_cast<LandmarkModel>(model);options.reflect_source_y=false;invalidate();}
        if(options.model==LandmarkModel::Similarity && ui::Checkbox("Reflect source Y explicitly",&options.reflect_source_y))invalidate();
        ui::Combo("Drag handles",&editing,"Target squares\0Source circles\0");
        ui::TextDisabled("Right drag: pan; wheel: zoom.");
        if(ui::AvailableButton("Add landmark",availability(Work::Add,blocked))) {const auto p=view.screen_to_image({canvas_size.x/2,canvas_size.y/2});std::string name;
            do{name="L"+std::to_string(next_id++);}while(std::any_of(pairs.begin(),pairs.end(),[&](const auto& pair){return pair.id==name;}));
            pairs.push_back({name,p,p});selected=pairs.size()-1;invalidate();}
        ui::LayoutRegion landmarks_region("Landmarks");ui::PaneTitle("Landmarks");
        if(pairs.empty())ui::TextDisabled("No landmarks. Use %s.",ui::i18n::text("Add landmark"));
        if(ImGui::BeginListBox("##landmarks",{ui::available(),ImGui::GetTextLineHeightWithSpacing()*4})) {
            for(std::size_t i=0;i<pairs.size();++i)if(ui::Selectable(ui::data_label(pairs[i].id+(pairs[i].held_out?" (held out)":""),std::to_string(i)).c_str(),i==selected))selected=i;
            ImGui::EndListBox();
        }
        if(selected<pairs.size()) {
            auto& pair=pairs[selected];bool changed=false;
            changed=ui::InputDouble("Source X",&pair.source.x)||changed;changed=ui::InputDouble("Source Y",&pair.source.y)||changed;
            changed=ui::InputDouble("Target X",&pair.target.x)||changed;changed=ui::InputDouble("Target Y",&pair.target.y)||changed;
            changed=ui::Checkbox("Held out of fitting",&pair.held_out)||changed;
            if(fit)ui::Text("Residual: %.6g px",fit->residuals()[selected].distance);
            if(ui::AvailableButton("Remove selected",availability(Work::Remove,blocked))){pairs.erase(pairs.begin()+std::ptrdiff_t(selected));selected=0;changed=true;}
            if(changed)invalidate();
        }
        if(ui::InputText("Registration name",id.data(),id.size()))accepted=false;
        const auto fit_available=availability(Work::Preview,blocked);
        if(ui::AvailableButton("Fit preview",fit_available,ui::design::Action::Primary))attempt([&]{calculate(doc);});mark(Action::Preview);
        ui::WorkflowReason("Fit preview",fit_available);
        if(ui::AvailableButton("Load saved landmarks",availability(Work::Load,blocked)))attempt([&]{load(doc);});mark(Action::Load);
        if(ui::AvailableButton("Accept registration",availability(Work::Accept,blocked)))attempt([&]{accept_landmark_registration(doc,id.data(),*request,*fit);accepted=true;});mark(Action::Accept);
        if(fit) {ui::Text("Training RMS: %.6g px",fit->training_rmse());if(fit->held_out_rmse())ui::Text("Held-out RMS: %.6g px",*fit->held_out_rmse());else ui::TextUnformatted("Held-out RMS: unavailable (no held-out pairs)");}
        ui::TextWrapped("%s",ui::i18n::text(accepted?"Accepted. Native Undo/Redo restores the alignment; save the project to keep it.":"Draft/preview only; not applied to this study."));
        ui::TextWrapped("Raw object geometry and gene counts stay unchanged. The accepted transform places expression overlays; other analyses must select their coordinate domain explicitly.");
        if(!error.empty())ui::TextWrapped("%s",error.c_str());
        if(tiles && !tiles->error().empty())ui::TextWrapped("%s",tiles->error().c_str());
    }
};
GuiLandmarkRegistration::GuiLandmarkRegistration():impl_(std::make_unique<Impl>()){}
GuiLandmarkRegistration::~GuiLandmarkRegistration()=default;
void GuiLandmarkRegistration::open(){impl_->open=true;}
void GuiLandmarkRegistration::close(){impl_->open=false;}
void GuiLandmarkRegistration::draw(Document& doc,const std::string& image_id,const DisplaySettings& display,bool blocked) {
    ui::i18n::SurfaceTrace translation_surface("tool/Registration");
    auto& s=*impl_;s.update(doc,image_id);if(!s.open)return;
    ui::SetNextToolWindow(ui::tool_window_label(ui::ToolId::Registration),{1100,850});
    if(ImGui::Begin(ui::tool_window_label(ui::ToolId::Registration),&s.open)) {
        const auto width=std::min(360.f*ui::design::scale(),ImGui::GetContentRegionAvail().x*.48f);
        ImGui::BeginChild("landmark-controls",{width,0});s.controls(doc,blocked);ImGui::EndChild();ImGui::SameLine();
        ImGui::BeginChild("landmark-view",{0,0});s.canvas(display);ImGui::EndChild();
    }ImGui::End();
}
void GuiLandmarkRegistration::set_landmarks(std::vector<LandmarkPair> pairs,LandmarkFitOptions options) {options.validate();impl_->pairs=std::move(pairs);impl_->options=options;impl_->selected=0;impl_->invalidate();}
const std::vector<LandmarkPair>& GuiLandmarkRegistration::landmarks() const{return impl_->pairs;}
std::shared_ptr<const LandmarkFit> GuiLandmarkRegistration::fit() const{return impl_->fit;}
const std::string& GuiLandmarkRegistration::error() const{return impl_->error;}
Rect GuiLandmarkRegistration::bounds(Action action) const{return impl_->bounds[std::size_t(action)];}
Point GuiLandmarkRegistration::handle_position(std::size_t i,bool target) const {const auto& pair=impl_->pairs.at(i);return impl_->screen(target?pair.target:pair.source);}
const std::string& GuiLandmarkRegistration::expression_source() const{return impl_->source;}
std::size_t GuiLandmarkRegistration::stashed_fit_count() const{return std::count_if(impl_->drafts.begin(),impl_->drafts.end(),[](const auto& draft){return bool(draft.fit);});}
std::size_t GuiLandmarkRegistration::source_refresh_count() const{return impl_->source_refreshes;}
std::size_t GuiLandmarkRegistration::selected_landmark() const{return impl_->selected;}
bool GuiLandmarkRegistration::settled() const{return !impl_->tiles || !impl_->tiles->pending();}
}

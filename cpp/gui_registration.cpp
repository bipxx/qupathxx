#include "gui_registration.hpp"
#include "gui_layout.hpp"
#include "gui_tiles.hpp"
#include "qupathxx/core.hpp"
#include <cmath>
#include <limits>

namespace qpx::detail {
struct GuiLandmarkRegistration::Impl {
    bool open=false,accepted=false;
    const Document* document=nullptr;
    std::shared_ptr<ImageServer> server;
    std::uint64_t revision=std::numeric_limits<std::uint64_t>::max();
    ObjectPlane plane;
    std::string image_id,source,error;
    std::vector<std::string> sources;
    std::optional<ExpressionDataset> dataset;
    std::vector<LandmarkPair> pairs;
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
        if(document==&doc && server==doc.server() && revision==doc.revision() && plane==doc.active_plane() && image_id==identity)return;
        const bool changed=document!=&doc || server!=doc.server() || plane!=doc.active_plane() || image_id!=identity;
        document=&doc;server=doc.server();revision=doc.revision();plane=doc.active_plane();image_id=identity;
        invalidate();dataset.reset();
        if(changed) {pairs.clear();selected=0;tiles.reset();canvas_size={};source.clear();}
        attempt([&] {
            sources=ExpressionDataset::sources(doc,plane);
            if(std::find(sources.begin(),sources.end(),source)==sources.end())source.clear();
            if(source.empty() && sources.size()==1)source=sources.front();
            if(source.empty())throw std::invalid_argument(sources.empty()?"No spatial expression source on this plane.":"Choose an expression source.");
            ExpressionCaptureOptions capture;capture.coordinate_frame="image-level-zero-px";capture.source_id=source;capture.plane=plane;
            dataset=ExpressionDataset::capture(doc,capture);
        });
    }
    void calculate(Document& doc) {
        invalidate();if(!dataset)throw std::invalid_argument("Select an expression source first");
        auto result=std::make_shared<const LandmarkFit>(fit_landmarks(pairs,options));
        CoordinateFrame from;from.id=dataset->coordinate_frame();from.source_id=dataset->source_id();from.plane=dataset->plane();
        RegistrationRequest candidate(*dataset,from,image_coordinate_frame(doc,image_id,plane),result->transform());
        auto field=registration_field(candidate);request=std::move(candidate);preview=std::move(field);fit=std::move(result);
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
        if(preview)for(const auto p:preview->positions)draw->AddCircleFilled(vertex(p),2,IM_COL32(50,230,110,180));
        for(std::size_t i=0;i<pairs.size();++i) {
            const auto& pair=pairs[i];if(!std::isfinite(pair.source.x) || !std::isfinite(pair.source.y) || !std::isfinite(pair.target.x) || !std::isfinite(pair.target.y))continue;
            const auto a=vertex(pair.source),b=vertex(pair.target);draw->AddLine(a,b,IM_COL32(150,150,150,150));
            draw->AddCircle(a,6,IM_COL32(40,210,255,255),0,i==selected?3.f:1.5f);
            draw->AddRect({b.x-6,b.y-6},{b.x+6,b.y+6},IM_COL32(255,150,40,255),0,0,i==selected?3.f:1.5f);
            draw->AddText({b.x+8,b.y},pair.held_out?IM_COL32(255,230,80,255):IM_COL32_WHITE,pair.id.c_str());
            if(fit)draw->AddLine(vertex(fit->transform().map(pair.source)),b,IM_COL32(255,80,200,255),2);
        }
        draw->PopClipRect();
    }
    void controls(Document& doc,bool blocked) {
        ui::TextWrapped("Landmarks use source pixels and image-level-zero target pixels. Cyan circles: source; orange squares: target; green: fitted expression positions.");
        if(sources.size()>1 && ui::BeginCombo("Expression source",source.empty()?"Choose source":source.c_str())) {
            for(const auto& choice:sources)if(ImGui::Selectable(choice.c_str(),source==choice)) {source=choice;pairs.clear();revision=std::numeric_limits<std::uint64_t>::max();invalidate();}
            ImGui::EndCombo();
        }
        int model=int(options.model);if(ui::Combo("Fit model",&model,"Translation\0Similarity\0Affine\0")){options.model=static_cast<LandmarkModel>(model);options.reflect_source_y=false;invalidate();}
        if(options.model==LandmarkModel::Similarity && ui::Checkbox("Reflect source Y explicitly",&options.reflect_source_y))invalidate();
        ui::Combo("Drag handles",&editing,"Target squares\0Source circles\0");
        ui::TextDisabled("Right drag: pan; wheel: zoom.");
        ImGui::BeginDisabled(!dataset || pairs.size()>=1024);
        if(ui::Button("Add landmark")) {const auto p=view.screen_to_image({canvas_size.x/2,canvas_size.y/2});std::string name;
            do{name="L"+std::to_string(next_id++);}while(std::any_of(pairs.begin(),pairs.end(),[&](const auto& pair){return pair.id==name;}));
            pairs.push_back({name,p,p});selected=pairs.size()-1;invalidate();}
        ImGui::EndDisabled();
        if(ImGui::BeginListBox("##landmarks",{ui::available(),ImGui::GetTextLineHeightWithSpacing()*4})) {
            for(std::size_t i=0;i<pairs.size();++i)if(ImGui::Selectable((pairs[i].id+(pairs[i].held_out?" (held out)":"")).c_str(),i==selected))selected=i;
            ImGui::EndListBox();
        }
        if(selected<pairs.size()) {
            auto& pair=pairs[selected];bool changed=false;
            changed=ui::InputDouble("Source X",&pair.source.x)||changed;changed=ui::InputDouble("Source Y",&pair.source.y)||changed;
            changed=ui::InputDouble("Target X",&pair.target.x)||changed;changed=ui::InputDouble("Target Y",&pair.target.y)||changed;
            changed=ui::Checkbox("Held out of fitting",&pair.held_out)||changed;
            if(fit)ui::Text("Residual: %.6g px",fit->residuals()[selected].distance);
            if(ui::Button("Remove selected")){pairs.erase(pairs.begin()+std::ptrdiff_t(selected));selected=0;changed=true;}
            if(changed)invalidate();
        }
        if(ui::InputText("Registration name",id.data(),id.size()))accepted=false;
        ImGui::BeginDisabled(!dataset || blocked);
        if(ui::Button("Fit preview"))attempt([&]{calculate(doc);});mark(Action::Preview);
        if(ui::Button("Load saved landmarks"))attempt([&]{load(doc);});mark(Action::Load);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!fit || !request || blocked);
        if(ui::Button("Accept registration"))attempt([&]{accept_landmark_registration(doc,id.data(),*request,*fit);revision=doc.revision();accepted=true;});mark(Action::Accept);
        ImGui::EndDisabled();
        if(fit) {ui::Text("Training RMS: %.6g px",fit->training_rmse());if(fit->held_out_rmse())ui::Text("Held-out RMS: %.6g px",*fit->held_out_rmse());else ui::TextUnformatted("Held-out RMS: unavailable (no held-out pairs)");}
        ui::TextWrapped("%s",accepted?"Accepted. Native Undo/Redo restores the alignment; save the project to retain it.":"Draft/preview only; not applied to this study.");
        ui::TextWrapped("Raw object geometry and gene counts stay unchanged. The accepted transform places expression overlays; other analyses must select their coordinate domain explicitly.");
        if(!error.empty())ui::TextWrapped("%s",error.c_str());
        if(tiles && !tiles->error().empty())ui::TextWrapped("%s",tiles->error().c_str());
    }
};
GuiLandmarkRegistration::GuiLandmarkRegistration():impl_(std::make_unique<Impl>()){}
GuiLandmarkRegistration::~GuiLandmarkRegistration()=default;
void GuiLandmarkRegistration::open(){impl_->open=true;}
void GuiLandmarkRegistration::draw(Document& doc,const std::string& image_id,const DisplaySettings& display,bool blocked) {
    auto& s=*impl_;if(!s.open)return;s.update(doc,image_id);
    ui::SetNextToolWindow("Landmark registration",{1100,850});
    if(ImGui::Begin("Landmark registration",&s.open)) {
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
bool GuiLandmarkRegistration::settled() const{return !impl_->tiles || !impl_->tiles->pending();}
}

#pragma once
#include "qupathxx/spatial_import.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/gene_view.hpp"
#include "gui_import_picker.hpp"
#include "gui_tiles.hpp"
#include "gui_indexed_import.hpp"
#include "qupathxx/backed_spatial_import.hpp"
#include <cstring>
#include <cmath>

namespace qpx::detail {
class GuiSpatialImporter {
public:
    enum class Action { Preview, Publish, Cancel, Open };
private:
    std::array<Rect,4> bounds_{};
    void mark(Action action) {const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();bounds_[std::size_t(action)]={lo.x,lo.y,hi.x-lo.x,hi.y-lo.y};}
    enum Path {Base,Image,Matrix,Features,Positions,Output};
    std::array<std::array<char,32769>,6> paths_{};
    std::array<std::array<char,257>,3> columns_{};
    std::array<char,257> search_{};
    SpatialImportRequest request_;
    SpatialImportJob job_;
    GuiImportPicker picker_;Path picking_=Base;
    std::shared_ptr<const SpatialFeatureCatalogue> catalogue_;
    std::shared_ptr<const SpatialImportBaseCatalogue> bases_;
    std::shared_ptr<const SpatialImportPreview> preview_;
    std::shared_ptr<const BackedSpatialImportPreview> backed_;
    std::weak_ptr<const BackedSpatialImportPreview> seen_backed_;
    std::unique_ptr<GuiIndexedImportInputs> indexed_inputs_;
    bool indexed_mode_=false,owned_=true;
    std::uint64_t epoch_=0,submitted_=0;
    std::weak_ptr<const SpatialFeatureCatalogue> seen_catalogue_;
    std::weak_ptr<const SpatialImportBaseCatalogue> seen_base_;
    std::weak_ptr<const SpatialImportPreview> seen_preview_;
    std::unique_ptr<GuiTiles> tiles_;
    Viewport view_;ImVec2 preview_size_{};
    std::string error_;bool open_=false,use_project_=true;int step_=0;std::size_t page_=0;
    void copy(Path field,const std::string& value) {
        if(value.size()>=paths_[field].size())throw std::invalid_argument("Import path exceeds32768 UTF-8 bytes");
        std::memcpy(paths_[field].data(),value.c_str(),value.size()+1);
    }
    void invalidate() {++epoch_;preview_.reset();backed_.reset();tiles_.reset();preview_size_={};error_.clear();}
    GuiIndexedImportInputs& indexed() {if(!indexed_inputs_)indexed_inputs_=std::make_unique<GuiIndexedImportInputs>();return *indexed_inputs_;}
    BackedSpatialImportRequest backed_request() {
        BackedSpatialImportRequest result;
        result.base={request_.input_project,request_.output_project,request_.image_path,request_.image_reader,request_.image_id,request_.image_series,ObjectPlane(-1,request_.options.z,request_.options.t)};
        result.store=indexed().store();result.positions=request_.positions;result.expected_base_sha256=request_.expected_base_sha256;
        result.position_options={request_.options.position_columns,request_.options.file_bytes,request_.options.work_bytes,request_.options.max_observations};
        result.position_unit=request_.position_unit;result.alignment=request_.alignment;result.owned=owned_;
        if(const auto source=indexed().source())result.expected_store_sha256=source->file_sha256();return result;
    }
    void sync() {
        request_.input_project=use_project_?paths_[Base].data():"";request_.image_path=use_project_?"":paths_[Image].data();
        request_.matrix=paths_[Matrix].data();request_.features=paths_[Features].data();request_.positions=paths_[Positions].data();request_.output_project=paths_[Output].data();
        for(unsigned i=0;i<3;++i)request_.options.position_columns[i]=columns_[i].data();
        if(!use_project_)request_.image_id=1;
    }
    void path(Path field,const char* label,std::vector<std::string> extensions,bool save=false) {
        ImGui::PushID(int(field));
        if(ui::InputText(label,paths_[field].data(),paths_[field].size())) {invalidate();if(field==Features) {catalogue_.reset();request_.expected_feature_sha256.clear();}if(field==Base) {bases_.reset();request_.expected_base_sha256.clear();request_.image_id=1;}}
        if(ui::Button("Browse...")) {picking_=field;picker_.open(paths_[field].data(),std::move(extensions),save);}ImGui::PopID();
    }
    void overlay() {
        if(!preview_ && !backed_)return;
        const auto image=backed_?backed_->image_server():preview_->image_server();
        const auto& frame=backed_?backed_->image_frame():preview_->image_frame();
        const auto& positions=backed_?backed_->mapped_positions():preview_->mapped_positions();
        const auto available=ImGui::GetContentRegionAvail();const ImVec2 size{std::max(1.f,available.x),std::clamp(available.y-80*ui::design::scale(),140.f,380.f*ui::design::scale())};
        const auto offset=ImGui::GetCursorScreenPos();
        if(!tiles_) {tiles_=std::make_unique<GuiTiles>(image);view_.fit(frame.width,frame.height,size.x,size.y);}
        else if(size.x!=preview_size_.x || size.y!=preview_size_.y)view_.center_on(view_.screen_to_image({preview_size_.x/2,preview_size_.y/2}),size.x,size.y);
        preview_size_=size;ImGui::InvisibleButton("import-preview",size,ImGuiButtonFlags_MouseButtonLeft);
        if(ImGui::IsItemHovered() && ImGui::GetIO().MouseWheel)view_.zoom_at({ImGui::GetIO().MousePos.x-offset.x,ImGui::GetIO().MousePos.y-offset.y},std::pow(1.2,ImGui::GetIO().MouseWheel));
        if(ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {view_.origin.x+=ImGui::GetIO().MouseDelta.x;view_.origin.y+=ImGui::GetIO().MouseDelta.y;}
        const auto plan=plan_view_tiles(image->metadata(),view_,size.x,size.y,1,256,128);tiles_->update(plan);
        auto* draw=ImGui::GetWindowDrawList();draw->PushClipRect(offset,{offset.x+size.x,offset.y+size.y},true);draw->AddRectFilled(offset,{offset.x+size.x,offset.y+size.y},IM_COL32(18,21,27,255));
        for(const auto& tile:plan.tiles)if(auto texture=tiles_->texture(tile)) {
            const auto a=view_.image_to_screen({tile.bounds.x,tile.bounds.y}),b=view_.image_to_screen({tile.bounds.x+tile.bounds.width,tile.bounds.y+tile.bounds.height});
            draw->AddImage(static_cast<ImTextureID>(texture),{offset.x+float(a.x),offset.y+float(a.y)},{offset.x+float(b.x),offset.y+float(b.y)});
        }
        for(const auto& position:positions) {const auto p=view_.image_to_screen(position);
            if(p.x>=0 && p.y>=0 && p.x<size.x && p.y<size.y)draw->AddCircleFilled({offset.x+float(p.x),offset.y+float(p.y)},2.5f*ui::design::scale(),IM_COL32(255,205,85,255));}
        draw->PopClipRect();if(!tiles_->error().empty())ui::TextWrapped("%s",tiles_->error().c_str());
    }
public:
    GuiSpatialImporter() {request_.image_id=1;for(unsigned i=0;i<3;++i)std::memcpy(columns_[i].data(),request_.options.position_columns[i].c_str(),request_.options.position_columns[i].size()+1);}
    void set_request(SpatialImportRequest request) {
        if(job_.busy())throw std::logic_error("Import is busy");indexed_mode_=false;request_=std::move(request);use_project_=!request_.input_project.empty();
        copy(Base,request_.input_project);copy(Image,request_.image_path);copy(Matrix,request_.matrix);copy(Features,request_.features);copy(Positions,request_.positions);copy(Output,request_.output_project);
        for(unsigned i=0;i<3;++i) {if(request_.options.position_columns[i].size()>256)throw std::invalid_argument("Coordinate column name too long");std::strcpy(columns_[i].data(),request_.options.position_columns[i].c_str());}
        invalidate();catalogue_.reset();bases_.reset();open_=true;
    }
    void set_request(const BackedSpatialImportRequest& request) {
        SpatialImportRequest common;common.input_project=request.base.input_project;common.output_project=request.base.output_project;
        common.image_path=request.base.image_path;common.image_reader=request.base.image_reader;common.image_id=request.base.image_id;common.image_series=request.base.image_series;
        common.positions=request.positions;common.options.position_columns=request.position_options.columns;common.options.file_bytes=request.position_options.file_bytes;
        common.options.work_bytes=request.position_options.work_bytes;common.options.max_observations=request.position_options.max_observations;
        common.options.z=request.base.plane.z;common.options.t=request.base.plane.t;common.position_unit=request.position_unit;common.alignment=request.alignment;
        common.expected_base_sha256=request.expected_base_sha256;set_request(std::move(common));indexed_mode_=true;owned_=request.owned;indexed().set_store(request.store,request.expected_store_sha256);
    }
    void set_index_inputs(const std::string& matrix,const std::string& features,const std::string& observations,const std::string& store,ExpressionMatrixImportOptions options={}) {
        if(job_.busy())throw std::logic_error("Import is busy");indexed_mode_=true;indexed().set_index(matrix,features,observations,store,options);invalidate();
    }
    void open(const Project* current=nullptr) {
        open_=true;if(request_.input_project.empty() && request_.image_path.empty() && current && !current->path().empty()) {
            use_project_=true;copy(Base,current->path());request_.image_id=current->active_image()?current->active_image():1;
            try{read_images();}catch(const std::exception& e){error_=e.what();}
        } else if(request_.input_project.empty() && request_.image_path.empty())use_project_=false;
    }
    void read_genes(){sync();submitted_=epoch_;if(indexed_mode_){indexed().read(job_,request_.options.work_bytes);error_.clear();}else if(job_.catalogue(request_.features,request_.options))error_.clear();}
    void read_images(){sync();if(job_.base_catalogue(request_.input_project))error_.clear();}
    void preview(){sync();invalidate();submitted_=epoch_;if(use_project_ && !bases_)throw std::invalid_argument("Read the saved project's images and choose one first");
        if(indexed_mode_){if(!indexed().source())throw std::invalid_argument("Read or index the full matrix first");if(job_.preview(backed_request(),indexed().source()))step_=3;}
        else if(job_.preview(request_))step_=3;
    }
    void publish(){submitted_=epoch_;if(backed_)job_.publish(backed_);else{if(!preview_)throw std::invalid_argument("Build a preview first");job_.publish(preview_);}}
    void poll() {
        job_.poll();
        if(indexed_inputs_)indexed_inputs_->poll(job_,request_.options.work_bytes);
        if(const auto catalogue=job_.catalogue_result();catalogue && catalogue!=seen_catalogue_.lock()) {seen_catalogue_=catalogue;catalogue_=catalogue;request_.expected_feature_sha256=catalogue->source_sha256;page_=0;}
        if(const auto base=job_.base_result();base && base!=seen_base_.lock()) {seen_base_=base;bases_=base;request_.expected_base_sha256=base->source_sha256;
            if(!base->images.empty() && std::none_of(base->images.begin(),base->images.end(),[&](const auto& image){return image.id==request_.image_id;}))request_.image_id=base->images[0].id;}
        if(const auto value=job_.result();value && value!=seen_preview_.lock()) {seen_preview_=value;if(submitted_==epoch_){preview_=value;tiles_.reset();preview_size_={};}}
        if(const auto value=job_.backed_result();value && value!=seen_backed_.lock()) {seen_backed_=value;if(submitted_==epoch_){backed_=value;tiles_.reset();preview_size_={};}}
    }
    void wait(){do{job_.wait();poll();}while(job_.busy());}
    void cancel(){job_.cancel();}
    void close(){open_=false;}
    bool busy() const {return job_.busy();}
    bool preview_ready() const {return (preview_ || backed_) && tiles_ && !tiles_->pending() && tiles_->error().empty();}
    SpatialImportPhase phase() const {return job_.phase();}
    std::shared_ptr<const SpatialImportPreview> result() const {return preview_;}
    std::shared_ptr<const BackedSpatialImportPreview> backed_result() const {return backed_;}
    std::shared_ptr<const ExpressionMatrixSource> indexed_source() const {return indexed_inputs_?indexed_inputs_->source():nullptr;}
    const std::string& error() const {return error_.empty()?job_.error():error_;}
    Rect bounds(Action action) const {return bounds_[std::size_t(action)];}
    std::string draw(bool blocked=false) {
        poll();bounds_={};std::string open_path;if(!open_)return open_path;
        ui::SetNextToolWindow("Import spatial expression",{780,760});
        if(ImGui::Begin("Import spatial expression",&open_)) {
            const auto invoke=[&](auto action){try{action();}catch(const std::exception& e){error_=e.what();}};
            const char* steps[]={"1. Image and files","2. Coordinates","3. Genes and limits","4. Preview and import"};ui::TextUnformatted(steps[step_]);
            if(job_.busy()) {ui::TextDisabled(job_.phase()==SpatialImportPhase::Publishing?"Writing new project...":job_.phase()==SpatialImportPhase::MatrixIndexing?"Indexing full matrix...":"Reading import inputs...");const auto clicked=ui::Button("Cancel import");mark(Action::Cancel);if(clicked)job_.cancel();}
            ImGui::BeginDisabled(job_.busy());
            if(step_==0) {
                int storage=indexed_mode_?1:0;
                if(ui::Combo("Expression storage",&storage,"Selected genes in project\0Full indexed matrix\0")){indexed_mode_=storage==1;invalidate();}
                ui::TextWrapped("%s",indexed_mode_?"Position rows join exact observation IDs. Full gene counts remain indexed on disk.":"Position rows must follow matrix-column order; feature rows must follow matrix-row order. Use Matrix Market integer counts and TSV tables.");
                int source=use_project_?0:1;
                if(ui::Combo("Image source",&source,"Saved project\0PPM / TIFF image\0")) {use_project_=source==0;request_.expected_base_sha256.clear();bases_.reset();request_.image_id=1;invalidate();}
                if(use_project_) {
                    path(Base,"Saved project",{".qpx"});if(ui::Button("Read project images"))invoke([&]{read_images();});
                    if(bases_ && ui::BeginCombo("Image",std::to_string(request_.image_id).c_str())) {
                        for(const auto& image:bases_->images)if(ImGui::Selectable((std::to_string(image.id)+": "+image.name).c_str(),image.id==request_.image_id)) {request_.image_id=image.id;invalidate();}
                        ImGui::EndCombo();
                    }
                    ui::TextWrapped("Saved project state is used. Save any current changes first if you need them in the import.");
                } else path(Image,"Image file",{".ppm",".tif",".tiff"});
                if(indexed_mode_){if(indexed().draw_files())invalidate();if(ui::Checkbox("Copy indexed matrix with project",&owned_))invalidate();}
                else{path(Matrix,"Counts (.mtx)",{".mtx"});path(Features,"Feature IDs (.tsv)",{".tsv"});}
                path(Positions,"Positions (.tsv)",{".tsv"});path(Output,"New project",{".qpx"},true);
            } else if(step_==1) {
                ui::TextWrapped("Map the three position-table headers exactly. X is image column; Y is image row. No axes or units are guessed.");
                for(unsigned i=0;i<3;++i)if(ui::InputText(i==0?"Observation ID column":i==1?"X column":"Y column",columns_[i].data(),columns_[i].size()))invalidate();
                int unit=int(request_.position_unit);if(ui::Combo("Source coordinate units",&unit,"Pixels\0Micrometres\0")) {request_.position_unit=static_cast<CoordinateUnit>(unit);invalidate();}
                ui::TextWrapped("Micrometres require known image calibration. Scaling and offsets below apply after conversion to image pixels.");
                for(const auto entry:{std::pair{0,"X scale"},std::pair{4,"Y scale"},std::pair{2,"X offset (px)"},std::pair{5,"Y offset (px)"}})
                    if(ui::InputDouble(entry.second,&request_.alignment.matrix[entry.first]))invalidate();
                int z=int(request_.options.z),t=int(request_.options.t),series=int(request_.image_series.value_or(0));
                if(ui::InputInt("Z plane",&z)) {if(z>=0 && z<65536)request_.options.z=std::uint32_t(z);invalidate();}
                if(ui::InputInt("Time point",&t)) {if(t>=0 && t<65536)request_.options.t=std::uint32_t(t);invalidate();}
                if(!use_project_ && ui::InputInt("TIFF series",&series)) {if(series>=0 && series<65536)request_.image_series=std::uint32_t(series);invalidate();}
            } else if(step_==2) {
                if(indexed_mode_){ui::TextWrapped("All genes are retained in the indexed matrix; no selected-gene truncation. This catalogue is paged.");invoke([&]{indexed().draw_catalogue(job_,request_.options.work_bytes);});}
                else{ui::TextWrapped("Store selected genes in the new project. The original full matrix is not embedded. Choose up to64 IDs.");
                if(ui::Button("Read feature catalogue"))invoke([&]{read_genes();});
                if(catalogue_) {
                    if(ui::InputText("Search IDs / symbols",search_.data(),search_.size()))page_=0;
                    const auto page=gene_feature_page(catalogue_->features,search_.data(),page_,24);
                    if(ImGui::BeginChild("features",{0,ui::CollectionHeight(page.indices.size())},ImGuiChildFlags_Borders)) {
                        for(auto row:page.indices) {const auto& gene=catalogue_->features[row];auto at=std::find(request_.options.features.begin(),request_.options.features.end(),gene);bool chosen=at!=request_.options.features.end();
                            ImGui::PushID(int(row));if(ImGui::Checkbox("##gene",&chosen)) {if(chosen)request_.options.features.push_back(gene);else request_.options.features.erase(at);invalidate();}
                            ImGui::SameLine();ImGui::TextUnformatted(gene.c_str());ImGui::PopID();}
                    }ImGui::EndChild();
                    ImGui::BeginDisabled(page_==0);if(ui::Button("Previous genes"))--page_;ImGui::EndDisabled();ImGui::SameLine();
                    ImGui::BeginDisabled((page_+1)*24>=page.matches);if(ui::Button("Next genes"))++page_;ImGui::EndDisabled();
                    ui::Text("%zu selected; %zu matching IDs",request_.options.features.size(),page.matches);
                }
                }
                int file=int(request_.options.file_bytes/(1024*1024)),work=int(request_.options.work_bytes/(1024*1024));
                if(ui::InputInt("Per-file limit (MiB)",&file) && file>0 && file<=1024) {request_.options.file_bytes=std::size_t(file)*1024*1024;invalidate();}
                if(ui::InputInt("Work limit (MiB)",&work) && work>0 && work<=1024) {request_.options.work_bytes=std::size_t(work)*1024*1024;invalidate();}
            } else {
                const auto rebuild=ui::Button("Build / refresh preview");mark(Action::Preview);if(rebuild)invoke([&]{preview();});
                if(preview_ || backed_) {
                    if(backed_)ui::Text("%zu spots; all %llu matrix features",backed_->mapped_positions().size(),static_cast<unsigned long long>(backed_->spatial().source()->metadata().features));
                    else ui::Text("%zu spots; %zu selected / %u matrix features",preview_->projection().observations.size(),preview_->projection().features.size(),preview_->projection().matrix_features);
                    ui::TextWrapped("Counts are exact integers. Coordinates below are mapped image pixels; original coordinates and the explicit recipe are retained.");
                    overlay();ui::TextWrapped("Output: %s",backed_?backed_->request().base.output_project.c_str():preview_->request().output_project.c_str());
                    if(job_.phase()!=SpatialImportPhase::Complete) {ImGui::BeginDisabled(!preview_ready());const auto clicked=ui::Button("Import new project");mark(Action::Publish);if(clicked)invoke([&]{publish();});ImGui::EndDisabled();}
                    if(job_.phase()==SpatialImportPhase::Complete) {ui::TextUnformatted("New project saved.");ImGui::BeginDisabled(blocked);const auto clicked=ui::Button("Open imported project");mark(Action::Open);if(clicked)open_path=backed_?backed_->request().base.output_project:preview_->request().output_project;ImGui::EndDisabled();}
                }
            }
            if(step_>0 && ui::Button("Back"))--step_;if(step_<3) {if(step_>0)ImGui::SameLine();if(ui::Button("Next")) {++step_;if(step_==2 && (indexed_mode_?!indexed().source():!catalogue_))invoke([&]{read_genes();});if(step_==3)invoke([&]{preview();});}}
            ImGui::EndDisabled();
            if(!error().empty())ui::TextWrapped("%s",error().c_str());
            if(const auto selected=picker_.draw()) {invoke([&]{copy(picking_,*selected);invalidate();if(picking_==Features) {catalogue_.reset();request_.expected_feature_sha256.clear();}if(picking_==Base) {bases_.reset();request_.expected_base_sha256.clear();request_.image_id=1;}});}
        }ImGui::End();return open_path;
    }
};
}

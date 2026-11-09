#include "gui_fluorescence.hpp"
#include "gui_layout.hpp"
#include "qupathxx/image_data_properties.hpp"
#include <GLFW/glfw3.h>
#include <future>
#include <chrono>
#include <algorithm>
namespace qpx::detail {
struct GuiFluorescence::Impl {
    bool open=false,use_selected=true,use_crop=false,shown=true,body=false,pending=false,verify=false,published=false;
    RawRegionRequest region;FluorescenceCellOptions options;std::array<std::uint32_t,2> context{32,32};
    int method=0,units=0,row=0,channel=0;
    FluorescenceCellJob job;FluorescenceCellStats stats;
    std::optional<FluorescenceCellRequest> request;
    std::shared_ptr<const FluorescenceCellRun> run;
    std::future<std::shared_ptr<const FluorescenceCellContours>> contours;
    std::atomic<bool> cancel{false};bool discard_contours=false;
    const Document* owner=nullptr;std::weak_ptr<ImageServer> source;
    std::shared_ptr<const ImageDataProperties> record_properties;
    std::vector<std::string> records;std::string record,kept,error,state="idle";
    std::size_t count=0,boundary_count=0;std::array<Rect,6> bounds{};
    GLuint texture=0;std::shared_ptr<const FluorescenceCellRun> texture_run;bool texture_body=false;
    int texture_width=0,texture_height=0;bool sampled=false;std::uint32_t display_z=0,display_t=0;
    ~Impl(){job.cancel();cancel.store(true);if(contours.valid())contours.wait();clear_texture();}
    bool busy() const{return pending || job.busy() || contours.valid();}
    void clear_texture(){if(texture)glDeleteTextures(1,&texture);texture=0;texture_run.reset();}
    void invalidate(){job.cancel();cancel.store(true);discard_contours=true;pending=false;verify=false;request.reset();run.reset();kept.clear();published=false;count=0;row=channel=0;clear_texture();}
    void refresh(Document& doc){if(owner==&doc && source.lock()==doc.server())return;
        invalidate();owner=&doc;source=doc.server();record_properties.reset();records.clear();record.clear();error.clear();state="idle";}
    void refresh_records(Document& doc){if(record_properties==doc.image_data_properties())return;
        record_properties=doc.image_data_properties();records=fluorescence_cell_record_ids(doc);
        if(std::find(records.begin(),records.end(),record)==records.end())record=records.empty()?std::string{}:records.back();}
    template<class F>void attempt(F f){try{f();error.clear();}catch(const std::exception& e){error=e.what();}}
    void mark(Action a){const auto p=ImGui::GetItemRectMin(),q=ImGui::GetItemRectMax();bounds[std::size_t(a)]={p.x,p.y,q.x-p.x,q.y-p.y};}
    void start(Document& doc,std::uint64_t selected){
        auto selection=region;const auto object=use_selected?selected:0;
        if(use_selected && !object)throw std::invalid_argument("Select an area annotation or turn off Use selected ROI");
        if(!object && !use_crop){selection.region={0,0,0,doc.metadata().width,doc.metadata().height};}
        auto candidate=FluorescenceCellRequest(doc,selection,object,options,context);
        if(!job.request(candidate))throw std::runtime_error("Fluorescence job is still running");
        request=std::move(candidate);run.reset();clear_texture();pending=true;verify=false;published=false;kept.clear();count=0;state="running";row=channel=0;
    }
    void replay(Document& doc){auto candidate=replay_fluorescence_cell_record(doc,record);
        if(!job.request(candidate))throw std::runtime_error("Fluorescence job is still running");
        region=candidate.core_region();options=candidate.options();context=candidate.context();use_selected=candidate.object_id()!=0;use_crop=!use_selected;
        method=int(options.threshold_method);units=int(options.units);request=std::move(candidate);run.reset();clear_texture();pending=true;verify=true;kept=record;published=false;count=0;state="running";
    }
    void poll(Document& doc,bool blocked){refresh(doc);if(const auto view=doc.server()->tiff_view()){display_z=view->z;display_t=view->t;}stats=job.poll(doc);
        if(pending && stats.state!="running"){
            pending=false;state=stats.state;
            if(stats.state=="ready" && request){run=job.result();boundary_count=0;for(const auto& b:run->raster().boundaries)if(b.nucleus || b.cell)++boundary_count;if(verify){attempt([&]{if(!verify_fluorescence_cell_record(doc,kept,*request,*run))throw std::runtime_error("Saved fluorescence result differs from replay");state="verified";});verify=false;}}
            else {run.reset();if(!stats.error.empty())error=stats.error;}
        }
        if(run && request && !request->matches(doc)){run.reset();request.reset();kept.clear();published=false;count=0;state="stale";clear_texture();cancel.store(true);discard_contours=true;}
        if(contours.valid() && (!blocked || discard_contours) && contours.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){
            try{auto traced=contours.get();if(!discard_contours && run && request){count=publish_fluorescence_cells(doc,*request,*traced,"Fluorescence cell",options.work_bytes,&cancel).size();published=true;state="published";error.clear();}}
            catch(const std::exception& e){if(!discard_contours){state="failed";error=e.what();}}
            if(discard_contours && state=="cancelling")state="cancelled";
        }
        attempt_records(doc);
    }
    void attempt_records(Document& doc){try{refresh_records(doc);}catch(const std::exception& e){error=e.what();records.clear();record.clear();}}
    void build_texture(){
        if(texture_run==run && texture_body==body)return;clear_texture();if(!run)return;
        const auto& r=run->raster();GLint maximum=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);const auto edge=std::min(1024,maximum);if(edge<=0)return;
        texture_width=int(std::min<std::int64_t>(r.width,edge));texture_height=int(std::min<std::int64_t>(r.height,edge));sampled=texture_width!=r.width || texture_height!=r.height;
        std::vector<unsigned char> rgba(std::size_t(texture_width)*texture_height*4,0);const auto& labels=body?r.bodies:r.nuclei;
        for(int y=0;y<texture_height;++y)for(int x=0;x<texture_width;++x){const auto sx=(2*std::int64_t(x)+1)*r.width/(2*texture_width),sy=(2*std::int64_t(y)+1)*r.height/(2*texture_height);
            const auto label=labels[std::size_t(sy)*std::size_t(r.width)+std::size_t(sx)];if(!label)continue;const auto at=(std::size_t(y)*texture_width+x)*4;rgba[at]=body?40:210;rgba[at+1]=210;rgba[at+2]=body?180:70;rgba[at+3]=115;}
        GLint old=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&old);glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812F);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812F);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,texture_width,texture_height,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());glBindTexture(GL_TEXTURE_2D,GLuint(old));
        if(glGetError()!=GL_NO_ERROR){clear_texture();error="Fluorescence preview texture allocation failed";}
        texture_run=run;texture_body=body;
    }
    void controls(Document& doc,std::uint64_t selected,bool blocked){
        ui::TextWrapped("Original-channel analysis. RGB display colors do not select measurement channels.");
        if(const auto view=doc.server()->tiff_view())ui::Text("Source series: %u",view->series);
        ImGui::BeginDisabled(busy() || blocked);const bool can_run=doc.server()->reader_id()=="tiff" && (!use_selected || selected);
        ImGui::BeginDisabled(!can_run);if(ui::Button("Run fluorescence"))attempt([&]{start(doc,selected);});mark(Action::Run);ImGui::EndDisabled();ImGui::EndDisabled();
        ImGui::BeginDisabled(!busy());if(ui::Button("Cancel fluorescence")){job.cancel();cancel.store(true);discard_contours=true;state="cancelling";}mark(Action::Cancel);ImGui::EndDisabled();
        ui::Text("Fluorescence state: %s",state.c_str());
        if(run){ui::Text("Detected raw cells: %zu",run->raster().measurements.size());
            ui::Text("Boundary cells: %zu",boundary_count);ui::Text("Resolved threshold: %.8g",run->raster().resolved_threshold);ui::Checkbox("Show fluorescence preview",&shown);ui::Checkbox("Preview cell bodies",&body);
            if(run->region().z!=display_z || run->region().t!=display_t)ui::TextWrapped("Preview hidden: display Z/time differs from the analysis plane.");
            if(sampled)ui::TextWrapped("Preview is sampled to at most1024 pixels per axis; analysis retains the original lattice.");
        }
        ImGui::BeginDisabled(blocked || busy() || !run || published || !has_geos() || (run && run->raster().measurements.empty()));
        if(ui::Button("Keep fluorescence cells"))attempt([&]{require_current_fluorescence_cells(doc,*request,*run);cancel.store(false);discard_contours=false;const auto snapshot=run;TissueContourOptions limit;limit.work_bytes=options.work_bytes;
            contours=std::async(std::launch::async,[this,snapshot,limit]{return trace_fluorescence_cells(snapshot,limit,&cancel);});state="tracing";});mark(Action::KeepCells);ImGui::EndDisabled();
        if(published)ui::Text("Published raw cells: %zu (Undo available)",count);
        if(!has_geos())ui::TextWrapped("Cell objects require GEOS. Run records remain available.");
        ImGui::BeginDisabled(blocked || busy() || !run || !kept.empty());if(ui::Button("Keep fluorescence run"))attempt([&]{kept=keep_fluorescence_cell_run(doc,*request,*run);record=kept;});mark(Action::KeepRecord);ImGui::EndDisabled();
        if(!kept.empty())ui::TextWrapped("Retained record: %s",kept.c_str());
        if(run && !run->raster().measurements.empty() && ui::CollapsingHeader("Inspect compartment measurements"))measurements();
        ImGui::BeginDisabled(busy() || blocked);if(ui::BeginCombo("Saved fluorescence record",record.empty()?"None":record.c_str())){for(const auto& id:records)if(ImGui::Selectable(id.c_str(),id==record))record=id;ui::EndCombo();}
        ImGui::BeginDisabled(record.empty());if(ui::Button("Replay fluorescence record"))attempt([&]{replay(doc);});mark(Action::Replay);
        if(ui::Button("Remove fluorescence record"))attempt([&]{const auto removing=record;remove_fluorescence_cell_record(doc,removing);if(kept==removing)kept.clear();});mark(Action::Remove);ImGui::EndDisabled();ImGui::EndDisabled();
        ui::TextWrapped("Replay verifies retained analysis identity. Later edits to published cells do not recompute observed intensities.");
        if(!error.empty())ui::TextWrapped("Fluorescence: %s",error.c_str());
        if(ui::CollapsingHeader("Fluorescence settings"))settings(blocked,selected);
    }
    void settings(bool blocked,std::uint64_t selected){
        ImGui::BeginDisabled(busy() || blocked);bool changed=false;
        changed=ui::Checkbox("Use selected ROI",&use_selected)||changed;
        if(use_selected)ui::Text("Selected ROI: %llu",static_cast<unsigned long long>(selected));
        else {changed=ui::Checkbox("Use explicit crop",&use_crop)||changed;if(use_crop){
            changed=ui::InputScalar("Crop X",ImGuiDataType_S64,&region.region.x)||changed;changed=ui::InputScalar("Crop Y",ImGuiDataType_S64,&region.region.y)||changed;
            changed=ui::InputScalar("Crop width",ImGuiDataType_S64,&region.region.width)||changed;changed=ui::InputScalar("Crop height",ImGuiDataType_S64,&region.region.height)||changed;}}
        changed=ui::InputScalar("First original channel",ImGuiDataType_U32,&region.first_channel)||changed;changed=ui::InputScalar("Channel count",ImGuiDataType_U32,&region.channels)||changed;
        changed=ui::InputScalar("Segmentation channel in block",ImGuiDataType_U32,&options.segmentation_channel)||changed;
        changed=ui::InputScalar("Analysis Z",ImGuiDataType_U32,&region.z)||changed;changed=ui::InputScalar("Analysis time",ImGuiDataType_U32,&region.t)||changed;
        changed=ui::Combo("Threshold method",&method,"Explicit\0Otsu 256\0")||changed;options.threshold_method=FluorescenceThreshold(method);
        if(!method)changed=ui::InputDouble("Raw threshold",&options.threshold,0,0,"%.8g")||changed;
        changed=ui::Combo("Analysis units",&units,"Pixels\0Micrometres\0")||changed;options.units=FluorescenceCellUnits(units);
        changed=ui::InputDouble("Gaussian sigma",&options.sigma,0,0,"%.5g")||changed;changed=ui::InputDouble("Peak prominence",&options.peak_prominence,0,0,"%.5g")||changed;
        changed=ui::InputDouble("Minimum nucleus area",&options.min_nucleus_area,0,0,"%.5g")||changed;
        bool max=bool(options.max_nucleus_area);if(ui::Checkbox("Limit nucleus area",&max)){options.max_nucleus_area=max?std::optional<double>(1000):std::nullopt;changed=true;}
        if(max)changed=ui::InputDouble("Maximum nucleus area",&*options.max_nucleus_area,0,0,"%.5g")||changed;
        changed=ui::InputDouble("Raw cell expansion",&options.cell_expansion,0,0,"%.5g")||changed;
        changed=ui::InputScalar("Context X",ImGuiDataType_U32,&context[0])||changed;changed=ui::InputScalar("Context Y",ImGuiDataType_U32,&context[1])||changed;
        changed=ui::InputScalar("Maximum cells",ImGuiDataType_U64,&options.max_cells)||changed;changed=ui::InputScalar("Work bytes",ImGuiDataType_U64,&options.work_bytes)||changed;
        if(changed){invalidate();state="idle";}
        ImGui::EndDisabled();
    }
    void measurements(){const auto& all=run->raster().measurements;ui::InputInt("Inspect raw cell",&row);row=std::clamp(row,0,int(all.size()-1));const auto& m=all[std::size_t(row)];
        ui::InputInt("Inspect channel in block",&channel);channel=std::clamp(channel,0,int(m.nucleus.channels.size()-1));const auto original=run->region().first_channel+std::uint32_t(channel);
        const auto& names=run->source_metadata().channel_names;ui::Text("Original channel: %u / %s",original,original<names.size()?names[original].c_str():"unnamed");
        const auto compartment=[&](const char* title,const RawCellCompartment& c){ui::Text("%s: %llu pixels / %llu valid",title,(unsigned long long)c.pixels,(unsigned long long)c.valid_pixels);
            const auto& v=c.channels[std::size_t(channel)];ui::Text("Finite: %llu / nonfinite: %llu",(unsigned long long)v.finite_samples,(unsigned long long)v.nonfinite_samples);
            if(v.mean)ui::Text("Mean: %.8g",*v.mean);else ui::TextUnformatted("Mean: absent");if(v.sum)ui::Text("Sum: %.8g",*v.sum);if(v.sample_standard_deviation)ui::Text("Sample SD: %.8g",*v.sample_standard_deviation);};
        compartment("Nucleus",m.nucleus);compartment("Cell body",m.cell);if(m.cytoplasm)compartment("Cytoplasm",*m.cytoplasm);else ui::TextUnformatted("Cytoplasm: absent");
    }
};
GuiFluorescence::GuiFluorescence():impl_(std::make_unique<Impl>()){}
GuiFluorescence::~GuiFluorescence()=default;
void GuiFluorescence::open(){impl_->open=true;}
void GuiFluorescence::poll(Document& doc,bool blocked){impl_->poll(doc,blocked);}
void GuiFluorescence::draw(Document& doc,std::uint64_t selected,bool blocked){auto& s=*impl_;s.poll(doc,blocked);s.bounds={};if(!s.open)return;
    ui::SetNextToolWindow("Fluorescence cells",{520,780});if(ImGui::Begin("Fluorescence cells",&s.open)){s.controls(doc,selected,blocked);}ImGui::End();}
void GuiFluorescence::draw_preview(ImDrawList* draw,const Viewport& view,ImVec2 offset,ImVec2 size){auto& s=*impl_;if(!s.shown || !s.run || s.run->region().z!=s.display_z || s.run->region().t!=s.display_t)return;s.build_texture();if(!s.texture)return;
    const auto& r=s.run->region().region;const auto a=view.image_to_screen({double(r.x),double(r.y)}),b=view.image_to_screen({double(r.x+r.width),double(r.y+r.height)});
    draw->PushClipRect(offset,{offset.x+size.x,offset.y+size.y},true);draw->AddImage(ImTextureID(s.texture),{offset.x+float(a.x),offset.y+float(a.y)},{offset.x+float(b.x),offset.y+float(b.y)});draw->PopClipRect();}
void GuiFluorescence::set_request(RawRegionRequest r,FluorescenceCellOptions o,std::array<std::uint32_t,2> c,bool selected){auto& s=*impl_;if(s.busy())throw std::runtime_error("Fluorescence is busy");o.validate();s.invalidate();s.region=r;s.options=o;s.context=c;s.use_selected=selected;s.use_crop=!selected;s.method=int(o.threshold_method);s.units=int(o.units);}
Rect GuiFluorescence::bounds(Action a) const{return impl_->bounds[std::size_t(a)];}
std::shared_ptr<const FluorescenceCellRun> GuiFluorescence::result() const{return impl_->run;}
const std::string& GuiFluorescence::error() const{return impl_->error;}
const std::string& GuiFluorescence::state() const{return impl_->state;}
const std::string& GuiFluorescence::kept_record() const{return impl_->kept;}
std::size_t GuiFluorescence::published_cells() const{return impl_->count;}
bool GuiFluorescence::busy() const{return impl_->busy();}
}

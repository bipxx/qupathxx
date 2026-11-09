#include "gui_linear_unmixing.hpp"
#include "gui_layout.hpp"
#include "qupathxx/linear_unmixing_io.hpp"
#include "qupathxx/raw_analysis.hpp"
#include "qupathxx/workflow.hpp"
#include "qupathxx/image_data_properties.hpp"
#include <GLFW/glfw3.h>
#include <future>
#include <chrono>
#include <algorithm>
namespace qpx::detail {
namespace {
bool edit(const char* label,std::string& value,std::size_t limit=32768){std::vector<char> text(limit+1,0);std::copy_n(value.data(),std::min(value.size(),limit),text.data());if(!ui::InputText(label,text.data(),text.size()))return false;value=text.data();return true;}
}
GuiUnmixPreview::GuiUnmixPreview(RawPixelBuffer b,RawRegionRequest r,const std::atomic<bool>* cancel):samples(std::move(b)),region(r),finite(r.channels,0),nonfinite(r.channels,0){const auto& values=std::get<std::vector<double>>(samples.storage());for(std::size_t p=0;p<samples.valid().size();++p){if((p&1023)==0)workflow_checkpoint(cancel);if(!samples.valid()[p])continue;++valid;for(std::size_t c=0;c<r.channels;++c)++(std::isfinite(values[p*r.channels+c])?finite[c]:nonfinite[c]);}}
struct GuiLinearUnmixing::Impl {
    struct Opened {LinearUnmixModel model;std::optional<DarkFlatRecipe> parent;std::array<std::shared_ptr<ImageServer>,2> fields;};
    bool open=false,discard=false,select=false,records_dirty=true,preview_configured=false;
    std::string file,parent_id,saved,kept,request_id,error,state="idle",display_error;
    std::optional<LinearMixingMatrix> draft;std::optional<LinearUnmixRecipe> restoring;
    std::optional<RawAnalysisRequest> guard;std::optional<LinearUnmixRequest> request;
    std::future<Opened> opening;std::future<std::shared_ptr<const GuiUnmixPreview>> reading;
    std::shared_ptr<const GuiUnmixPreview> block;std::atomic<bool> cancel{false};
    const Document* owner=nullptr;std::weak_ptr<ImageServer> server;
    std::shared_ptr<const ImageDataProperties> properties;std::vector<std::string> records,parents;
    RawRegionRequest region;int rows=2,columns=2,row=0,column=0,pixel=0,channel=0;
    double lower=-32,upper=128;std::array<Rect,13> bounds{};
    GLuint texture=0;std::shared_ptr<const GuiUnmixPreview> texture_block;int texture_channel=-1,width=0,height=0;double texture_lower=0,texture_upper=0;
    ~Impl(){cancel.store(true);if(opening.valid())opening.wait();if(reading.valid())reading.wait();clear_texture();}
    bool busy()const{return opening.valid() || reading.valid();}
    void clear_texture(){if(texture)glDeleteTextures(1,&texture);texture=0;texture_block.reset();}
    void invalidate(const char* why="idle"){cancel.store(true);discard=true;select=false;guard.reset();request.reset();restoring.reset();block.reset();request_id.clear();kept.clear();clear_texture();state=why;}
    template<class F>void attempt(F f){try{f();error.clear();}catch(const std::exception& e){error=e.what();}}
    void mark(Action a){const auto p=ImGui::GetItemRectMin(),q=ImGui::GetItemRectMax();bounds[std::size_t(a)]={p.x,p.y,q.x-p.x,q.y-p.y};}
    void refresh(Document& doc){if(owner!=&doc || server.lock()!=doc.server()){invalidate();owner=&doc;server=doc.server();properties.reset();records.clear();parents.clear();saved.clear();records_dirty=true;error.clear();if(!preview_configured){region.region={0,0,0,std::min<std::int64_t>(256,doc.metadata().width),std::min<std::int64_t>(256,doc.metadata().height)};region.channels=1;}}
        if(guard && !guard->matches(doc))invalidate("stale");if(request && !request->matches(doc))invalidate("stale");
        if((open || request) && (records_dirty || properties!=doc.image_data_properties())){properties=doc.image_data_properties();records_dirty=false;try{records=linear_unmix_recipe_ids(doc);parents=dark_flat_recipe_ids(doc);if(std::find(records.begin(),records.end(),saved)==records.end())saved=records.empty()?std::string{}:records.back();kept=!request_id.empty() && std::find(records.begin(),records.end(),request_id)!=records.end()?request_id:std::string{};}catch(const std::exception& e){error=e.what();records.clear();parents.clear();saved.clear();kept.clear();}}
    }
    void start(Document& doc,int mode){std::optional<LinearUnmixRecipe> recipe;if(mode==2)recipe=load_linear_unmix_recipe(doc,saved);auto matrix=recipe?std::optional<LinearMixingMatrix>(recipe->matrix):draft;
        if(mode==1 && !matrix)throw std::invalid_argument("Create or load a matrix draft first");const auto selected_parent=recipe?recipe->correction_record:parent_id;
        std::optional<DarkFlatRecipe> parent;if(!selected_parent.empty())parent=load_dark_flat_recipe(doc,selected_parent);
        RawAnalysisRequest captured(doc,RawRegionRequest{});invalidate();cancel.store(false);discard=false;guard=std::move(captured);restoring=std::move(recipe);const auto path=file;
        opening=std::async(std::launch::async,[this,path,mode,matrix,parent]{auto p=mode==0?load_linear_mixing_matrix_file(path,&cancel):*matrix;LinearUnmixModel model(std::move(p),&cancel);std::array<std::shared_ptr<ImageServer>,2> fields;if(parent)for(std::size_t i=0;i<2;++i){TiffOpenOptions o;o.view.series=parent->sources[i+1].series;fields[i]=open_tiff(parent->sources[i+1].path,o,&cancel);}return Opened{std::move(model),parent,std::move(fields)};});state="opening";
    }
    void poll(Document& doc,bool blocked){refresh(doc);
        if(opening.valid() && (!blocked || discard) && opening.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){try{auto ready=opening.get();if(!discard && guard && guard->matches(doc)){std::optional<DarkFlatRequest> parent;if(ready.parent)parent=restore_dark_flat_recipe(doc,*ready.parent,ready.fields[0],ready.fields[1],&cancel);
                    if(restoring)load_linear_unmix_recipe(doc,restoring->sha256());
                    auto captured=restoring?restore_linear_unmix_recipe(doc,*restoring,ready.model,parent,&cancel):parent?LinearUnmixRequest(doc,*parent,ready.model,&cancel):LinearUnmixRequest(doc,ready.model,&cancel);
                    draft=ready.model.matrix();rows=int(draft->inputs.size());columns=int(draft->components.size());parent_id=captured.recipe().correction_record;request_id=captured.recipe().sha256();request=std::move(captured);kept=std::find(records.begin(),records.end(),request_id)!=records.end()?request_id:std::string{};if(!kept.empty())saved=kept;restoring.reset();state="ready";error.clear();
                }}catch(const std::exception& e){if(!discard){error=e.what();state="failed";}}}
        if(reading.valid() && reading.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){try{auto result=reading.get();if(!discard && request){request->require_current(doc,&cancel);block=std::move(result);pixel=channel=0;state="preview";error.clear();}}catch(const std::exception& e){if(!discard){error=e.what();state="failed";}}}
        if(discard && !busy() && state=="cancelling")state="cancelled";
    }
    void preview(){cancel.store(false);discard=false;block.reset();clear_texture();const auto captured=*request;const auto selected=region;reading=std::async(std::launch::async,[this,captured,selected]{constexpr std::size_t work=256u*1024*1024;const auto fixed=captured.recipe().payload_bytes()+65536;auto required=fixed;workflow_capture_charge(required,captured.source()->read_work_bytes(selected),work);auto samples=captured.source()->read(selected,work-fixed,&cancel);return std::make_shared<const GuiUnmixPreview>(std::move(samples),selected,&cancel);});state="reading";}
    void image(){if(!block)return;const auto& samples=block->samples;channel=std::clamp(channel,0,int(samples.channels()-1));if(!std::isfinite(lower) || !std::isfinite(upper) || upper<=lower || !std::isfinite(upper-lower)){display_error="Display range needs finite lower < upper";return;}display_error.clear();
        if(texture_block!=block || texture_channel!=channel || texture_lower!=lower || texture_upper!=upper){clear_texture();width=int(std::min<std::int64_t>(320,samples.width()));height=int(std::min<std::int64_t>(320,samples.height()));std::vector<unsigned char> rgba(std::size_t(width)*height*4,0);const auto& values=std::get<std::vector<double>>(samples.storage());
            for(int y=0;y<height;++y)for(int x=0;x<width;++x){const auto sx=(2*std::int64_t(x)+1)*samples.width()/(2*width),sy=(2*std::int64_t(y)+1)*samples.height()/(2*height);const auto at=std::size_t(sy*samples.width()+sx),p=std::size_t(y*width+x)*4;if(!samples.valid()[at])continue;const auto v=values[at*samples.channels()+std::size_t(channel)];if(std::isfinite(v)){const auto gray=static_cast<unsigned char>(255*std::clamp((v-lower)/(upper-lower),0.0,1.0));rgba[p]=rgba[p+1]=rgba[p+2]=gray;}else{rgba[p]=255;rgba[p+2]=255;}rgba[p+3]=255;}
            GLint old=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&old);glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());glBindTexture(GL_TEXTURE_2D,GLuint(old));texture_block=block;texture_channel=channel;texture_lower=lower;texture_upper=upper;}
        if(texture){const auto scale=std::min(16.f,320.f/std::max(width,height));ImGui::Image(ImTextureID(texture),{width*scale,height*scale});}
    }
    void editor(Document& doc){bool changed=false;ui::InputInt("Measured rows for new draft",&rows);ui::InputInt("Components for new draft",&columns);
        if(ui::Button("Create zero matrix draft"))attempt([&]{if(rows<1 || rows>64 || columns<1 || columns>rows)throw std::invalid_argument("New matrix requires1..64 rows and components<=rows");const auto m=doc.server()->raw_metadata();if(!m || std::size_t(rows)>m->channel_names.size())throw std::invalid_argument("New draft rows exceed named source channels");LinearMixingMatrix p;for(int i=0;i<rows;++i)p.inputs.push_back({std::uint32_t(i),m->channel_names[i]});for(int j=0;j<columns;++j)p.components.push_back("Component "+std::to_string(j+1));p.coefficients.assign(std::size_t(rows)*columns,0);draft=std::move(p);invalidate();});mark(Action::NewDraft);
        if(!draft)return;auto& p=*draft;int domain=p.input_domain=="original-channel-samples"?0:1;if(ui::Combo("Matrix input domain",&domain,"Original samples\0Dark/flat corrected samples\0")){p.input_domain=domain?"qpx.dark-flat-corrected-samples/1":"original-channel-samples";changed=true;}
        changed=ui::InputDouble("Maximum condition",&p.maximum_condition,0,0,"%.17g")||changed;ui::InputInt("Measured row",&row);ui::InputInt("Component column",&column);row=std::clamp(row,0,int(p.inputs.size()-1));column=std::clamp(column,0,int(p.components.size()-1));auto& binding=p.inputs[std::size_t(row)];changed=ui::InputScalar("Measured channel index",ImGuiDataType_U32,&binding.channel)||changed;changed=edit("Exact measured channel name",binding.name,96)||changed;changed=edit("Component name",p.components[std::size_t(column)],96)||changed;
        changed=ui::InputDouble("Mixing coefficient",&p.coefficients[std::size_t(row)*p.components.size()+column],0,0,"%.17g")||changed;mark(Action::Coefficient);
        ui::Text("Editing row %d (%s), component %d (%s)",row,binding.name.c_str(),column,p.components[std::size_t(column)].c_str());for(std::size_t j=0;j<p.components.size();++j)ui::Text("%s: %.17g",p.components[j].c_str(),p.coefficients[std::size_t(row)*p.components.size()+j]);
        ui::TextWrapped("Rows are measured channels; columns are components. Enter known coefficients, then Apply matrix draft. No spectra are estimated. Zero drafts require a full-rank matrix before use.");if(changed)invalidate();
    }
    void draw(Document& doc,bool blocked){poll(doc,blocked);bounds={};if(!open)return;ui::SetNextToolWindow("Linear unmixing",{640,920});if(ImGui::Begin("Linear unmixing",&open)){
            ui::Text("Unmixing state: %s",state.c_str());ui::TextWrapped("Measured = matrix * components. Signed outputs and residuals; original samples stay unchanged.");
            if(!has_linear_unmixing() || !has_linear_mixing_io())ui::TextWrapped("This build requires native OpenCV core and project/JSON capabilities for this tool.");
            ImGui::BeginDisabled(blocked || busy() || !has_linear_unmixing() || !has_linear_mixing_io());edit("Mixing matrix JSON path",file);
            if(ui::Button("Load matrix JSON"))attempt([&]{start(doc,0);});mark(Action::LoadFile);ImGui::SameLine();ImGui::BeginDisabled(!draft);if(ui::Button("Apply matrix draft"))attempt([&]{start(doc,1);});mark(Action::Apply);ImGui::EndDisabled();
            if(ui::BeginCombo("Input correction (explicit)",parent_id.empty()?"None / original input":parent_id.c_str())){if(ImGui::Selectable("None / original input",parent_id.empty())){parent_id.clear();invalidate();}for(const auto& id:parents)if(ImGui::Selectable(id.c_str(),id==parent_id)){parent_id=id;invalidate();}ui::EndCombo();}mark(Action::Parent);
            ImGui::BeginDisabled(!request);if(ui::Button("Preview unmixed samples"))attempt([&]{preview();});mark(Action::Preview);ImGui::SameLine();if(ui::Button("Keep unmixing recipe"))attempt([&]{kept=keep_linear_unmix_recipe(doc,*request,&cancel);saved=kept;records_dirty=true;});mark(Action::Keep);
            ImGui::BeginDisabled(kept.empty());if(ui::Button("Use unmixing for fluorescence"))attempt([&]{request->require_current(doc);select=true;open=false;});mark(Action::Use);ImGui::EndDisabled();ImGui::EndDisabled();ImGui::EndDisabled();
            ImGui::BeginDisabled(!busy());if(ui::Button("Cancel unmixing"))invalidate("cancelling");mark(Action::Cancel);ImGui::EndDisabled();
            if(!error.empty())ui::TextWrapped("Unmixing: %s",error.c_str());if(!kept.empty())ui::TextWrapped("Retained unmixing: %s",kept.c_str());if(request){ui::TextWrapped("Input domain: %s",request->recipe().matrix.input_domain.c_str());ui::TextWrapped("Output domain: %s",request->source()->domain().c_str());}
            ImGui::BeginDisabled(blocked || busy());if(ui::BeginCombo("Saved unmixing recipe",saved.empty()?"None":saved.c_str())){for(const auto& id:records)if(ImGui::Selectable(id.c_str(),id==saved))saved=id;ui::EndCombo();}ImGui::BeginDisabled(saved.empty());if(ui::Button("Load saved unmixing"))attempt([&]{start(doc,2);});mark(Action::LoadSaved);ImGui::SameLine();if(ui::Button("Remove unmixing recipe"))attempt([&]{remove_linear_unmix_recipe(doc,saved);saved.clear();kept.clear();records_dirty=true;});mark(Action::Remove);ImGui::EndDisabled();
            const bool editing=ui::CollapsingHeader("Edit explicit mixing matrix");mark(Action::Editor);if(editing)editor(doc);
            bool changed=false;if(ui::CollapsingHeader("Preview crop and outputs")){changed=ui::InputScalar("Preview X",ImGuiDataType_S64,&region.region.x);changed=ui::InputScalar("Preview Y",ImGuiDataType_S64,&region.region.y)||changed;changed=ui::InputScalar("Preview width",ImGuiDataType_S64,&region.region.width)||changed;changed=ui::InputScalar("Preview height",ImGuiDataType_S64,&region.region.height)||changed;changed=ui::InputScalar("First output channel",ImGuiDataType_U32,&region.first_channel)||changed;changed=ui::InputScalar("Output channel count",ImGuiDataType_U32,&region.channels)||changed;changed=ui::InputScalar("Preview Z",ImGuiDataType_U32,&region.z)||changed;changed=ui::InputScalar("Preview time",ImGuiDataType_U32,&region.t)||changed;if(changed){block.reset();clear_texture();}}ImGui::EndDisabled();
            if(block){const auto& b=block->samples;channel=std::clamp(channel,0,int(b.channels()-1));ui::Text("Crop %lld,%lld / %lld x %lld / Z%u T%u",(long long)block->region.region.x,(long long)block->region.region.y,(long long)b.width(),(long long)b.height(),block->region.z,block->region.t);ui::InputInt("Inspect output in block",&channel);channel=std::clamp(channel,0,int(b.channels()-1));ui::Text("Derived output %u / %s",block->region.first_channel+std::uint32_t(channel),request->source()->metadata().channel_names.at(block->region.first_channel+std::uint32_t(channel)).c_str());ui::Text("Valid pixels: %llu / finite %llu / nonfinite %llu",(unsigned long long)block->valid,(unsigned long long)block->finite[channel],(unsigned long long)block->nonfinite[channel]);ui::InputInt("Inspect pixel",&pixel);pixel=std::clamp(pixel,0,int(b.valid().size()-1));if(b.valid()[std::size_t(pixel)])ui::Text("Signed sample: %.17g",std::get<std::vector<double>>(b.storage())[std::size_t(pixel)*b.channels()+channel]);else ui::TextUnformatted("Sample: absent");ui::InputDouble("Display lower",&lower,0,0,"%.8g");ui::InputDouble("Display upper",&upper,0,0,"%.8g");ui::TextWrapped("Display range only. Preview samples at most320 pixels per axis; nonfinite magenta, absent transparent.");image();mark(Action::Image);if(!display_error.empty())ui::TextWrapped("%s",display_error.c_str());}
        }ImGui::End();}
};
GuiLinearUnmixing::GuiLinearUnmixing():impl_(std::make_unique<Impl>()){}GuiLinearUnmixing::~GuiLinearUnmixing()=default;
void GuiLinearUnmixing::open(){impl_->open=true;}void GuiLinearUnmixing::close(){impl_->open=false;}
void GuiLinearUnmixing::cancel(){impl_->invalidate("cancelling");}
void GuiLinearUnmixing::restore(Document& doc,std::string record){if(impl_->busy())throw std::runtime_error("Unmixing is busy");impl_->refresh(doc);impl_->saved=std::move(record);impl_->start(doc,2);}
std::optional<LinearUnmixRequest> GuiLinearUnmixing::request()const{return impl_->request;}
void GuiLinearUnmixing::poll(Document& doc,bool blocked){impl_->poll(doc,blocked);}void GuiLinearUnmixing::draw(Document& doc,bool blocked){impl_->draw(doc,blocked);}
void GuiLinearUnmixing::set_matrix_file(std::string file,std::string parent){if(impl_->busy())throw std::runtime_error("Unmixing is busy");impl_->invalidate();impl_->file=std::move(file);impl_->parent_id=std::move(parent);}
void GuiLinearUnmixing::set_matrix(LinearMixingMatrix p,std::string parent){if(impl_->busy())throw std::runtime_error("Unmixing is busy");p.validate();impl_->invalidate();impl_->rows=int(p.inputs.size());impl_->columns=int(p.components.size());impl_->draft=std::move(p);impl_->parent_id=std::move(parent);}
void GuiLinearUnmixing::set_saved_record(std::string id){if(impl_->busy())throw std::runtime_error("Unmixing is busy");impl_->saved=std::move(id);}
void GuiLinearUnmixing::set_preview(RawRegionRequest r){if(impl_->busy())throw std::runtime_error("Unmixing is busy");impl_->region=r;impl_->preview_configured=true;impl_->block.reset();impl_->clear_texture();}
void GuiLinearUnmixing::set_display_range(double a,double b){impl_->lower=a;impl_->upper=b;}
std::optional<LinearUnmixRequest> GuiLinearUnmixing::take_selection(){if(!impl_->select)return {};impl_->select=false;return impl_->request;}
std::shared_ptr<const GuiUnmixPreview> GuiLinearUnmixing::result()const{return impl_->block;}bool GuiLinearUnmixing::busy()const{return impl_->busy();}Rect GuiLinearUnmixing::bounds(Action a)const{return impl_->bounds[std::size_t(a)];}
const std::string& GuiLinearUnmixing::kept_record()const{return impl_->kept;}const std::string& GuiLinearUnmixing::error()const{return impl_->error;}const std::string& GuiLinearUnmixing::state()const{return impl_->state;}
}

#include "gui_dark_flat.hpp"
#include "gui_layout.hpp"
#include "qupathxx/raw_analysis.hpp"
#include "qupathxx/image_data_properties.hpp"
#include <GLFW/glfw3.h>
#include <future>
#include <chrono>
namespace qpx::detail {
namespace {
bool edit(const char* label,std::string& value){std::vector<char> text(std::max<std::size_t>(4096,value.size()+1024),0);std::copy(value.begin(),value.end(),text.begin());if(!ui::InputText(label,text.data(),text.size()))return false;value=text.data();return true;}
}
struct GuiDarkFlat::Impl {
    using Fields=std::array<std::shared_ptr<ImageServer>,2>;
    bool open=false,discard=false,select=false,configured=false,preview_configured=false,defaults_attempted=false,records_dirty=true;
    std::string dark,flat,kept,saved,error,state="idle",display_error,request_id;
    DarkFlatOptions options;std::uint32_t dark_series=0,flat_series=0;
    RawRegionRequest region;int scale_channel=0,pixel=0,channel=0;
    double lower=-256,upper=256;
    std::optional<RawAnalysisRequest> guard;
    std::optional<DarkFlatRequest> request;
    std::optional<DarkFlatRecipe> restoring;
    std::shared_ptr<const DarkFlatBlock> block;
    std::future<Fields> opening;
    std::future<std::shared_ptr<const DarkFlatBlock>> reading;
    std::atomic<bool> cancel{false};
    std::array<Rect,9> bounds{};
    const Document* owner=nullptr;std::weak_ptr<ImageServer> server;
    std::shared_ptr<const ImageDataProperties> properties;
    std::vector<std::string> records;
    GLuint texture=0;std::shared_ptr<const DarkFlatBlock> texture_block;
    int texture_channel=-1,width=0,height=0;double texture_lower=0,texture_upper=0;
    ~Impl(){cancel.store(true);if(opening.valid())opening.wait();if(reading.valid())reading.wait();clear_texture();}
    bool busy() const{return opening.valid() || reading.valid();}
    void clear_texture(){if(texture)glDeleteTextures(1,&texture);texture=0;texture_block.reset();}
    void invalidate(const char* why="idle"){cancel.store(true);discard=true;guard.reset();request.reset();request_id.clear();restoring.reset();block.reset();kept.clear();select=false;state=why;clear_texture();}
    template<class F>void attempt(F f){try{f();error.clear();}catch(const std::exception& e){error=e.what();}}
    void mark(Action a){auto p=ImGui::GetItemRectMin(),q=ImGui::GetItemRectMax();bounds[std::size_t(a)]={p.x,p.y,q.x-p.x,q.y-p.y};}
    void refresh(Document& doc){
        if(owner!=&doc || server.lock()!=doc.server()){
            invalidate();owner=&doc;server=doc.server();properties.reset();records.clear();saved.clear();error.clear();defaults_attempted=false;records_dirty=true;

            if(!preview_configured){region.region={0,0,0,std::min<std::int64_t>(256,doc.metadata().width),std::min<std::int64_t>(256,doc.metadata().height)};region.channels=1;}
        }
        if(open && !configured && !defaults_attempted){defaults_attempted=true;attempt([&]{const auto m=doc.server()->raw_metadata();if(!m || !m->channels || m->channels>65536)throw std::invalid_argument("Correction requires quantitative channel metadata");options.scales.assign(m->channels,1);});}
        if(guard && !guard->matches(doc))invalidate("stale");
        if(request && !request->matches(doc))invalidate("stale");
        if(records_dirty || properties!=doc.image_data_properties()){
            properties=doc.image_data_properties();records_dirty=false;
            try{auto ids=dark_flat_recipe_ids(doc);records=std::move(ids);
                if(std::find(records.begin(),records.end(),saved)==records.end())saved=records.empty()?std::string{}:records.back();
                kept=!request_id.empty() && std::find(records.begin(),records.end(),request_id)!=records.end()?request_id:std::string{};
            }catch(const std::exception& e){error=e.what();records.clear();saved.clear();kept.clear();}
        }
    }
    void load(Document& doc,bool from_saved){
        std::optional<DarkFlatRecipe> recipe;if(from_saved)recipe=load_dark_flat_recipe(doc,saved);
        if(recipe){dark=recipe->sources[1].path;flat=recipe->sources[2].path;dark_series=recipe->sources[1].series;flat_series=recipe->sources[2].series;options=recipe->options;configured=true;}
        RawAnalysisRequest captured(doc,RawRegionRequest{});invalidate();cancel.store(false);discard=false;guard=std::move(captured);restoring=std::move(recipe);
        const auto a=dark,b=flat;TiffOpenOptions d,f;d.view.series=dark_series;f.view.series=flat_series;
        opening=std::async(std::launch::async,[this,a,b,d,f]{auto first=open_tiff(a,d,&cancel);auto second=open_tiff(b,f,&cancel);return Fields{std::move(first),std::move(second)};});state="opening";
    }
    void poll(Document& doc,bool blocked){
        refresh(doc);
        if(opening.valid() && (!blocked || discard) && opening.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){
            try{auto fields=opening.get();if(!discard && guard && guard->matches(doc)){
                    auto captured=restoring?restore_dark_flat_recipe(doc,*restoring,fields[0],fields[1],&cancel):DarkFlatRequest(doc,fields[0],fields[1],options,&cancel);
                    const auto id=captured.recipe().sha256();request_id=id;request=std::move(captured);kept=std::find(records.begin(),records.end(),id)!=records.end()?id:std::string{};if(!kept.empty())saved=kept;state="ready";error.clear();restoring.reset();
                }}catch(const std::exception& e){if(!discard){error=e.what();state="failed";}}
        }
        if(reading.valid() && reading.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){
            try{auto result=reading.get();if(!discard && request && request->matches(doc)){block=std::move(result);state="preview";error.clear();pixel=channel=0;}}
            catch(const std::exception& e){if(!discard){error=e.what();state="failed";}}
        }
        if(discard && !busy() && state=="cancelling")state="cancelled";
    }
    void preview(){cancel.store(false);discard=false;block.reset();clear_texture();const auto captured=*request;const auto selected=region;
        reading=std::async(std::launch::async,[this,captured,selected]{return read_dark_flat_block(captured,selected,256u*1024*1024,&cancel);});state="reading";
    }
    void image(){
        if(!block)return;const auto& samples=block->samples();channel=std::clamp(channel,0,int(samples.channels()-1));
        if(!std::isfinite(lower) || !std::isfinite(upper) || upper<=lower || !std::isfinite(upper-lower)){display_error="Display range needs finite lower < upper";return;}display_error.clear();
        if(texture_block!=block || texture_channel!=channel || texture_lower!=lower || texture_upper!=upper){
            clear_texture();width=int(std::min<std::int64_t>(320,samples.width()));height=int(std::min<std::int64_t>(320,samples.height()));
            std::vector<unsigned char> rgba(std::size_t(width)*height*4,0);const auto& values=std::get<std::vector<double>>(samples.storage());
            for(int y=0;y<height;++y)for(int x=0;x<width;++x){const auto sx=(2*std::int64_t(x)+1)*samples.width()/(2*width),sy=(2*std::int64_t(y)+1)*samples.height()/(2*height);const auto at=std::size_t(sy*samples.width()+sx),p=std::size_t(y*width+x)*4;if(!samples.valid()[at])continue;
                const auto value=values[at*samples.channels()+std::size_t(channel)];if(std::isfinite(value)){const auto gray=static_cast<unsigned char>(255*std::clamp((value-lower)/(upper-lower),0.0,1.0));rgba[p]=rgba[p+1]=rgba[p+2]=gray;}else{rgba[p]=255;rgba[p+2]=255;}rgba[p+3]=255;
            }
            GLint old=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&old);glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());glBindTexture(GL_TEXTURE_2D,GLuint(old));
            texture_block=block;texture_channel=channel;texture_lower=lower;texture_upper=upper;
        }
        if(texture){const auto scale=std::min(16.f,320.f/std::max(width,height));ImGui::Image(ImTextureID(texture),{width*scale,height*scale});}
    }
    void draw(Document& doc,bool blocked){
        poll(doc,blocked);bounds={};if(!open)return;
        ui::SetNextToolWindow("Dark/flat correction",{620,850});if(ImGui::Begin("Dark/flat correction",&open)){
            ui::Text("Correction state: %s",state.c_str());ui::TextWrapped("Derived samples: (signal-dark)/(flat-dark)*scale. Original samples stay unchanged.");
            ImGui::BeginDisabled(blocked || busy());
            if(ui::Button("Load correction fields"))attempt([&]{load(doc,false);});mark(Action::Load);
            ImGui::BeginDisabled(!request);if(ui::Button("Preview corrected samples"))attempt([&]{preview();});mark(Action::Preview);
            if(ui::Button("Keep correction recipe"))attempt([&]{kept=keep_dark_flat_recipe(doc,*request,&cancel);saved=kept;records_dirty=true;});mark(Action::Keep);
            ImGui::BeginDisabled(kept.empty());if(ui::Button("Use for fluorescence"))attempt([&]{request->require_current(doc);select=true;open=false;});mark(Action::Use);ImGui::EndDisabled();ImGui::EndDisabled();ImGui::EndDisabled();
            ImGui::BeginDisabled(!busy());if(ui::Button("Cancel correction"))invalidate("cancelling");mark(Action::Cancel);ImGui::EndDisabled();
            if(!kept.empty())ui::TextWrapped("Retained correction: %s",kept.c_str());
            if(!error.empty())ui::TextWrapped("Correction: %s",error.c_str());
            ImGui::BeginDisabled(blocked || busy());
            if(ui::BeginCombo("Saved correction",saved.empty()?"None":saved.c_str())){for(const auto& id:records)if(ImGui::Selectable(id.c_str(),id==saved))saved=id;ui::EndCombo();}
            ImGui::BeginDisabled(saved.empty());if(ui::Button("Load saved correction"))attempt([&]{load(doc,true);});mark(Action::LoadSaved);
            if(ui::Button("Remove correction recipe"))attempt([&]{const auto id=saved;remove_dark_flat_recipe(doc,id);if(kept==id)kept.clear();records_dirty=true;});mark(Action::Remove);ImGui::EndDisabled();
            const bool fields=ui::CollapsingHeader("Correction fields and normalization",ImGuiTreeNodeFlags_DefaultOpen);mark(Action::Fields);if(fields){
                bool changed=edit("Dark TIFF path",dark);changed=edit("Flat TIFF path",flat)||changed;changed=ui::InputScalar("Dark series",ImGuiDataType_U32,&dark_series)||changed;changed=ui::InputScalar("Flat series",ImGuiDataType_U32,&flat_series)||changed;
                if(ui::Button("Reset normalization to source channels"))attempt([&]{const auto m=doc.server()->raw_metadata();if(!m || !m->channels || m->channels>65536)throw std::invalid_argument("Invalid source channel count");options.scales.assign(m->channels,1);configured=true;invalidate();});
                ui::Text("Explicit normalization channels: %zu",options.scales.size());if(!options.scales.empty()){ui::InputInt("Normalize original channel",&scale_channel);scale_channel=std::clamp(scale_channel,0,int(options.scales.size()-1));changed=ui::InputDouble("Source-sample scale",&options.scales[std::size_t(scale_channel)],0,0,"%.17g")||changed;}
                changed=ui::InputDouble("Minimum denominator",&options.minimum_denominator,0,0,"%.17g")||changed;
                if(changed){configured=true;invalidate();}
            }
            if(ui::CollapsingHeader("Correction preview region")){
                bool changed=false;changed=ui::InputScalar("Preview X",ImGuiDataType_S64,&region.region.x)||changed;changed=ui::InputScalar("Preview Y",ImGuiDataType_S64,&region.region.y)||changed;
                changed=ui::InputScalar("Preview width",ImGuiDataType_S64,&region.region.width)||changed;changed=ui::InputScalar("Preview height",ImGuiDataType_S64,&region.region.height)||changed;
                changed=ui::InputScalar("Preview first channel",ImGuiDataType_U32,&region.first_channel)||changed;changed=ui::InputScalar("Preview channels",ImGuiDataType_U32,&region.channels)||changed;changed=ui::InputScalar("Preview Z",ImGuiDataType_U32,&region.z)||changed;changed=ui::InputScalar("Preview time",ImGuiDataType_U32,&region.t)||changed;
                if(changed){preview_configured=true;block.reset();clear_texture();}
            }ImGui::EndDisabled();
            if(block){const auto& r=block->region();ui::Text("Corrected domain: qpx.dark-flat-corrected-samples/1");ui::Text("Crop: %lld,%lld / %lld x %lld / Z%u T%u",(long long)r.region.x,(long long)r.region.y,(long long)r.region.width,(long long)r.region.height,r.z,r.t);
                ui::Text("Valid source pixels: %llu",(unsigned long long)block->valid_pixels());ui::InputInt("Inspect preview channel",&channel);channel=std::clamp(channel,0,int(r.channels-1));ui::Text("Original channel %u: %llu finite / %llu nonfinite",r.first_channel+std::uint32_t(channel),(unsigned long long)block->finite_samples()[std::size_t(channel)],(unsigned long long)block->nonfinite_samples()[std::size_t(channel)]);
                ui::InputInt("Inspect preview pixel",&pixel);pixel=std::clamp(pixel,0,int(block->samples().valid().size()-1));const auto at=std::size_t(pixel);if(block->samples().valid()[at])ui::Text("Corrected value: %.17g",std::get<std::vector<double>>(block->samples().storage())[at*r.channels+std::size_t(channel)]);else ui::TextUnformatted("Corrected value: absent source pixel");
                const bool show_image=ui::CollapsingHeader("Corrected image preview");mark(Action::Image);if(show_image){ui::InputDouble("Display lower",&lower);ui::InputDouble("Display upper",&upper);ui::TextWrapped("Presentation only, sampled to320 pixels per axis. Nonfinite magenta; absent transparent. Signed analysis values are preserved.");image();if(!display_error.empty())ui::TextWrapped("%s",display_error.c_str());}
            }
        }ImGui::End();
    }
};
GuiDarkFlat::GuiDarkFlat():impl_(std::make_unique<Impl>()){}
GuiDarkFlat::~GuiDarkFlat()=default;
void GuiDarkFlat::open(){impl_->open=true;}
void GuiDarkFlat::close(){impl_->open=false;}
void GuiDarkFlat::poll(Document& doc,bool blocked){impl_->poll(doc,blocked);}
void GuiDarkFlat::draw(Document& doc,bool blocked){impl_->draw(doc,blocked);}
void GuiDarkFlat::set_fields(std::string dark,std::string flat,DarkFlatOptions options,std::uint32_t ds,std::uint32_t fs){if(impl_->busy())throw std::runtime_error("Correction is busy");options.validate();impl_->invalidate();impl_->dark=std::move(dark);impl_->flat=std::move(flat);impl_->options=std::move(options);impl_->dark_series=ds;impl_->flat_series=fs;impl_->configured=true;}
void GuiDarkFlat::set_preview(RawRegionRequest region){if(impl_->busy())throw std::runtime_error("Correction is busy");impl_->region=region;impl_->preview_configured=true;impl_->block.reset();impl_->clear_texture();}
std::optional<DarkFlatRequest> GuiDarkFlat::take_selection(){if(!impl_->select)return {};impl_->select=false;return impl_->request;}
std::shared_ptr<const DarkFlatBlock> GuiDarkFlat::result()const{return impl_->block;}
bool GuiDarkFlat::busy()const{return impl_->busy();}
Rect GuiDarkFlat::bounds(Action a)const{return impl_->bounds[std::size_t(a)];}
const std::string& GuiDarkFlat::kept_record()const{return impl_->kept;}
const std::string& GuiDarkFlat::error()const{return impl_->error;}
const std::string& GuiDarkFlat::state()const{return impl_->state;}
}

#pragma once
#include "qupathxx/pixel_stream_file.hpp"
#include "gui_pixel_preview.hpp"
#include "gui_layout.hpp"
#include "utf8_text.hpp"
#include <array>
#include <cstring>
#include <functional>
namespace qpx::detail {
class GuiPixelStreamBrowser {
    PixelStreamReadJob job_;PixelStreamReadStats stats_;GuiPixelPreview preview_;
    std::shared_ptr<const PixelPredictionResult> palette_source_;
    std::vector<std::uint32_t> palette_;std::array<char,4096> path_{};
    std::array<Rect,11> targets_{};Rect image_bounds_{0,0,0,0};
    std::uint64_t index_=0,max_bytes_=1ULL<<40;bool open_=false,visible_=false;std::string error_;
    using Records=std::map<std::uint64_t,std::shared_ptr<const PixelStreamRecord>>;
    std::shared_ptr<const char> identity_;std::uint64_t revision_=0,selected_=0,kept_=0;
    Records::const_iterator cursor_;std::vector<std::shared_ptr<const PixelStreamRecord>> records_;
    bool complete_=false;std::size_t max_step_=0;
    static std::size_t index(GuiInputTarget t) {return static_cast<std::size_t>(t)-static_cast<std::size_t>(GuiInputTarget::OpenPixelStreamFile);}
    template<class Work> void run(Work work) {try {work();error_.clear();update();}catch(const std::exception& e) {error_=e.what();}}
public:
    explicit GuiPixelStreamBrowser(const GuiOptions& o):index_(o.pixel_stream_read_index),max_bytes_(o.pixel_stream_read_max_bytes),open_(o.pixel_stream_browser_open) {
        const auto& path=o.pixel_stream_read_path.empty()?o.pixel_stream_path:o.pixel_stream_read_path;bounded_utf8(path,path_.size()-1);std::memcpy(path_.data(),path.data(),path.size());
        if(max_bytes_<128 || max_bytes_>(1ULL<<46)) throw std::invalid_argument("invalid pixel stream file byte limit");
    }
    void open() {open_=true;}
    void catalog(const Document& doc,const Records& records,const std::shared_ptr<const char>& identity) {
        if(identity_!=identity || revision_!=doc.revision()) {if(identity_!=identity) {selected_=kept_=0;}identity_=identity;revision_=doc.revision();cursor_=records.cbegin();records_.clear();complete_=false;if(selected_ && !records.count(selected_)) selected_=0;if(!selected_ && !records.empty()) selected_=records.begin()->first;}
        std::size_t step=0;while(open_ && !complete_ && step<1024) {if(cursor_==records.cend()) {complete_=true;break;}records_.push_back((cursor_++)->second);++step;}max_step_=std::max(max_step_,step);
    }
    std::uint64_t kept() const {return kept_;}std::uint64_t selected() const {return selected_;}
    bool catalog_complete() const {return complete_;}std::size_t catalog_max_step() const {return max_step_;}

    void cancel() {job_.cancel();}
    bool busy() const {return job_.busy();}
    bool settled() const {return !busy() && (!open_ || !visible_ || !stats_.block || preview_.ready() || !preview_.error().empty());}
    const PixelStreamReadStats& stats() const {return stats_;}
    const GuiPixelPreview& preview() const {return preview_;}
    const Rect& preview_bounds() const {return image_bounds_;}
    Rect bounds(GuiInputTarget t) const {return targets_.at(index(t));}
    std::string error() const {return !error_.empty()?error_:!stats_.error.empty()?stats_.error:preview_.error();}
    void update() {
        stats_=job_.poll();
        if(palette_source_!=stats_.block) {
            palette_source_=stats_.block;palette_.clear();
            static constexpr std::uint32_t colors[]={0x4c78a8,0xf58518,0xe45756,0x72b7b2,0x54a24b,0xeeca3b,0xb279a2,0xff9da6};
            if(stats_.block) for(std::size_t i=0;i<stats_.block->model().classes().size();++i) {const auto c=colors[i%8];palette_.push_back(((c>>16)&255)|(c&0xff00)|((c&255)<<16)|0xff000000u);}
        }
        preview_.update(open_?stats_.block:nullptr,palette_,false,1024*1024);
    }
    void finish(bool finite) {if(!finite) job_.cancel();if(job_.busy()) job_.wait();update();if(open_) preview_.update(stats_.block,palette_,true,1024*1024);}
    void draw(Document& doc,bool blocked,const std::function<void(GuiInputTarget)>& mark) {
        image_bounds_={0,0,0,0};visible_=false;if(!open_) return;
        const auto* vp=ImGui::GetMainViewport();const auto scale=ImGui::GetFontSize()/13.f;
        ImGui::SetNextWindowSize({std::min(650.f*scale,vp->WorkSize.x*.88f),std::min(680.f*scale,vp->WorkSize.y*.88f)},ImGuiCond_Appearing);
        ImGui::SetNextWindowPos({vp->WorkPos.x+vp->WorkSize.x*.55f,vp->WorkPos.y+vp->WorkSize.y*.5f},ImGuiCond_Appearing,{.5f,.5f});
        ImGui::SetNextWindowSizeConstraints({std::min(280.f*scale,vp->WorkSize.x),std::min(180.f*scale,vp->WorkSize.y)},vp->WorkSize);
        const bool visible=visible_=ImGui::Begin("Saved pixel stream files",&open_);
        if(!open_) {if(busy()) job_.cancel();else job_.close();update();}
        if(!visible || !open_) {ImGui::End();return;}
        auto button=[&](const char* label,GuiInputTarget target,bool is_disabled) {ImGui::BeginDisabled(is_disabled);const bool clicked=ui::Button(label);const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();targets_[index(target)]={a.x,a.y,b.x-a.x,b.y-a.y};mark(target);ImGui::EndDisabled();return clicked;};
        auto disabled=[&] {return blocked || busy();};auto loaded=[&] {return bool(stats_.declared_summary) && stats_.state!="cancelled" && stats_.state!="cancelling";};
        ImGui::BeginDisabled(disabled());ui::InputText("Saved stream file",path_.data(),path_.size());ui::InputScalar("Block index (zero based)",ImGuiDataType_U64,&index_);ImGui::EndDisabled();
        if(ImGui::CollapsingHeader("File size limit")) {ImGui::BeginDisabled(disabled());std::uint64_t gib=max_bytes_/(1ULL<<30);if(ui::InputScalar("Maximum file size (GiB)",ImGuiDataType_U64,&gib)) max_bytes_=std::clamp<std::uint64_t>(gib,1,65536)*(1ULL<<30);ImGui::EndDisabled();}
        if(button("Open stream file",GuiInputTarget::OpenPixelStreamFile,disabled() || !path_[0])) run([&] {job_.open(path_.data(),index_,max_bytes_);});ImGui::SameLine();
        if(button("Read block",GuiInputTarget::ReadPixelStreamBlock,disabled() || !loaded())) run([&] {job_.read(index_);});
        if(button("Previous block",GuiInputTarget::PreviousPixelStreamBlock,disabled() || !loaded() || !stats_.block_index || stats_.block_index>=stats_.declared_summary->blocks)) run([&] {index_=stats_.block_index-1;job_.read(index_);});ImGui::SameLine();
        if(button("Next block",GuiInputTarget::NextPixelStreamBlock,disabled() || !loaded() || stats_.block_index>=stats_.declared_summary->blocks-1)) run([&] {index_=stats_.block_index+1;job_.read(index_);});
        if(button("Verify all blocks",GuiInputTarget::VerifyPixelStreamFile,disabled() || !loaded())) run([&] {job_.verify();});ImGui::SameLine();
        if(button("Cancel reading",GuiInputTarget::CancelPixelStreamRead,blocked || !busy())) run([&] {job_.cancel();});ImGui::SameLine();
        if(button("Close file",GuiInputTarget::ClosePixelStreamFile,disabled() || !stats_.declared_summary)) run([&] {job_.close();});
        ImGui::TextWrapped("%s%s",stats_.state.c_str(),stats_.verified?" / full file verified":" / whole-file totals not verified");
        if(!error().empty()) ImGui::TextWrapped("%s",error().c_str());
        if(stats_.declared_summary) {const auto& summary=*stats_.declared_summary;ImGui::TextWrapped("%llu blocks / %llu bytes / %s",static_cast<unsigned long long>(summary.blocks),static_cast<unsigned long long>(summary.bytes),stats_.windowed?"windowed target":"full target");ImGui::TextWrapped("%llu valid / %llu classified pixels (%s totals)",static_cast<unsigned long long>(summary.valid_pixels),static_cast<unsigned long long>(summary.classified_pixels),stats_.verified?"verified":"declared");}
        const bool compatible=stats_.verified && stats_.link && stats_.link->source_width()==doc.metadata().width && stats_.link->source_height()==doc.metadata().height;
        if(button("Keep verified stream link",GuiInputTarget::KeepPixelStreamLink,disabled() || !compatible)) run([&] {kept_=selected_=doc.keep_pixel_stream(stats_.link);});
        std::shared_ptr<const PixelStreamRecord> selected;if(selected_) {try {selected=doc.pixel_stream_record(selected_);}catch(const std::out_of_range&) {selected_=0;}}
        ImGui::BeginDisabled(disabled());const auto label=selected?"#"+std::to_string(selected->id)+" / "+selected->name:std::string("No saved stream links");
        if(ui::BeginCombo("Project stream links",label.c_str())) {ImGuiListClipper clipper;clipper.Begin(static_cast<int>(records_.size()));while(clipper.Step()) for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i) {ImGui::PushID(i);const auto& r=records_[i];const auto name="#"+std::to_string(r->id)+" / "+r->name;if(ImGui::Selectable(name.c_str(),r->id==selected_)) selected_=r->id;ImGui::PopID();}ui::EndCombo();}ImGui::EndDisabled();
        if(button("Open saved stream",GuiInputTarget::OpenPixelStreamRecord,disabled() || !selected)) run([&] {index_=0;const auto path=selected->link->path();bounded_utf8(path,path_.size()-1);path_.fill(0);std::memcpy(path_.data(),path.data(),path.size());job_.open_link(selected->link,index_,max_bytes_);});ImGui::SameLine();
        if(button("Relink selected stream",GuiInputTarget::RelinkPixelStreamRecord,disabled() || !selected || !stats_.verified || !stats_.link || !selected->link->same_content(*stats_.link))) run([&] {doc.relink_pixel_stream_record(selected_,stats_.link);});ImGui::SameLine();
        if(button("Remove stream link",GuiInputTarget::RemovePixelStreamRecord,disabled() || !selected)) run([&] {doc.remove_pixel_stream_record(selected_);selected_=0;});
        if(selected && ImGui::CollapsingHeader("Saved stream link details")) {ImGui::TextWrapped("%s",selected->link->path().c_str());ImGui::TextWrapped("Historical content SHA-256: %s",selected->link->summary().sha256.c_str());ImGui::TextWrapped("Project loading checks metadata only. Opening this link verifies the complete external file. Removing a link does not delete its file.");}
        if(stats_.block) {
            const auto& block=*stats_.block;ImGui::TextWrapped("Historical block %llu: %lld x %lld at level %d",static_cast<unsigned long long>(stats_.block_index),static_cast<long long>(block.width()),static_cast<long long>(block.height()),block.level());
            ImGui::TextWrapped("Labels only. This preview is separate from the current image.");
            preview_.update(stats_.block,palette_,true,1024*1024);
            const ImVec2 size{std::max(1.f,ImGui::GetContentRegionAvail().x),std::max(64.f,std::min(240.f*scale,ImGui::GetContentRegionAvail().y-24.f*scale))};const auto at=ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("Historical block labels",size);auto* draw=ImGui::GetWindowDrawList();draw->AddRectFilled(at,{at.x+size.x,at.y+size.y},IM_COL32(30,32,36,255));preview_.draw_local(draw,at,size);
            const auto zoom=std::min(double(size.x)/block.width(),double(size.y)/block.height());image_bounds_={at.x+(size.x-block.width()*zoom)*.5,at.y+(size.y-block.height()*zoom)*.5,block.width()*zoom,block.height()*zoom};
            if(ImGui::CollapsingHeader("Original source and class counts")) {
                const auto& p=*block.provenance();ImGui::TextWrapped("%lld x %lld / %s",static_cast<long long>(p.source.width),static_cast<long long>(p.source.height),p.reader.c_str());
                ImGui::TextWrapped("%s",p.locator.c_str());ImGui::TextWrapped("Numeric SHA-256: %s",block.sha256().c_str());
                ImGui::TextWrapped("%llu valid / %llu classified pixels in this block",static_cast<unsigned long long>(block.valid_pixels()),static_cast<unsigned long long>(block.classified_pixels()));
                const auto grid=block.level_bounds();ImGui::TextWrapped("Grid position: %.0f, %.0f / downsample %.6g",grid.x,grid.y,block.downsample());
                if(p.effective_pixel_width_um && p.effective_pixel_height_um) ImGui::TextWrapped("Sampling: %.6g x %.6g um",*p.effective_pixel_width_um,*p.effective_pixel_height_um);
                ImGui::TextWrapped("Class: this block / whole file (%s)",stats_.verified?"verified":"declared");
                ImGuiListClipper clipper;clipper.Begin(static_cast<int>(block.model().classes().size()));while(clipper.Step()) for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i) {
                    auto label=block.model().classes()[i];for(auto& c:label) if(c=='\n' || c=='\r' || c=='\t') c=' ';
                    const auto color=palette_[i];ImGui::PushID(i);ImGui::ColorButton("Class color",{float(color&255)/255,float((color>>8)&255)/255,float((color>>16)&255)/255,1},ImGuiColorEditFlags_NoTooltip,{ImGui::GetFontSize(),ImGui::GetFontSize()});ImGui::SameLine();
                    ImGui::Text("%s: %llu / %llu",label.c_str(),static_cast<unsigned long long>(block.class_counts()[i]),static_cast<unsigned long long>(stats_.declared_summary->class_counts[i]));ImGui::PopID();
                }
            }
        }
        ImGui::End();
    }
};
}

#pragma once
#include "gui_layout.hpp"
#include "gui_shell_open.hpp"
#include "atomic_export.hpp"
#include <array>
#include <ctime>
#include <functional>
#include <memory>
#include <string_view>

namespace qpx::ui {
// File identities keep their native spelling. Only presentation removes the
// Windows extended-length prefix, including the corresponding UNC spelling.
inline std::string notice_path(const std::string& path) {
    if(path.empty())return {};
    auto value=detail::export_path(path);
    if(value.size()>=8 && value.compare(0,4,"\\\\?\\")==0 && (value[4]=='U' || value[4]=='u') && (value[5]=='N' || value[5]=='n') && (value[6]=='C' || value[6]=='c') && value[7]=='\\')value="\\\\"+value.substr(8);
    else if(value.compare(0,4,"\\\\?\\")==0)value.erase(0,4);
    return value;
}
enum class Severity {Info,Success,Warning,Error};
struct Notice {Severity severity;std::string source,message,path;bool message_key=false;};
inline Notice exception_notice(std::string source,const std::exception& exception,std::string path={}) {
    if(const auto* message=dynamic_cast<const NoticeMessageKey*>(&exception))return {Severity::Error,std::move(source),message->key,std::move(path),true};
    return {Severity::Error,std::move(source),exception.what(),std::move(path)};
}
class Notifications {
public:
    struct Entry {Notice notice;std::string text,time;std::uint64_t sequence=0;};
private:
    std::array<std::shared_ptr<const Entry>,50> entries_{};
    std::shared_ptr<const Entry> current_;
    std::size_t count_=0,next_=0;
    std::uint64_t sequence_=0;
    bool activity_open_=false;
    static void tooltip(const char* text,const char* hint=nullptr) {
        ImGui::BeginTooltip();ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+std::min(50*ImGui::GetFontSize(),ImGui::GetMainViewport()->WorkSize.x*.8f));
        ui::DataText(text);if(hint)ui::DataText(hint);
        ImGui::PopTextWrapPos();ImGui::EndTooltip();
    }
    static const char* severity_key(Severity s) {
        switch(s){case Severity::Info:return "Information";case Severity::Success:return "Success";case Severity::Warning:return "Warning";default:return "Error";}
    }
    // Captions set the natural width; narrow windows can clip the caption while
    // retaining the full caption and explanation on hover.
    static bool control(const char* key,float width,const char* tip) {
        width=std::max(1.f,std::min(width,ImGui::CalcTextSize(i18n::text(key)).x+2*ImGui::GetStyle().FramePadding.x));
        const auto pos=ImGui::GetCursorScreenPos();const float height=ImGui::GetFrameHeight();
        const bool pressed=ImGui::InvisibleButton(key,{std::max(1.f,width),height});
        observe(key);trace_layout(LayoutKind::TextButton,key);
        auto* draw=ImGui::GetWindowDrawList();draw->AddRectFilled(pos,{pos.x+width,pos.y+height},ImGui::GetColorU32(ImGui::IsItemHovered()?ImGuiCol_ButtonHovered:ImGuiCol_Button));
        const auto padding=ImGui::GetStyle().FramePadding;
        ImGui::RenderTextClipped({pos.x+padding.x,pos.y+padding.y},{pos.x+width-padding.x,pos.y+height},i18n::text(key),nullptr,nullptr);
        if(ImGui::IsItemHovered())tooltip(i18n::text(key),i18n::text(tip));
        return pressed;
    }
    static void clipped(const char* text,float width,bool log_summary=false) {
        const auto pos=ImGui::GetCursorScreenPos();const float height=ImGui::GetFrameHeight();
        ImGui::Dummy({std::max(1.f,width),height});
        observe();trace_layout(LayoutKind::Caption,text);
        const ImVec4 clip{pos.x,pos.y,pos.x+width,pos.y+height};
        // Keep the existing summary available to ImGui's explicit text capture.
        if(log_summary && ImGui::GetCurrentContext()->LogEnabled)ImGui::LogRenderedText(&pos,text);
        ImGui::GetWindowDrawList()->AddText(nullptr,0,{pos.x,pos.y+ImGui::GetStyle().FramePadding.y},ImGui::GetColorU32(ImGuiCol_Text),text,nullptr,0,&clip);
        if(ImGui::IsItemHovered())tooltip(text);
    }
    void path_actions(const Entry& entry,float width,float folder_width=-1) {
        if(control("Copy path",width,"Copy the full path to the clipboard."))ImGui::SetClipboardText(entry.notice.path.c_str());
        ImGui::SameLine();
        if(control("Open folder",folder_width<0?width:folder_width,"Open the containing folder.")) {
            try {const auto p=std::filesystem::u8path(entry.notice.path);open_in_shell((std::filesystem::is_directory(p)?p:p.parent_path()).u8string());}
            catch(const std::exception& e){post(exception_notice(entry.notice.source,e,entry.notice.path));}
        }
    }
public:
    static ImVec4 color(Severity s) {
        const auto& p=design::colors();return design::color(s==Severity::Error?p.error:s==Severity::Warning?p.warning:s==Severity::Success?p.success:p.info);
    }
    void post(Notice notice) {
        const auto source=std::find_if(std::begin(i18n::catalogue),std::end(i18n::catalogue),[&](const auto& row){return notice.source==row.english;});
        if(source==std::end(i18n::catalogue))throw std::invalid_argument("Notification source has no catalogue row");
        auto entry=std::make_shared<Entry>();
        try {notice.path=notice_path(notice.path);}catch(const std::exception&){notice.path.clear();}
        entry->notice=std::move(notice);entry->sequence=++sequence_;
        const auto now=std::time(nullptr);std::tm local{};
#ifdef _WIN32
        localtime_s(&local,&now);
#else
        localtime_r(&now,&local);
#endif
        char time[16];std::strftime(time,sizeof(time),"%H:%M:%S",&local);entry->time=time;
        const std::string_view source_text=i18n::text(entry->notice.source.c_str());
        const std::string_view message_text=entry->notice.message_key?std::string_view(i18n::text(entry->notice.message.c_str())):std::string_view(entry->notice.message);
        entry->text.reserve(source_text.size()+2+message_text.size()+(entry->notice.path.empty()?0:3+entry->notice.path.size()));
        entry->text.append(source_text);entry->text+=": ";entry->text.append(message_text);
        if(!entry->notice.path.empty()){entry->text+=" | ";entry->text+=entry->notice.path;}
        if(!current_ || current_->notice.severity!=Severity::Error || entry->notice.severity==Severity::Error || entry->notice.source==current_->notice.source)current_=entry;
        entries_[next_]=std::move(entry);next_=(next_+1)%entries_.size();count_=std::min(count_+1,entries_.size());
    }
    void resolve(std::string_view source) {
        // Quiet foreground edits end only their own standing error.
        if(current_ && current_->notice.severity==Severity::Error && current_->notice.source==source)dismiss_errors();
    }
    void dismiss_errors() {
        // Dismissal ends the standing error, while history remains inspectable.
        current_.reset();
        for(std::size_t i=0;i<count_;++i)if(newest(i).notice.severity!=Severity::Error){current_=entries_[(next_+entries_.size()-1-i)%entries_.size()];break;}
    }
    const Entry* current() const {return current_.get();}
    std::size_t size() const {return count_;}
    const Entry& newest(std::size_t i) const {return *entries_.at((next_+entries_.size()-1-i)%entries_.size());}
    std::uint64_t posted() const {return sequence_;}
    void draw_status_bar(const char* summary=nullptr) {
        const auto entry=current_;const float gap=ImGui::GetStyle().ItemSpacing.x;
        const bool error=entry && entry->notice.severity==Severity::Error,path=entry && !entry->notice.path.empty();
        const char* keys[]={"Activity","Dismiss error","Copy path","Open folder"};std::array<float,4> widths{};
        float controls_width=0;int controls=0;
        for(int i=0;i<4;++i)if(i==0 || (i==1 && error) || (i>=2 && path)) {
            widths[i]=ImGui::CalcTextSize(i18n::text(keys[i])).x+2*ImGui::GetStyle().FramePadding.x;controls_width+=widths[i];++controls;
        }
        const float total=std::max(1.f,ImGui::GetContentRegionAvail().x),gaps=gap*(controls+(summary?1:0));
        const float notice_min=std::min(entry?design::metrics.field_min_width*design::scale():ImGui::CalcTextSize(i18n::text("Ready")).x,std::max(1.f,total-gaps-controls-(summary?1:0)));
        const float summary_width=summary?std::max(1.f,std::min(ImGui::CalcTextSize(summary).x,total-gaps-controls_width-notice_min)):0;
        const float compression=std::min(1.f,std::max(float(controls),total-gaps-summary_width-notice_min)/std::max(1.f,controls_width));
        for(auto& width:widths)width*=compression;
        if(summary){clipped(summary,summary_width,true);ImGui::SameLine();}
        if(control("Activity",widths[0],"Show this session's last 50 notices."))activity_open_=true;
        ImGui::SameLine();
        if(error){if(control("Dismiss error",widths[1],"Dismiss the standing error."))dismiss_errors();ImGui::SameLine();}
        if(path){path_actions(*entry,widths[2],widths[3]);ImGui::SameLine();}
        if(entry){ImGui::PushStyleColor(ImGuiCol_Text,color(entry->notice.severity));clipped(entry->text.c_str(),ImGui::GetContentRegionAvail().x);ImGui::PopStyleColor();}
        else clipped(i18n::text("Ready"),ImGui::GetContentRegionAvail().x);
    }
    void draw_activity(bool* open) {
        if(!*open)return;
        SetNextToolWindow("Activity",{850,500});
        if(ImGui::Begin(i18n::label("Activity").c_str(),open,ImGuiWindowFlags_NoFocusOnAppearing)) {
            const float height=ImGui::GetFrameHeight()*2+ImGui::GetStyle().ItemSpacing.y*2;
            ImGuiListClipper clipper;clipper.Begin(static_cast<int>(count_),height);
            while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i) {
                const auto entry=entries_[(next_+entries_.size()-1-i)%entries_.size()];ImGui::PushID(static_cast<int>(entry->sequence));
                const auto y=ImGui::GetCursorPosY();ImGui::PushStyleColor(ImGuiCol_Text,color(entry->notice.severity));
                ui::DataTextf("%s  %s",entry->time.c_str(),i18n::text(severity_key(entry->notice.severity)));ImGui::SameLine();
                clipped(entry->text.c_str(),ImGui::GetContentRegionAvail().x);ImGui::PopStyleColor();
                if(!entry->notice.path.empty())path_actions(*entry,std::min(170.f*design::scale(),ImGui::GetContentRegionAvail().x*.45f));
                ImGui::SetCursorPosY(y+height-ImGui::GetStyle().ItemSpacing.y);ImGui::Dummy({0,0});ImGui::PopID();
            }
        }ImGui::End();
    }
    void draw_activity(){draw_activity(&activity_open_);}
};
// Each observer belongs to a particular operation. It compares native state,
// never native prose, and does not change a job's lifetime or result.
struct NoticeJob {
    std::string state;
    std::uint64_t generation=0;
    void observe(Notifications& notices,const char* source,std::string_view next,bool success,bool failed,std::string_view message={},std::string_view path={},std::uint64_t next_generation=0,bool message_key=false) {
        if(state==next && generation==next_generation)return;state.assign(next);generation=next_generation;
        if(failed)notices.post({Severity::Error,source,message.empty()?std::string("The operation failed."):std::string(message),std::string(path),message.empty() || message_key});
        else if(success)notices.post({Severity::Success,source,"The operation completed.",std::string(path),true});
    }
};
struct NotificationsTrace {
    std::function<void(Notifications&)> before_frame,after_frame;
};
inline thread_local NotificationsTrace* notifications_trace=nullptr;
}

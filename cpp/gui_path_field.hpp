#pragma once
#include "gui_import_picker.hpp"
#include "utf8_text.hpp"
#include <functional>
#include <memory>
#include <string_view>
#include <utility>

namespace qpx::ui {
enum class PathRole {OpenFile,NewFile,OpenFolder,NewFolder};
struct PathSpec {const char* label;PathRole role;std::vector<std::string> extensions;const char* suggestion="";const char* hint="";bool enter_to_submit=false;bool unscoped_id=false;bool literal_hint=false;};
struct PathContext {std::string project_directory;std::string* last_directory=nullptr;};
struct PathState {std::string resolved;bool exists=false,valid=false;std::string problem;};
// One shared process-directory observation per ImGui frame. Headless callers
// have no frame and observe their current process directory on demand.
inline thread_local ImGuiContext* path_process_context=nullptr;
inline thread_local int path_process_frame=-2;
inline thread_local std::string path_process_value;
inline thread_local std::uint64_t path_process_reads=0;
inline const std::string& path_process_directory() {
    auto* context=ImGui::GetCurrentContext();
    if(!context || context!=path_process_context || context->FrameCount!=path_process_frame) {
        path_process_value=std::filesystem::current_path().u8string();++path_process_reads;
        path_process_context=context;path_process_frame=context?context->FrameCount:-2;
    }
    return path_process_value;
}
inline std::string resolve_user_path(std::string_view text,const PathContext& context) {
    if(text.empty())return {};
    detail::bounded_utf8(std::string(text),32768);
    if(text.find('\0')!=std::string_view::npos)throw std::invalid_argument("Invalid path text");
    const auto path=std::filesystem::u8path(text.begin(),text.end());
    if(path.is_absolute())return path.u8string();
    const auto base=!context.project_directory.empty()?std::filesystem::u8path(context.project_directory):
        context.last_directory && !context.last_directory->empty()?std::filesystem::u8path(*context.last_directory):std::filesystem::u8path(path_process_directory());
    return (base/path).u8string();
}
class PathField {
    friend struct PathFieldTestAccess;
    PathSpec spec_;
    std::array<char,32769> text_{};
    PathState state_;
    std::unique_ptr<detail::GuiImportPicker> picker_;
    bool hovered_=false,refreshed_=false,option_=false,focus_=false,submitted_=false,picked_=false,picker_active_=false,pending_edit_=false;
    int drawn_frame_=-2,picker_frame_=-2;ImRect input_bounds_{},browse_bounds_{};ImGuiID modal_=0;
    ImGuiContext* input_context_=nullptr;ImGuiID input_id_=0;
    ImGuiID input_root_window_=0;
    std::uint64_t refresh_count_=0;int draw_count_frame_=-2,draw_count_=0;
    bool clipped_=false,tooltip_hovered_=false,tooltip_requested_=false;
    std::string notice_;
    std::string project_,last_,process_;
    bool folder_role() const {return spec_.role==PathRole::OpenFolder || spec_.role==PathRole::NewFolder;}
    PathContext cached_context() {return {project_,&last_};}
public:
    explicit PathField(PathSpec spec):spec_(std::move(spec)) {set("");}
    void refresh(const PathContext& context) {
        ++refresh_count_;
        // Snapshot values: an external last-directory string can change in place.
        const auto project=context.project_directory,last=context.last_directory?*context.last_directory:std::string{};
        state_={};project_=project;last_=last;refreshed_=true;
        try {
            process_=option_ || (project_.empty() && last_.empty())?path_process_directory():std::string{};
            auto ctx=option_?PathContext{}:cached_context();const auto resolved=resolve_user_path(text_.data(),ctx);
            if(resolved.empty()) {state_.problem="Choose a path";return;}
            const auto path=std::filesystem::absolute(std::filesystem::u8path(resolved)).lexically_normal();state_.resolved=path.u8string();
            state_.exists=std::filesystem::exists(path);
            if(state_.exists && (folder_role()?!std::filesystem::is_directory(path):!std::filesystem::is_regular_file(path)))state_.problem=folder_role()?"Choose a folder path":"Choose a file path";
            else if(!state_.exists && (spec_.role==PathRole::OpenFile || spec_.role==PathRole::OpenFolder))state_.problem=folder_role()?"Choose an existing folder":"Choose an existing regular file";
            else if(!state_.exists && !std::filesystem::is_directory(path.parent_path()))state_.problem="Parent folder does not exist";
            else state_.valid=true;
        }catch(const std::exception& e){state_.problem=e.what();}
    }
    void sync(const PathContext& context) {
        const std::string_view last=context.last_directory?std::string_view(*context.last_directory):std::string_view{};
        if(!refreshed_ || project_!=context.project_directory || last_!=last ||
            ((option_ || (project_.empty() && last_.empty())) && process_!=path_process_directory()))refresh(context);
    }
    bool draw(const PathContext& context,bool disabled=false,const std::function<void()>& after_input={},const std::function<void(bool)>& action={},const char* column_label=nullptr) {
        sync(context);
        if(ImGui::GetCurrentWindow()->SkipItems)return false;
        if(input_context_!=ImGui::GetCurrentContext() || draw_count_frame_!=ImGui::GetFrameCount()){draw_count_frame_=ImGui::GetFrameCount();draw_count_=0;}++draw_count_;
        bool changed=std::exchange(pending_edit_,false),edited=false;submitted_=false;ImGui::BeginDisabled(disabled);
        IconReason blocked_path(!disabled && icon_availability.reason?icon_availability.reason:"Wait for the current work to finish.");
        // Visible labels can use the section's column without changing the
        // original input, browse or picker identity scopes.
        const float label_x=ImGui::GetCursorPosX();
        const auto localized=i18n::label(column_label?column_label:spec_.label);const auto* end=label_end(localized.c_str());
        const bool column=column_label || end!=localized.c_str();
        if(column)prepare_field(localized.c_str(),end,available(),FieldPlacement::Column);
        const float help_indent=column?ImGui::GetCursorPosX()-label_x:0;
        {
        ActionRow row(action?2:1);
        if(!spec_.unscoped_id)ImGui::PushID(spec_.label);
            if(focus_){ImGui::SetKeyboardFocusHere();focus_=false;}
            const std::string input_label=spec_.unscoped_id?std::string("###")+spec_.label:"##value";
            const bool result=ImGui::InputTextWithHint(input_label.c_str(),spec_.literal_hint?spec_.hint:i18n::text(spec_.hint),text_.data(),text_.size(),spec_.enter_to_submit?ImGuiInputTextFlags_EnterReturnsTrue:0);
            trace_layout(LayoutKind::Input,spec_.label);
            input_context_=ImGui::GetCurrentContext();input_id_=ImGui::GetItemID();input_root_window_=ImGui::GetCurrentWindow()->RootWindow->ID;
            clipped_=ImGui::CalcTextSize(text_.data()).x>std::max(0.f,ImGui::GetItemRectSize().x-2*ImGui::GetStyle().FramePadding.x);
            submitted_=spec_.enter_to_submit && result;
            edited=ImGui::IsItemEdited();changed=edited || changed;
            const bool was_hovered=hovered_ && drawn_frame_>=ImGui::GetFrameCount()-1;hovered_=ImGui::IsItemHovered();
            if(ImGui::IsItemActivated() || (hovered_ && !was_hovered))refresh(context);
            drawn_frame_=(ImGui::GetCurrentContext()->LastItemData.ItemFlags&ImGuiItemFlags_Disabled)?-2:ImGui::GetFrameCount();
            tooltip_hovered_=ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
            input_bounds_=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());input_bounds_.ClipWith(ImGui::GetCurrentWindow()->ClipRect);
            auto* window=ImGui::GetCurrentWindow();while(window && !(window->Flags&ImGuiWindowFlags_Modal))window=window->ParentWindow;modal_=window?window->ID:0;
            if(after_input)after_input();observe(spec_.label);
        if(!spec_.unscoped_id)ImGui::PopID();
        if(edited){option_=false;notice_.clear();refresh(context);}
        tooltip_requested_=tooltip_hovered_ && clipped_ && !state_.resolved.empty();
        if(tooltip_requested_ && ImGui::BeginTooltip()) {
            const float width=std::max(1.f,std::min(600.f*design::scale(),ImGui::GetMainViewport()->WorkSize.x*.8f));
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+width);ImGui::TextUnformatted(state_.resolved.c_str());ImGui::PopTextWrapPos();ImGui::EndTooltip();
        }
        row.next();
        ImGui::PushID(spec_.label);
        if(IconButton("Browse",Icon::Open))open_picker(context);
        browse_bounds_=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
        ImGui::PopID();
        if(action){row.next();action(submitted_);}
        }
        ImGui::EndDisabled();
        if(help_indent>0)ImGui::Indent(help_indent);
        describe();
        if(help_indent>0)ImGui::Unindent(help_indent);
        const bool picked=draw_picker(context);if(picked)pending_edit_=false;
        return picked || changed;
    }
    void describe() const {
        if(!text_[0])return;
        if(!state_.problem.empty())TextWrapped("%s",i18n::text(state_.problem.c_str()));
        else if(state_.resolved!=text_.data())TextWrapped("Resolved path: %s",state_.resolved.c_str());
        if(!notice_.empty())TextWrapped("%s",i18n::text(notice_.c_str()));
    }
    void request_focus(){focus_=true;}
    void open_picker(const PathContext& context,const std::string& suggestion={}) {
        refresh(context);auto current=state_.resolved;
        if(current.empty())current=resolve_user_path(suggestion.empty()?(spec_.suggestion && *spec_.suggestion?spec_.suggestion:"."):suggestion,context);
        const auto mode=spec_.role==PathRole::OpenFile?detail::PickerMode::OpenFile:spec_.role==PathRole::NewFile?detail::PickerMode::NewFile:spec_.role==PathRole::OpenFolder?detail::PickerMode::OpenFolder:detail::PickerMode::NewFolder;
        if(!picker_)picker_=std::make_unique<detail::GuiImportPicker>();
        picker_->open(current,spec_.extensions,mode);picker_active_=true;picked_=false;
    }
    bool draw_picker(const PathContext& context) {
        if(!picker_ || !picker_active_ || picker_frame_==ImGui::GetFrameCount())return false;picker_frame_=ImGui::GetFrameCount();bool changed=false;
        // One stable registry host also services fields hidden by a task switch.
        ImGui::PushOverrideID(ImHashStr("qupathxx.path-picker"));
        if(auto path=picker_->draw()) {
            set(*path);if(context.last_directory){const auto p=std::filesystem::u8path(*path);*context.last_directory=(folder_role()?p:p.parent_path()).u8string();}
            refresh(context);picked_=true;picker_active_=false;pending_edit_=true;changed=true;
        } else if(!picker_->active())picker_active_=false;
        ImGui::PopID();return changed;
    }
    bool take_picked(){return picker_ && std::exchange(picked_,false);}
    bool picker_active() const{return picker_ && picker_active_;}
    bool hit(double x,double y,ImGuiID modal=0) const {
        if(!ImGui::GetCurrentContext() || input_context_!=ImGui::GetCurrentContext() || !input_root_window_ || drawn_frame_<ImGui::GetFrameCount()-1 || modal_!=modal || !input_bounds_.Contains({float(x),float(y)}))return false;
        ImGuiWindow* front=nullptr;ImGuiWindow* under_moving=nullptr;
        ImGui::FindHoveredWindowEx({float(x),float(y)},true,&front,&under_moving);
        return front && front->RootWindow->ID==input_root_window_;
    }
    const PathState& state() const {return state_;}
    const char* text() const {return text_.data();}
    void set(std::string_view text) {
        detail::bounded_utf8(std::string(text),32768);
        if(text.find('\0')!=std::string_view::npos)throw std::invalid_argument("Invalid path text");
        pending_edit_=false;option_=false;notice_.clear();std::copy(text.begin(),text.end(),text_.begin());text_[text.size()]=0;
        // ImGui owns a separate buffer while editing. An OS drop or another
        // programmatic set must survive the next draw of that active input.
        if(input_id_ && ImGui::GetCurrentContext()==input_context_) {
            if(auto* input=ImGui::GetInputTextState(input_id_))input->ReloadUserBufAndMoveToEnd();
            if(input_context_->InputTextDeactivatedState.ID==input_id_)input_context_->InputTextDeactivatedState.ID=0;
        }
        auto ctx=cached_context();refresh(ctx);
    }
    void set_option(std::string_view text){set(text);option_=!text.empty();auto ctx=cached_context();refresh(ctx);}
    bool hovered() const {return hovered_;}
    void drop(const std::string& path,bool multiple=false) {set(path);pending_edit_=true;if(multiple)notice_="Using the first dropped path.";}
};
}

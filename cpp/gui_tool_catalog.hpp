#pragma once
#include "gui_layout.hpp"
#include "gui_commands.hpp"
#include "qupathxx/geometry.hpp"
#include "ascii_case.hpp"
#include <array>
#include <functional>
#include <initializer_list>
#include <stdexcept>
#include <map>

namespace qpx::ui {
// Presentation routes only. Native controllers retain their jobs, parameters,
// validation, result identities and keyboard/replay contracts.
enum class ToolId {ObjectClassifier,PixelClassifier,RawPixelClassifier,Fluorescence,
    Spatial,Genes,Registration,SpatialImport,Batch,Tma,Density,Channels,Count};
enum class ToolGroup {Classifiers,Spatial,Project};
enum class ToolRequirement {None,Image,Project,Quantitative};
struct ToolDefinition {ToolId id;const char* label;ToolGroup group;const char* shortcut;ToolRequirement needs;const char* keywords;};
inline constexpr std::array tool_groups={"Classifiers","Spatial and fluorescence","Project tools"};
inline const std::array tool_definitions={
    ToolDefinition{ToolId::ObjectClassifier,command(CommandId::Tool_ObjectClassifier).caption,ToolGroup::Classifiers,command_chord(CommandId::Tool_ObjectClassifier),ToolRequirement::None,"train predict model objects"},
    ToolDefinition{ToolId::PixelClassifier,command(CommandId::Tool_PixelClassifier).caption,ToolGroup::Classifiers,command_chord(CommandId::Tool_PixelClassifier),ToolRequirement::None,"train predict model pixels stream contours"},
    ToolDefinition{ToolId::RawPixelClassifier,command(CommandId::Tool_RawPixelClassifier).caption,ToolGroup::Classifiers,command_chord(CommandId::Tool_RawPixelClassifier),ToolRequirement::None,"quantitative train model stream"},
    ToolDefinition{ToolId::Fluorescence,command(CommandId::Tool_Fluorescence).caption,ToolGroup::Spatial,command_chord(CommandId::Tool_Fluorescence),ToolRequirement::Image,"multiplex markers correction unmixing"},
    ToolDefinition{ToolId::Spatial,command(CommandId::Tool_Spatial).caption,ToolGroup::Spatial,command_chord(CommandId::Tool_Spatial),ToolRequirement::None,"distances neighborhoods density"},
    ToolDefinition{ToolId::Genes,command(CommandId::Tool_Genes).caption,ToolGroup::Spatial,command_chord(CommandId::Tool_Genes),ToolRequirement::Image,"expression compare normalization genes"},
    ToolDefinition{ToolId::Registration,command(CommandId::Tool_Registration).caption,ToolGroup::Spatial,command_chord(CommandId::Tool_Registration),ToolRequirement::Image,"align transform landmarks"},
    ToolDefinition{ToolId::SpatialImport,command(CommandId::Tool_SpatialImport).caption,ToolGroup::Spatial,command_chord(CommandId::Tool_SpatialImport),ToolRequirement::None,"csv h5ad anndata indexed"},
    ToolDefinition{ToolId::Batch,command(CommandId::Tool_Batch).caption,ToolGroup::Project,command_chord(CommandId::Tool_Batch),ToolRequirement::None,"project checkpoint resume"},
    ToolDefinition{ToolId::Tma,command(CommandId::Tool_Tma).caption,ToolGroup::Project,command_chord(CommandId::Tool_Tma),ToolRequirement::Image,"tma qptma core grid"},
    ToolDefinition{ToolId::Density,command(CommandId::Tool_Density).caption,ToolGroup::Project,command_chord(CommandId::Tool_Density),ToolRequirement::Project,"cohort windows cells resume"},
    ToolDefinition{ToolId::Channels,command(CommandId::Tool_Channels).caption,ToolGroup::Project,command_chord(CommandId::Tool_Channels),ToolRequirement::Quantitative,"quantitative tiff zarr planes"}
};
static_assert(tool_definitions.size()==static_cast<std::size_t>(ToolId::Count));
inline constexpr std::array tool_commands={CommandId::Tool_ObjectClassifier,CommandId::Tool_PixelClassifier,CommandId::Tool_RawPixelClassifier,CommandId::Tool_Fluorescence,CommandId::Tool_Spatial,CommandId::Tool_Genes,CommandId::Tool_Registration,CommandId::Tool_SpatialImport,CommandId::Tool_Batch,CommandId::Tool_Tma,CommandId::Tool_Density,CommandId::Tool_Channels};
static_assert(tool_commands.size()==static_cast<std::size_t>(ToolId::Count));
inline constexpr CommandId command_id(ToolId id) {return tool_commands.at(static_cast<std::size_t>(id));}
struct ToolContext {bool image=false,project=false,quantitative=false;};
inline const ToolDefinition& tool_definition(ToolId id) {
    const auto index=static_cast<std::size_t>(id);
    if(index>=tool_definitions.size() || tool_definitions[index].id!=id)throw std::logic_error("Unregistered tool route");
    return tool_definitions[index];
}
inline const char* tool_requirement(const ToolDefinition& tool,const ToolContext& context) {
    switch(tool.needs) {
    case ToolRequirement::Image:if(!context.image)return "Select or add an image to use image tools.";break;
    case ToolRequirement::Project:if(!context.project)return "Open a project first.";break;
    case ToolRequirement::Quantitative:if(!context.quantitative)return "Raw channels require a quantitative image.";break;
    case ToolRequirement::None:break;
    }
    return nullptr;
}
inline bool tool_matches(const ToolDefinition& tool,std::string query) {
    std::string text=std::string(tool.label)+" "+i18n::text(tool.label)+" "+tool.keywords+" "+tool_groups[static_cast<std::size_t>(tool.group)]+" "+i18n::text(tool_groups[static_cast<std::size_t>(tool.group)]);
    detail::ascii_lower_in_place(query);detail::ascii_lower_in_place(text);
    return query.empty() || text.find(query)!=std::string::npos;
}
struct ToolBinding {ToolId id;std::function<void()> open;};
enum class ToolChild {MarkerRules,DarkFlat,Unmixing,SavedStreams,Count};
struct ToolChildDefinition {ToolChild id;ToolId parent;const char* label;};
inline const std::array tool_children={
    ToolChildDefinition{ToolChild::MarkerRules,ToolId::Fluorescence,command(CommandId::Child_MarkerRules).caption},
    ToolChildDefinition{ToolChild::DarkFlat,ToolId::Fluorescence,command(CommandId::Child_DarkFlat).caption},
    ToolChildDefinition{ToolChild::Unmixing,ToolId::Fluorescence,command(CommandId::Child_Unmixing).caption},
    ToolChildDefinition{ToolChild::SavedStreams,ToolId::PixelClassifier,command(CommandId::Child_SavedStreams).caption}
};
inline constexpr const char* tool_legacy_titles[]={"Object classifiers","Pixel classifiers","Raw pixel classifier","Fluorescence cells","Spatial analysis","Gene expression","Landmark registration","Import spatial expression","Project batch","Tissue microarray","Project-wide density","Quantitative channels","Marker rules","Dark/flat correction","Linear unmixing","Saved pixel stream files"};
inline const char* tool_window_label(std::size_t index) {
    static std::array<std::string,16> labels;
    static int language=-1;
    if(language!=i18n::state.effective) {
        // Cache population visits titles that this surface does not draw.
        struct Pause {
            i18n::MissTrace* previous=i18n::miss_trace;
            Pause(){i18n::miss_trace=nullptr;}
            ~Pause(){i18n::miss_trace=previous;}
        } pause;
        for(std::size_t i=0;i<labels.size();++i) {
            const auto* caption=i<tool_definitions.size()?tool_definitions[i].label:tool_children[i-tool_definitions.size()].label;
            std::string title=i18n::text(caption);
            if(title.size()>=3 && title.compare(title.size()-3,3,"...")==0)title.resize(title.size()-3);
            labels[i]=title+"###"+tool_legacy_titles[i];
        }
        language=i18n::state.effective;
    }
    // Observe the requested caption even when another draw filled its cache.
    if(i18n::miss_trace) {
        const auto* caption=index<tool_definitions.size()?tool_definitions.at(index).label:tool_children.at(index-tool_definitions.size()).label;
        i18n::text(caption);
    }
    return labels.at(index).c_str();
}
inline const char* tool_window_label(ToolId id) {return tool_window_label(static_cast<std::size_t>(id));}
inline const char* tool_window_label(ToolChild id) {return tool_window_label(tool_definitions.size()+static_cast<std::size_t>(id));}
inline bool tool_child_matches(const ToolChildDefinition& child,std::string query) {
    std::string names=std::string(child.label)+" "+i18n::text(child.label);
    detail::ascii_lower_in_place(query);detail::ascii_lower_in_place(names);
    return !query.empty() && names.find(query)!=std::string::npos;
}
struct ToolChildBinding {ToolChild id;std::function<void()> open;};
class ToolLauncher {
    std::array<std::function<void()>,tool_definitions.size()> handlers_;
    std::array<std::function<void()>,tool_children.size()> children_;
public:
    explicit ToolLauncher(std::initializer_list<ToolBinding> bindings,std::initializer_list<ToolChildBinding> children={}) {
        for(const auto& binding:bindings) {
            const auto index=static_cast<std::size_t>(tool_definition(binding.id).id);
            if(!binding.open || handlers_[index])throw std::logic_error("Duplicate or empty tool handler");
            handlers_[index]=binding.open;
        }
        for(const auto& handler:handlers_)if(!handler)throw std::logic_error("Missing tool handler");
        for(const auto& child:children) {
            const auto i=static_cast<std::size_t>(child.id);
            if(i>=children_.size() || children_[i] || !child.open)throw std::logic_error("Invalid child tool handler");
            children_[i]=child.open;
        }
        if(children.size())for(const auto& child:children_)if(!child)throw std::logic_error("Missing child tool handler");
    }
    bool open(ToolId id,const ToolContext& context) const {
        if(tool_requirement(tool_definition(id),context))return false;
        handlers_[static_cast<std::size_t>(id)]();return true;
    }
    bool open(ToolChild child,const ToolContext& context) const {
        const auto i=static_cast<std::size_t>(child);if(i>=children_.size())return false;
        const auto parent=tool_children[i].parent;
        if(tool_requirement(tool_definition(parent),context))return false;
        if(children_[i])children_[i]();else handlers_[static_cast<std::size_t>(parent)]();
        return true;
    }
};
inline void tool_hint(const ToolDefinition& tool,const ToolContext& context) {
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip|ImGuiHoveredFlags_AllowWhenDisabled) || ImGui::IsItemFocused()) {
        if(const auto* reason=tool_requirement(tool,context))ImGui::SetTooltip("%s\n%s",i18n::text(tool.label),i18n::text(reason));
        else ImGui::SetTooltip("%s",i18n::text(tool.label));
    }
}
inline void ToolMenus(const ToolLauncher& launcher,const ToolContext& context) {
    for(std::size_t group=0;group<tool_groups.size();++group)if(BeginMenu(tool_groups[group])) {
        for(const auto& tool:tool_definitions)if(static_cast<std::size_t>(tool.group)==group) {
            if(MenuItem(tool.label,tool.shortcut,false,!tool_requirement(tool,context)))launcher.open(tool.id,context);
            tool_hint(tool,context);
        }
        ImGui::EndMenu();
    }
}
struct ToolCatalogTrace {std::map<CommandId,Rect> rows;};
inline std::size_t ToolCatalog(const ToolLauncher& launcher,const ToolContext& context,char* search,std::size_t capacity,ToolCatalogTrace* trace=nullptr) {
    if(trace)trace->rows.clear();
    SetNextItemWidth(-1);InputTextWithHint("##Tool search","Search tools",search,capacity);
    std::size_t matching=0;
    for(std::size_t group=0;group<tool_groups.size();++group) {
        bool heading=false;
        for(const auto& tool:tool_definitions)if(static_cast<std::size_t>(tool.group)==group && tool_matches(tool,search)) {
            if(!heading) {SeparatorText(tool_groups[group]);heading=true;}
            ++matching;ImGui::PushID(static_cast<int>(tool.id));
            ImGui::BeginDisabled(tool_requirement(tool,context)!=nullptr);
            if(CatalogEntry(i18n::text(tool.label),false,*tool.shortcut?tool.shortcut:nullptr))launcher.open(tool.id,context);
            if(trace) {const auto lo=ImGui::GetItemRectMin(),size=ImGui::GetItemRectSize();trace->rows[command_id(tool.id)]={lo.x,lo.y,size.x,size.y};}
            tool_hint(tool,context);ImGui::EndDisabled();ImGui::PopID();
        }
        for(const auto& child:tool_children) {
            const auto& parent=tool_definition(child.parent);
            if(static_cast<std::size_t>(parent.group)!=group || !tool_child_matches(child,search))continue;
            if(!heading) {SeparatorText(tool_groups[group]);heading=true;}
            ++matching;ImGui::PushID(100+static_cast<int>(child.id));
            ImGui::BeginDisabled(tool_requirement(parent,context)!=nullptr);
            if(CatalogEntry(i18n::text(child.label),false))launcher.open(child.id,context);
            if(trace) {const auto lo=ImGui::GetItemRectMin(),size=ImGui::GetItemRectSize();trace->rows[static_cast<CommandId>(static_cast<int>(CommandId::Child_MarkerRules)+static_cast<int>(child.id))]={lo.x,lo.y,size.x,size.y};}
            tool_hint(parent,context);ImGui::EndDisabled();
            TextDisabled("%s",i18n::text(parent.label));ImGui::PopID();
        }
    }
    if(!matching)TextDisabled("No matching tools.");
    return matching;
}
}

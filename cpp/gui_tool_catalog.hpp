#pragma once
#include "gui_layout.hpp"
#include "ascii_case.hpp"
#include <array>
#include <functional>
#include <initializer_list>
#include <stdexcept>

namespace qpx::ui {
// Presentation routes only. Native controllers retain their jobs, parameters,
// validation, result identities and keyboard/replay contracts.
enum class ToolId {ObjectClassifier,PixelClassifier,RawPixelClassifier,Fluorescence,
    Spatial,Genes,Registration,SpatialImport,Batch,Tma,Density,Channels,Count};
enum class ToolGroup {Classifiers,Spatial,Project};
enum class ToolRequirement {None,Image,Project,Quantitative};
struct ToolDefinition {ToolId id;const char* label;ToolGroup group;const char* shortcut;ToolRequirement needs;const char* keywords;};
inline constexpr std::array tool_groups={"Classifiers","Spatial and fluorescence","Project tools"};
inline constexpr std::array tool_definitions={
    ToolDefinition{ToolId::ObjectClassifier,"Object classifiers...",ToolGroup::Classifiers,"F4",ToolRequirement::None,"train predict model objects"},
    ToolDefinition{ToolId::PixelClassifier,"Pixel classifiers...",ToolGroup::Classifiers,"F5",ToolRequirement::None,"train predict model pixels stream contours"},
    ToolDefinition{ToolId::RawPixelClassifier,"Raw pixel classifier",ToolGroup::Classifiers,"",ToolRequirement::None,"quantitative train model stream"},
    ToolDefinition{ToolId::Fluorescence,"Fluorescence cells...",ToolGroup::Spatial,"",ToolRequirement::Image,"multiplex markers correction unmixing"},
    ToolDefinition{ToolId::Spatial,"Spatial...",ToolGroup::Spatial,"",ToolRequirement::None,"distances neighborhoods density"},
    ToolDefinition{ToolId::Genes,"Gene expression...",ToolGroup::Spatial,"",ToolRequirement::Image,"expression compare normalization genes"},
    ToolDefinition{ToolId::Registration,"Landmark registration...",ToolGroup::Spatial,"",ToolRequirement::Image,"align transform landmarks"},
    ToolDefinition{ToolId::SpatialImport,"Import spatial expression...",ToolGroup::Spatial,"",ToolRequirement::None,"csv h5ad anndata indexed"},
    ToolDefinition{ToolId::Batch,"Batch analysis...",ToolGroup::Project,"Ctrl+Alt+B",ToolRequirement::None,"project checkpoint resume"},
    ToolDefinition{ToolId::Tma,"Tissue microarray...",ToolGroup::Project,"",ToolRequirement::Image,"tma qptma core grid"},
    ToolDefinition{ToolId::Density,"Project density...",ToolGroup::Project,"",ToolRequirement::Project,"cohort windows cells resume"},
    ToolDefinition{ToolId::Channels,"Raw channels (F2)",ToolGroup::Project,"",ToolRequirement::Quantitative,"quantitative tiff zarr planes"}
};
static_assert(tool_definitions.size()==static_cast<std::size_t>(ToolId::Count));
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
inline constexpr std::array tool_children={
    ToolChildDefinition{ToolChild::MarkerRules,ToolId::Fluorescence,"Marker rules..."},
    ToolChildDefinition{ToolChild::DarkFlat,ToolId::Fluorescence,"Dark/flat correction..."},
    ToolChildDefinition{ToolChild::Unmixing,ToolId::Fluorescence,"Linear unmixing..."},
    ToolChildDefinition{ToolChild::SavedStreams,ToolId::PixelClassifier,"Saved stream files"}
};
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
inline std::size_t ToolCatalog(const ToolLauncher& launcher,const ToolContext& context,char* search,std::size_t capacity) {
    SetNextItemWidth(-1);InputTextWithHint("##Tool search","Search tools",search,capacity);
    std::size_t matching=0;
    for(std::size_t group=0;group<tool_groups.size();++group) {
        bool heading=false;
        for(const auto& tool:tool_definitions)if(static_cast<std::size_t>(tool.group)==group && tool_matches(tool,search)) {
            if(!heading) {SeparatorText(tool_groups[group]);heading=true;}
            ++matching;ImGui::PushID(static_cast<int>(tool.id));
            ImGui::BeginDisabled(tool_requirement(tool,context)!=nullptr);
            if(CatalogEntry(i18n::text(tool.label),false,*tool.shortcut?tool.shortcut:nullptr))launcher.open(tool.id,context);
            tool_hint(tool,context);ImGui::EndDisabled();ImGui::PopID();
        }
        for(const auto& child:tool_children) {
            const auto& parent=tool_definition(child.parent);
            if(static_cast<std::size_t>(parent.group)!=group || !tool_child_matches(child,search))continue;
            if(!heading) {SeparatorText(tool_groups[group]);heading=true;}
            ++matching;ImGui::PushID(100+static_cast<int>(child.id));
            ImGui::BeginDisabled(tool_requirement(parent,context)!=nullptr);
            if(CatalogEntry(i18n::text(child.label),false))launcher.open(child.id,context);
            tool_hint(parent,context);ImGui::EndDisabled();
            TextDisabled("%s",i18n::text(parent.label));ImGui::PopID();
        }
    }
    if(!matching)TextDisabled("No matching tools.");
    return matching;
}
}

#include "gui_shell.hpp"
#include "gui_fonts.hpp"
#include "gui_icons.hpp"
#include <imgui_internal.h>
#include <RmlUi/Core.h>
#include <RmlUi/Core/Geometry.h>
#include <RmlUi/Core/RenderManager.h>
#include <RmlUi_Renderer_GL3.h>
#include <RmlUi_Platform_GLFW.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace qpx::ui {
namespace {
constexpr const char* ids[]={"project","image","annotations","workflow","select","rectangle",
    "ellipse","polygon","polyline","points","vertices","hole","fit","undo","redo","spatial","channels","reset"};
constexpr Icon icons[]={Icon::Project,Icon::Image,Icon::Annotations,Icon::Workflow,Icon::Select,
    Icon::Rectangle,Icon::Ellipse,Icon::Polygon,Icon::Polyline,Icon::Points,Icon::Vertices,Icon::Hole,
    Icon::Fit,Icon::Undo,Icon::Redo,Icon::Spatial,Icon::Channels,Icon::Reset};
static_assert(std::size(ids)==shell_command_count);
std::string read_resource(const std::filesystem::path& path) {
    constexpr std::size_t limit=1024*1024;
    std::ifstream in(path,std::ios::binary);
    if(!in) throw std::runtime_error("Cannot open UI resource: "+path.u8string());
    std::string text(limit+1,'\0');in.read(text.data(),text.size());text.resize(static_cast<std::size_t>(in.gcount()));
    if(in.bad() || text.empty() || text.size()>limit || text.find('\0')!=std::string::npos)
        throw std::runtime_error("UI resource must be nonempty UTF-8 text, at most 1 MiB");
    return text;
}
std::string escaped(const std::string& s) {
    std::string out;for(char c:s) {if(c=='&') out+="&amp;";else if(c=='<') out+="&lt;";else if(c=='>') out+="&gt;";else out+=c;}return out;
}
void property(Rml::Element* e,const char* name,float value) {
    const auto* old=e->GetLocalProperty(name);
    if(!old || old->unit!=Rml::Unit::PX || old->value.Get<float>()!=value) e->SetProperty(name,std::to_string(value)+"px");
}
// The same vector strokes used by native panels, compiled once per icon size /
// colour into RmlUi retained geometry. No raster atlas or platform emoji icons.
class IconElement final : public Rml::Element {
    Rml::Geometry geometry_;
    Rml::Vector2f size_{};
    Rml::Colourb color_{};
    std::string name_;
public:
    explicit IconElement(const Rml::String& tag):Element(tag) {}
    void OnRender() override {
        const auto size=GetBox().GetSize(Rml::BoxArea::Content);
        const auto color=GetProperty<Rml::Colourb>("color");
        const auto name=GetAttribute<Rml::String>("name","");
        if(!geometry_ || size!=size_ || color!=color_ || name!=name_) {
            size_=size;color_=color;name_=name;
            std::size_t index=0;while(index<std::size(ids) && name!=ids[index]) ++index;
            if(index==std::size(ids)) return;
            ImDrawList draw(ImGui::GetDrawListSharedData());draw._ResetForNewFrame();draw._SetPixelDensity(1.f);
            draw.Flags=ImDrawListFlags_AntiAliasedLines|ImDrawListFlags_AntiAliasedFill;
            draw_icon_to(&draw,icons[index],{0,0},{size.x,size.y},IM_COL32(color.red,color.green,color.blue,color.alpha));
            Rml::Mesh mesh;mesh.vertices.reserve(draw.VtxBuffer.Size);mesh.indices.reserve(draw.IdxBuffer.Size);
            for(const auto& v:draw.VtxBuffer) {
                const auto a=static_cast<Rml::byte>((v.col>>IM_COL32_A_SHIFT)&255);
                auto channel=[&](int shift){return static_cast<Rml::byte>(((v.col>>shift)&255)*a/255);};
                mesh.vertices.push_back({{v.pos.x,v.pos.y},{channel(IM_COL32_R_SHIFT),channel(IM_COL32_G_SHIFT),channel(IM_COL32_B_SHIFT),a},{0,0}});
            }
            for(auto i:draw.IdxBuffer) mesh.indices.push_back(i);
            geometry_=GetRenderManager()->MakeGeometry(std::move(mesh));
        }
        geometry_.Render(GetAbsoluteOffset());
    }
};
class ShellSystem final : public SystemInterface_GLFW {
public:
    using SystemInterface_GLFW::SystemInterface_GLFW;
    bool loading=false;
    std::string diagnostic;
    bool LogMessage(Rml::Log::Type type,const Rml::String& text) override {
        if(loading && type<=Rml::Log::LT_WARNING) {if(diagnostic.size()<8192) diagnostic+=text+"\n";}
        return true;
    }
};
}
bool ShellSnapshot::enabled(ShellCommand command) const {
    if(blocked || static_cast<int>(command)<0 || command>=ShellCommand::Count) return false;
    if(command<=ShellCommand::Workflow || command==ShellCommand::ResetLayout) return true;
    if(!has_image) return false;
    if(command==ShellCommand::Undo) return can_undo;
    if(command==ShellCommand::Redo) return can_redo;
    if(command==ShellCommand::Channels) return raw_channels;
    return true;
}
struct RmlShell::Impl final : Rml::EventListener {
    ShellSystem system;
    std::unique_ptr<RenderInterface_GL3> renderer;
    Rml::ElementInstancerGeneric<IconElement> icon_instancer;
    Rml::Context* context=nullptr;
    Rml::ElementDocument* document=nullptr;
    std::filesystem::path resources;
    std::array<Rml::Element*,shell_command_count> buttons{};
    std::array<std::string,shell_command_count> labels{};
    std::array<std::string,shell_command_count> applied_labels{};
    std::array<bool,shell_command_count> applied_enabled{},applied_selected{};
    std::vector<ShellCommand> commands;
    ShellSnapshot snapshot;
    std::string error;
    unsigned generation=0;
    int width=1,height=1,hover=-1;
    float scale=1,pixel_ratio=1;
    bool light=false,initialized=false,loader=false,focused=false,styles_applied=false,applied_light=false,applied_channels=false;
    ShellRect rail{},toolbar{};
    explicit Impl(GLFWwindow* window):system(window) {}
    ~Impl() {
        if(initialized) Rml::Shutdown();
        renderer.reset();if(loader) RmlGL3::Shutdown();
    }
    void ProcessEvent(Rml::Event& event) override {
        auto* target=event.GetTargetElement();
        while(target && target!=document) {
            for(std::size_t i=0;i<buttons.size();++i) if(target==buttons[i]) {
                if(event.GetType()=="click" && snapshot.enabled(static_cast<ShellCommand>(i)) && commands.size()<32)
                    commands.push_back(static_cast<ShellCommand>(i));
                if(event.GetType()=="mouseover" || event.GetType()=="focus") hover=static_cast<int>(i);
                if(event.GetType()=="mouseout" || event.GetType()=="blur") hover=-1;
                return;
            }
            target=target->GetParentNode();
        }
    }
    void sync() {
        if(!styles_applied || applied_light!=light) {document->SetClass("light",light);applied_light=light;}
        for(std::size_t i=0;i<buttons.size();++i) {
            auto* b=buttons[i];const auto c=static_cast<ShellCommand>(i);
            const bool enabled=snapshot.enabled(c),selected=i<4?snapshot.pane==i:(i<12 && snapshot.tool==static_cast<int>(i)-4);
            if(!styles_applied || applied_selected[i]!=selected) {b->SetClass("selected",selected);applied_selected[i]=selected;}
            if(!styles_applied || applied_enabled[i]!=enabled) {
                b->SetPseudoClass("disabled",!enabled);
                if(enabled) b->RemoveAttribute("disabled");else b->SetAttribute("disabled",true);
                applied_enabled[i]=enabled;
            }
            if(!styles_applied || applied_labels[i]!=labels[i]) {
                b->SetAttribute("aria-label",labels[i]);b->SetAttribute("title",labels[i]);applied_labels[i]=labels[i];
            }
            if(c==ShellCommand::Channels && (!styles_applied || applied_channels!=snapshot.raw_channels)) {
                b->SetProperty("display",snapshot.raw_channels?"block":"none");applied_channels=snapshot.raw_channels;
            }
        }
        styles_applied=true;
        auto* tip=document->GetElementById("tooltip");
        tip->SetProperty("display","none");
        if(hover>=0 && !snapshot.blocked) {
            if(tip->GetInnerRML()!=escaped(labels[hover])) tip->SetInnerRML(escaped(labels[hover]));
        }
    }
    void place(Rml::Element* e,const ShellRect& r) {
        property(e,"left",r.x);property(e,"top",r.y);if(r.height>0) property(e,"height",r.height);
        if(r.width>0) property(e,"width",r.width);
    }
};
RmlShell::RmlShell(GLFWwindow* window,std::filesystem::path resources):impl_(std::make_unique<Impl>(window)) {
    auto& p=*impl_;
    p.resources=resources.empty()?detail::gui_font_directory().parent_path()/"ui":std::move(resources);
    if(!RmlGL3::Initialize()) throw std::runtime_error("RmlUi OpenGL initialization failed");p.loader=true;
    p.renderer=std::make_unique<RenderInterface_GL3>();
    if(!*p.renderer) throw std::runtime_error("RmlUi renderer initialization failed");
    Rml::SetSystemInterface(&p.system);Rml::SetRenderInterface(p.renderer.get());
    if(!Rml::Initialise()) throw std::runtime_error("RmlUi initialization failed");p.initialized=true;
    Rml::Factory::RegisterElementInstancer("qpx-icon",&p.icon_instancer);
    const auto& fonts=detail::gui_font_assets();
    auto font=[](const auto& bytes,const char* family,bool fallback,Rml::Style::FontWeight weight=Rml::Style::FontWeight::Normal) {
        if(!Rml::LoadFontFace({bytes.data(),bytes.size()},family,Rml::Style::FontStyle::Normal,weight,fallback))
            throw std::runtime_error("Cannot load RmlUi packaged font");
    };
    font(fonts.sans,"IBM Plex Sans",false);font(fonts.semibold,"IBM Plex Sans",false,Rml::Style::FontWeight::Bold);
    font(fonts.cjk,"Noto Sans CJK",true);font(fonts.emoji,"Noto Emoji",true);
    p.context=Rml::CreateContext("qupathxx-workspace",{1,1});
    if(!p.context) throw std::runtime_error("Cannot create RmlUi workspace context");
    if(!reload()) throw std::runtime_error(p.error);
}
RmlShell::~RmlShell()=default;
bool RmlShell::reload() {
    auto& p=*impl_;Rml::ElementDocument* candidate=nullptr;
    try {
        auto rml=read_resource(p.resources/"workspace.rml");const auto css=read_resource(p.resources/"workspace.rcss");
        const auto head=rml.find("</head>");if(head==std::string::npos) throw std::runtime_error("Workspace RML requires a head");
        rml.insert(head,"<style>"+css+"</style>");p.system.diagnostic.clear();p.system.loading=true;
        // RmlUi's URL parser treats Windows drive letters as protocols. Both
        // resource files are already read natively; use a portable virtual URL.
        candidate=p.context->LoadDocumentFromMemory(rml,"qupathxx/ui/workspace.rml");
        p.system.loading=false;
        if(!candidate || !p.system.diagnostic.empty()) throw std::runtime_error("Invalid UI resources: "+p.system.diagnostic);
        if(candidate->GetId()!="workspace" || candidate->GetAttribute<Rml::String>("schema","")!="1")
            throw std::runtime_error("Unsupported workspace resource schema");
        for(const auto* id:ids) if(!candidate->GetElementById(id) || candidate->GetElementById(id)->GetTagName()!="button")
            throw std::runtime_error(std::string("Workspace button missing: ")+id);
        for(const char* id:{"activity","toolbar","tooltip"}) if(!candidate->GetElementById(id)) throw std::runtime_error(std::string("Workspace element missing: ")+id);
        auto* old=p.document;p.document=candidate;
        for(std::size_t i=0;i<p.buttons.size();++i) p.buttons[i]=candidate->GetElementById(ids[i]);
        for(const char* event:{"click","mouseover","mouseout","focus","blur"}) candidate->AddEventListener(event,&p);
        p.hover=-1;p.focused=false;p.commands.clear();p.styles_applied=false;p.sync();
        p.place(candidate->GetElementById("activity"),p.rail);
        p.place(candidate->GetElementById("toolbar"),p.toolbar);
        candidate->Show(Rml::ModalFlag::None,Rml::FocusFlag::None);p.context->Update();
        if(old) old->Close();p.context->Update();p.error.clear();++p.generation;return true;
    } catch(const std::exception& e) {
        p.system.loading=false;if(candidate) candidate->Close();p.context->Update();p.error=e.what();return false;
    }
}
void RmlShell::update(int w,int h,float scale,bool light,const ShellSnapshot& snapshot,const std::array<std::string,shell_command_count>& labels,float framebuffer_scale) {
    auto& p=*impl_;p.width=w;p.height=h;p.pixel_ratio=framebuffer_scale;p.scale=scale*framebuffer_scale;p.light=light;p.snapshot=snapshot;
    p.labels=labels;p.context->SetDimensions({static_cast<int>(w*framebuffer_scale),static_cast<int>(h*framebuffer_scale)});p.context->SetDensityIndependentPixelRatio(p.scale);p.sync();p.context->Update();
}
float RmlShell::rail_width() const {return impl_->document->GetElementById("activity")->GetBox().GetSize(Rml::BoxArea::Border).x/impl_->pixel_ratio;}
void RmlShell::place_rail(float x,float y,float h) {
    auto& p=*impl_;p.rail={x*p.pixel_ratio,y*p.pixel_ratio,0,h*p.pixel_ratio};p.place(p.document->GetElementById("activity"),p.rail);p.context->Update();
}
float RmlShell::place_toolbar(float x,float y,float width) {
    auto& p=*impl_;auto* e=p.document->GetElementById("toolbar");
    p.toolbar={x*p.pixel_ratio,y*p.pixel_ratio,width*p.pixel_ratio,0};p.place(e,p.toolbar);p.context->Update();
    return e->GetBox().GetSize(Rml::BoxArea::Border).y/p.pixel_ratio;
}
ShellRect RmlShell::bounds(ShellCommand c) const {
    if(static_cast<int>(c)<0 || c>=ShellCommand::Count) return {};
    auto* e=impl_->buttons[static_cast<std::size_t>(c)];const auto o=e->GetAbsoluteOffset(Rml::BoxArea::Border),s=e->GetBox().GetSize(Rml::BoxArea::Border);
    const auto r=impl_->pixel_ratio;return {o.x/r,o.y/r,s.x/r,s.y/r};
}
bool RmlShell::contains(float x,float y) const {
    for(std::size_t i=0;i<shell_command_count;++i) {const auto b=bounds(static_cast<ShellCommand>(i));if(b.width>0 && b.height>0 && x>=b.x && y>=b.y && x<b.x+b.width && y<b.y+b.height) return true;}return false;
}
bool RmlShell::keyboard_active() const {return impl_->focused && !impl_->snapshot.blocked;}
void RmlShell::input(const ShellInput& i) {
    auto& p=*impl_;const bool hit=contains(i.x,i.y);const int mods=(i.ctrl?Rml::Input::KM_CTRL:0)|(i.shift?Rml::Input::KM_SHIFT:0);
    // ImGui represents a pointer outside the window with -FLT_MAX. Never cast
    // that sentinel (or a nonfinite coordinate) to an integer.
    const bool valid=std::isfinite(i.x) && std::isfinite(i.y) && i.x>=0 && i.y>=0 && i.x<=p.width && i.y<=p.height;
    p.context->ProcessMouseMove(valid?static_cast<int>(i.x*p.pixel_ratio):-10000,valid?static_cast<int>(i.y*p.pixel_ratio):-10000,mods);
    if(i.down) {p.focused=hit;if(hit && !p.snapshot.blocked) p.context->ProcessMouseButtonDown(0,mods);}
    if(i.up) p.context->ProcessMouseButtonUp(0,mods);
    if(hit && i.wheel!=0) p.context->ProcessMouseWheel(-i.wheel,mods);
    if(p.focused && !p.snapshot.blocked) {
        auto key=[&](bool pressed,Rml::Input::KeyIdentifier key){if(pressed) {p.context->ProcessKeyDown(key,mods);p.context->ProcessKeyUp(key,mods);}};
        key(i.tab,Rml::Input::KI_TAB);key(i.enter,Rml::Input::KI_RETURN);key(i.space,Rml::Input::KI_SPACE);
    }
    auto* tip=p.document->GetElementById("tooltip");tip->SetProperty("display",p.hover>=0 && !p.snapshot.blocked?"block":"none");
    if(p.hover>=0 && !p.snapshot.blocked) {
        if(tip->GetInnerRML()!=escaped(p.labels[p.hover])) tip->SetInnerRML(escaped(p.labels[p.hover]));p.context->Update();const auto s=tip->GetBox().GetSize();
        property(tip,"left",std::max(0.f,std::min(i.x*p.pixel_ratio+12*p.scale,p.width*p.pixel_ratio-s.x)));
        property(tip,"top",std::max(0.f,std::min(i.y*p.pixel_ratio+20*p.scale,p.height*p.pixel_ratio-s.y)));
    }
    p.context->Update();
}
std::vector<ShellCommand> RmlShell::take_commands() {std::vector<ShellCommand> out;out.swap(impl_->commands);return out;}
const std::string& RmlShell::error() const {return impl_->error;}
unsigned RmlShell::generation() const {return impl_->generation;}
void RmlShell::render(int w,int h) {auto& p=*impl_;p.renderer->SetViewport(w,h);p.renderer->BeginFrame();p.context->Render();p.renderer->EndFrame();}
}

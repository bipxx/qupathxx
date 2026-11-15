#pragma once
#include "gui_output_gate_checks.hpp"
#include "../reader_provenance.hpp"
namespace output_gate_fix1_checks {
using namespace qpx;
inline void decisions(const std::filesystem::path& directory) {
    const auto dir=std::filesystem::absolute(directory).lexically_normal();
    {
        output_gate_checks::Host host;Project project;auto doc=std::make_shared<Document>(Image(8,8,std::vector<std::uint8_t>(192,80))),member=std::make_shared<Document>(Image(8,8,std::vector<std::uint8_t>(192,80)));project.add_document(doc);project.add_document(member);
        int captures=0,writes=0;std::string error;
        auto cheap=[&](bool selected,bool project_scope){detail::ExportOwnerStamp stamp;stamp.project=&project;stamp.document=doc.get();stamp.server=doc->server().get();stamp.revision=doc->revision();stamp.calibration=doc->calibration_revision();stamp.project_generation=project.generation();stamp.selected_only=selected;stamp.project_scope=project_scope;return stamp;};
        host.gate.bind([&](bool selected,bool project_scope){auto stamp=cheap(selected,project_scope);if(project_scope){++captures;for(const auto& image:project.images()){const auto d=project.document(image.id);stamp.members.emplace_back(image.id,d.get(),d->revision(),d->calibration_revision(),d->server().get());}}return stamp;},[]{return std::vector<std::string>{};});
        check(host.gate.owner().members.empty() && captures==0,"Current-image intent captured project members");
        const auto path=dir/"pending-project.csv";write(path,"sentinel");const auto before=output_gate_checks::hash(path);
        host.gate.submit(path.u8string(),"Project",[&](const std::string&,ExportWritePolicy,const std::vector<std::string>&){++writes;},[&](const std::string& message){error=message;},false,{}, {},true);
        check(captures==1 && host.gate.pending(),"Project intent did not capture once at submission");
        for(int i=0;i<12;++i){ImGui::NewFrame();ui::begin_frame();ImGui::Begin("Cheap owner host");host.gate.draw([&](const detail::ExportOwnerStamp& stamp){return cheap(stamp.selected_only,stamp.project_scope).same_context(stamp);});ImGui::End();ImGui::Render();}
        check(captures==1,"Pending dialog recaptured/enumerated project members per frame");member->add_rectangle({0,0,2,2});
        check(!detail::OutputGateTestAccess::confirm(host.gate,[&](const detail::ExportOwnerStamp& stamp){return cheap(stamp.selected_only,stamp.project_scope).same_context(stamp);}),"Changed member was accepted on Confirm");
        check(captures==2 && writes==0 && !error.empty() && output_gate_checks::hash(path)==before,"Confirm did not recapture once/refuse without writing");
    }
    {
        output_gate_checks::Host host;char text[128]="keep typing";ImGuiID id=0;bool focus=false;
        host.panel=[&]{ImGui::SetNextWindowPos({10,10},ImGuiCond_Always);ImGui::SetNextWindowSize({450,180},ImGuiCond_Always);ImGui::Begin("Typing during export");if(focus)ImGui::SetKeyboardFocusHere();ImGui::InputText("Text",text,sizeof(text));id=ImGui::GetItemID();ImGui::End();};
        host.frame();host.frame();focus=true;host.frame();focus=false;host.frame();host.frame();
        auto& context=*ImGui::GetCurrentContext();const auto active=context.ActiveId;auto* focused=context.NavWindow;check(active==id && focused,"Typing fixture did not acquire active text input");
        host.gate.succeeded((dir/"background.csv").u8string());for(int i=0;i<5;++i){host.frame();check(context.ActiveId==active && context.NavWindow==focused,"Export receipt stole active input or focus");}
        check(!ImGui::FindWindowByName(ui::i18n::label("Export complete").c_str()),"Export completion opened a window");
        check(host.notices.current() && host.notices.current()->notice.severity==ui::Severity::Success,"Export completion notice missing");
        check(host.notices.current()->notice.path==ui::notice_path((dir/"background.csv").u8string()),"Export notice lost its absolute path");
        ImGui::GetIO().AddInputCharactersUTF8("!");host.frame();check(std::string(text).find('!')!=std::string::npos,"Typing did not continue through completion");
    }
    std::cout<<"Fix1: pending project member capture0 per frame / Confirm1 / stale member refusal; passive lower-right receipt preserves active typing/focus/order\n";
}
inline std::filesystem::path extended(const std::filesystem::path& plain) {
#ifdef _WIN32
    const auto path=std::filesystem::absolute(plain).lexically_normal().native();return std::filesystem::path(path.rfind(L"\\\\",0)==0?L"\\\\?\\UNC\\"+path.substr(2):L"\\\\?\\"+path);
#else
    return plain;
#endif
}
inline void long_path(const std::filesystem::path& directory) {
    const auto dir=std::filesystem::absolute(directory).lexically_normal();auto folder=dir;
    while(folder.native().size()<290)folder/=std::string(35,'a');std::filesystem::create_directories(extended(folder));const auto path=folder/"metadata.csv";check(path.native().size()>260,"Long-path fixture is too short");
    Document doc(Image(32,32,std::vector<std::uint8_t>(32*32*3,80)));ObjectSelection selection(doc);TmaGridOptions grid;grid.rows=grid.columns=1;grid.origin={10,10};grid.width=grid.height=4;doc.create_tma_grid(grid);
    GuiOptions options;options.tma_open=true;options.tma_tab=2;options.tma_metadata_path=path.u8string();detail::GuiTma panel(options);output_gate_checks::Host host;
    host.panel=[&]{panel.draw(doc,selection,nullptr,false,true,[](GuiInputTarget){},&host.gate);};for(int i=0;i<5;++i)host.frame();panel.reveal_target(GuiInputTarget::ExportTmaMetadata);for(int i=0;i<5;++i)host.frame();const auto b=panel.bounds(GuiInputTarget::ExportTmaMetadata);check(panel.enabled(GuiInputTarget::ExportTmaMetadata),"Long-path CSV action disabled");auto& io=ImGui::GetIO();io.AddMousePosEvent(float(b.x+b.width/2),float(b.y+b.height/2));host.frame();io.AddMouseButtonEvent(0,true);host.frame();io.AddMouseButtonEvent(0,false);host.frame();
    for(int i=0;i<2000 && panel.export_state()!="saved" && panel.error().empty();++i){host.frame();std::this_thread::sleep_for(std::chrono::milliseconds(1));}host.frame();
    check(panel.export_state()=="saved" && panel.error().empty(),"Long-path metadata failed: "+panel.error());check(read(extended(path))==tma_metadata_csv(*doc.tma_grid()),"Long-path metadata differs from native bytes");check(detail::OutputGateTestAccess::success(host.gate)==path.u8string(),"Long path receipt exposed an extended namespace or relative name");
    std::cout<<"Fix1: actual metadata CSV writes >260-character destination; exact native bytes and plain absolute receipt\n";
}
class Compound final:public ImageServer {
    std::shared_ptr<ImageServer> image_;std::shared_ptr<const ReaderSource> source_;
public:
    Compound(const std::filesystem::path& primary,const std::filesystem::path& companion):image_(ppm_image_server(primary.u8string())),source_(make_compound_reader_source("ppm",primary.u8string(),detail::workflow_file_version(primary.u8string()),{*detail::workflow_file_version(companion.u8string())})){}
    const ImageMetadata& metadata() const override{return image_->metadata();}std::string reader_id() const override{return "ppm";}std::string source_locator() const override{return source_->locator;}std::shared_ptr<const ReaderSource> reader_source() const override{return source_;}
    PixelBuffer read_region(const RegionRequest& r) const override{return image_->read_region(r);}void close() override{image_->close();}bool closed() const override{return image_->closed();}
};
inline void guards(const std::filesystem::path& directory) {
    const auto dir=std::filesystem::absolute(directory).lexically_normal();const auto primary=dir/"second.ppm",companion=dir/"second-companion.bin",alias=dir/"companion-alias.bin";
    write(primary,std::string("P6\n2 2\n255\n")+std::string(12,char(80)));write(companion,"protected companion");std::filesystem::create_hard_link(companion,alias);
    Project project;auto active=std::make_shared<Document>(Image(2,2,std::vector<std::uint8_t>(12,80)));const auto first=project.add_document(active);project.add_document(std::make_shared<Document>(std::make_shared<Compound>(primary,companion)),"Second",primary.u8string(),"ppm");project.set_active_image(first);
    const auto common=detail::export_protected_paths(&project,*active,"",{});check(std::find(common.begin(),common.end(),companion.u8string())==common.end(),"Fixture does not distinguish non-active companion guard");const auto before=output_gate_checks::hash(companion);bool refused=false;try {detail::guard_raw_pixel_frontend_output(&project,alias.u8string());}catch(const std::invalid_argument&){refused=true;}
    check(refused && output_gate_checks::hash(companion)==before && output_gate_checks::hash(alias)==before,"Native guard lost non-active companion identity alias protection");std::cout<<"Fix1: native raw-pixel guard refuses hardlink to second project image companion by file identity\n";
}
}

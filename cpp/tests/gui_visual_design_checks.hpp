#pragma once
#include "gui_icon_glyph_before.hpp"
#include <limits>

namespace glyph_checks {
using namespace qpx;
inline std::map<int,std::string> names() {
    std::ifstream file(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()/"tests/fixtures/gui-icon-vocabulary.tsv");
    check(bool(file),"Glyph vocabulary missing");std::map<int,std::string> result;std::string line;
    while(std::getline(file,line))if(!line.empty() && line[0]!='#') {std::istringstream row(line);int id;std::string command,name;row>>id>>std::quoted(command)>>name;check(bool(row),"Malformed glyph vocabulary");result.emplace(id,name);}
    // Display-only glyphs have no command pair, but are still in the documented
    // vocabulary and must be distinguishable from actionable glyphs.
    const std::pair<ui::Icon,const char*> display[]={{ui::Icon::New,"New"},{ui::Icon::Export,"Export"},{ui::Icon::Settings,"Settings"},{ui::Icon::ChevronRight,"ChevronRight"},{ui::Icon::ChevronDown,"ChevronDown"}};
    for(const auto& glyph:display)result.emplace(int(glyph.first),glyph.second);
    check(result.size()==std::size_t(ui::Icon::Stop)+1,"Glyph raster coverage is incomplete");
    return result;
}
inline void run(const std::filesystem::path& dir,bool validate_before=false) {
    std::filesystem::create_directories(dir);const auto icons=names();check(!icons.empty(),"No glyphs to rasterise");
    check(glfwInit(),"Glyph GLFW init");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    auto* window=glfwCreateWindow(2200,1600,"Icon masks",nullptr,nullptr);check(window,"Glyph GL window");glfwMakeContextCurrent(window);glfwSwapInterval(0);
    int before_bad=0,after_bad=0;double largest=0;
    std::ofstream pairs(dir/"l2-glyph-pairs.tsv");pairs<<"phase\tsize\tfirst\tsecond\tforeground_IoU\n";
    {
        model_safety_checks::GlHost host(window);
        for(bool old:{true,false})for(int size:{24,48}) {
            const auto prefix=std::string("l2-glyph-")+(old?"before-":"after-")+(size==24?"100-":"200-");
            auto draw=[&]{ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({2200,1600});ImGui::Begin("Glyph atlas",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoInputs);
                int cell=0;for(const auto& [id,name]:icons) {const ImVec2 lo{float(32+(cell%8)*160),float(32+(cell/8)*112)},hi{lo.x+float(size),lo.y+float(size)};
                    auto* d=ImGui::GetWindowDrawList();d->AddRectFilled({lo.x-2,lo.y-2},{hi.x+2,hi.y+2},IM_COL32(0,0,0,255));
                    if(old)baseline::draw_before(d,ui::Icon(id),lo,hi,IM_COL32(255,255,255,255));else ui::draw_icon_to(d,ui::Icon(id),lo,hi,IM_COL32(255,255,255,255));++cell;}
                ImGui::End();};
            const auto atlas=dir/(prefix+"atlas.ppm");host.capture(atlas,draw);
            std::ifstream input(atlas,std::ios::binary);std::string magic;int width,height,maximum;input>>magic>>width>>height>>maximum;input.get();check(magic=="P6" && maximum==255,"Glyph atlas format");
            std::vector<unsigned char> rgb(std::size_t(width)*height*3);input.read(reinterpret_cast<char*>(rgb.data()),rgb.size());check(bool(input),"Glyph atlas bytes");
            std::vector<std::pair<std::string,std::vector<bool>>> masks;int cell=0;
            for(const auto& [id,name]:icons) {
                const int x0=32+(cell%8)*160,y0=32+(cell/8)*112;++cell;std::vector<bool> mask;std::ofstream crop(dir/(prefix+name+".ppm"),std::ios::binary);crop<<"P6\n"<<size<<' '<<size<<"\n255\n";
                for(int y=0;y<size;++y)for(int x=0;x<size;++x) {const auto at=(std::size_t(y0+y)*width+x0+x)*3;mask.push_back(rgb[at]>=128);crop.write(reinterpret_cast<const char*>(rgb.data()+at),3);}
                check(std::count(mask.begin(),mask.end(),true)>0,"Empty icon mask");masks.emplace_back(name,std::move(mask));
            }
            for(std::size_t a=0;a<masks.size();++a)for(std::size_t b=a+1;b<masks.size();++b) {
                int intersection=0,united=0;for(std::size_t p=0;p<masks[a].second.size();++p){const bool x=masks[a].second[p],y=masks[b].second[p];intersection+=x&&y;united+=x||y;}
                const double score=double(intersection)/std::max(1,united);pairs<<(old?"before":"after")<<'\t'<<size<<'\t'<<masks[a].first<<'\t'<<masks[b].first<<'\t'<<score<<'\n';
                if(!old)largest=std::max(largest,score);
                // Foreground IoU ignores the common blank background. A 90%
                // overlap leaves too little distinct ink at the 24-pixel size.
                if(score>=.90){if(old)++before_bad;else ++after_bad;std::cout<<(old?"BEFORE":"AFTER")<<" near-identical icon masks: "<<masks[a].first<<" / "<<masks[b].first<<" size "<<size<<" IoU "<<score<<'\n';}
            }
        }
    }
    glfwDestroyWindow(window);glfwTerminate();
    std::cout<<"Glyph check: "<<icons.size()<<" vocabulary icons, 24/48 pixels, foreground threshold 128, IoU rejection >=0.90; before collisions "<<before_bad<<", after collisions "<<after_bad<<", largest after IoU "<<largest<<'\n';
    check((validate_before?before_bad:after_bad)==0,"Near-identical icon masks exceed foreground IoU threshold");
}
}

namespace visual_design_checks {
using namespace qpx;
inline void run() {
    simplification_tools_checks::NativeUi host;ImGui::GetIO().DisplaySize={3000,2000};
    int integer=std::numeric_limits<int>::max();std::int64_t long_integer=std::numeric_limits<std::int64_t>::min();double real=-999999999999.9999;char text[64]="Literal path";
    for(int language:{0,2})for(float scale:{1.f,2.f}) {
        ui::i18n::choose(language);ImGui::GetStyle().FontSizeBase=16*scale;
        auto draw=[&]{ImGui::SetNextWindowSize({2400,1400});ImGui::Begin("Visual design geometry");
            const float token=ui::design::metrics.number_width*ui::design::scale();
            check(ImGui::CalcTextSize("-9223372036854775808").x+2*ImGui::GetStyle().FramePadding.x+2*ImGui::GetFrameHeight()+2*ImGui::GetStyle().ItemInnerSpacing.x<=token,"Numeric token clips signed 64-bit boundary with steppers");
            auto width=[&](auto render){render();check(std::abs(ImGui::GetItemRectSize().x-token)<1,"Numeric field is not the compact token width");};
            ui::SetNextItemWidth(-1);width([&]{ui::InputInt("##integer",&integer);});
            ui::SetNextItemWidth(-1);width([&]{ui::InputInt64("##long",&long_integer);});
            ui::SetNextItemWidth(-1);width([&]{ui::InputDouble("##real",&real);});
            {ui::NumberWidth wide(320);ui::InputDouble("##wide",&real);check(std::abs(ImGui::GetItemRectSize().x-320*ui::design::scale())<1,"Explicit numeric width was ignored");}
            ui::SetNextItemWidth(-1);ui::InputText("##text",text,sizeof(text));check(ImGui::GetItemRectSize().x>token*2,"Text field lost its full-width rule");
            const float right=ImGui::GetCursorScreenPos().x+ImGui::GetContentRegionAvail().x;ImVec2 first_end;
            {ui::CaptionedStrip strip("Project",2,0,true);strip.next();ui::IconButton("Browse",ui::Icon::Open);first_end=ImGui::GetItemRectMax();
                auto* list=ImGui::GetWindowDrawList();const int vertex_start=list->VtxBuffer.Size;strip.destructive();bool visible_separator=false;
                const auto ink=ImGui::ColorConvertFloat4ToU32(ui::design::color(ui::design::colors().border_light));
                for(int v=vertex_start;v<list->VtxBuffer.Size;++v)if((list->VtxBuffer[v].col&0x00ffffffu)==(ink&0x00ffffffu) && (list->VtxBuffer[v].col>>24)>0)visible_separator=true;
                check(visible_separator,"Destructive separator has no visible canvas-token stroke");ui::IconButton("Delete selected checkpoint...",ui::Icon::Trash);
                check(std::abs(ImGui::GetItemRectMin().x-first_end.x-ui::design::group_separator_width*ui::design::scale())<1,"Destructive group gap differs from canvas token");
                check(std::abs(ImGui::GetItemRectMax().x-right)<1,"Destructive strip no longer aligns with content edge");}
            ImGui::End();};host.settle(draw);
        ui::Notifications notices;const auto state=notices.posted();const char* summary="1 of 1 images | 128 x 96 px | Zoom 687% | 1 objects | 0 selected";
        auto status=[&]{ImGui::SetNextWindowSize({2400,220});ImGui::Begin("Status geometry");ui::LayoutTrace trace;auto* previous=ui::layout_trace;ui::layout_trace=&trace;notices.draw_status_bar(summary);ui::layout_trace=previous;
            for(const auto& item:trace.items){if(item.caption==summary)check(std::abs(item.rect.GetWidth()-ImGui::CalcTextSize(summary).x)<1,"Wide status summary is clipped");if(item.command=="Activity")check(std::abs(item.rect.GetWidth()-ImGui::CalcTextSize(ui::i18n::text("Activity")).x-2*ImGui::GetStyle().FramePadding.x)<1,"Activity is not caption-sized");}
            check(ui::layout_stats.overflow_items==0,"Content-sized status overflow");ImGui::End();};host.settle(status);check(notices.posted()==state,"Status rendering posted a notification");
    }
    check(integer==std::numeric_limits<int>::max() && long_integer==std::numeric_limits<std::int64_t>::min() && real==-999999999999.9999 && std::string(text)=="Literal path","Visual changes altered inputs");
    std::cout<<"Visual design: numeric boundaries/width override, full-width text, destructive gap/right edge, natural summary/Activity, unchanged values: English/French 100/200% passed\n";
}
}

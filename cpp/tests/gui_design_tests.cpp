#include "../gui_design_tokens.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <set>
namespace ds=qpx::ui::design;
static_assert(sizeof(ds::Palette)==19*sizeof(std::uint32_t),"captured palette role count");
static_assert(ds::metrics.font_size==16 && ds::metrics.panel_radius==8 && ds::metrics.control_radius==2 && ds::metrics.border==1,"compact role geometry");
static_assert(ds::metrics.control_height==28 && ds::metrics.action_height==28 && ds::metrics.dialog_action_height==32 && ds::metrics.compact_height==24,"action role geometry");
static_assert(ds::metrics.activity_width==48 && ds::metrics.activity_row==48,"activity geometry");
static void check(bool v,const char* reason) {if(!v) throw std::runtime_error(reason);}
static double luminance(std::uint32_t rgb) {
    auto channel=[](unsigned int v){const double c=v/255.;return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
    return .2126*channel((rgb>>16)&255)+.7152*channel((rgb>>8)&255)+.0722*channel(rgb&255);
}
static double contrast(std::uint32_t a,std::uint32_t b) {const auto x=luminance(a),y=luminance(b);return (std::max(x,y)+.05)/(std::min(x,y)+.05);}
int main() {std::cout<<std::unitbuf;std::cerr<<std::unitbuf;try {
    const auto fixture=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()/"tests/fixtures/colmap-design-system.json";
    std::ifstream file(fixture);check(bool(file),"captured palette fixture missing");
    std::ostringstream buffer;buffer<<file.rdbuf();const auto reference=buffer.str();
    const std::regex palette_row("\"(dark|light)\"\\s*:\\s*\\{([^}]+)\\}");
    const std::regex color_row("\"([^\"]+)\"\\s*:\\s*\"#([a-fA-F0-9]{6})\"");
    std::size_t modes=0;std::set<std::string> modes_seen;
    for(std::sregex_iterator it(reference.begin(),reference.end(),palette_row),end;it!=end;++it) {
        check(modes_seen.insert((*it)[1].str()).second,"duplicate captured palette mode");
        const auto& p=(*it)[1]=="dark"?ds::dark:ds::light;
        const std::pair<const char*,std::uint32_t> values[]={{"bg-void",p.bg_void},{"bg-primary",p.bg_primary},{"bg-secondary",p.bg_secondary},{"bg-tertiary",p.bg_tertiary},{"bg-hover",p.bg_hover},{"bg-input",p.bg_input},{"bg-elevated",p.bg_elevated},{"text-primary",p.text_primary},{"text-secondary",p.text_secondary},{"text-muted",p.text_muted},{"border-color",p.border},{"border-light",p.border_light},{"border-subtle",p.border_subtle},{"accent",p.accent},{"accent-hover",p.accent_hover},{"success",p.success},{"warning",p.warning},{"error",p.error},{"info",p.info}};
        const auto body=(*it)[2].str();std::size_t colors=0;std::set<std::string> roles_seen;
        for(std::sregex_iterator row(body.begin(),body.end(),color_row);row!=end;++row) {
            check(roles_seen.insert((*row)[1].str()).second,"duplicate captured palette role");
            const auto value=std::find_if(std::begin(values),std::end(values),[&](const auto& v){return v.first==(*row)[1].str();});
            check(value!=std::end(values),"unknown captured palette role");
            check(value->second==std::stoul((*row)[2].str(),nullptr,16),"compiled palette differs from captured reference");++colors;
        }
        check(colors==std::size(values),"captured palette role count");++modes;
    }
    check(modes==2,"both captured palettes checked");
    check(ds::resolve(ds::Theme::System,false)==ds::Theme::Dark,"system dark");
    check(ds::resolve(ds::Theme::System,true)==ds::Theme::Light,"system light");
    check(ds::resolve(ds::Theme::Dark,true)==ds::Theme::Dark,"explicit dark");
    check(ds::resolve(ds::Theme::Light,false)==ds::Theme::Light,"explicit light");
    for(auto theme:{ds::Theme::Dark,ds::Theme::Light}) {
        const auto& p=ds::palette(theme);
        check(contrast(p.text_primary,p.bg_secondary)>=7,"primary text contrast");
        check(contrast(p.text_secondary,p.bg_tertiary)>=4.5,"secondary text contrast");
        check(contrast(p.bg_void,p.accent)>=7,"primary action contrast");
    }
    check(ds::metrics.action_height>=28 && ds::metrics.dialog_action_height>=32 && ds::metrics.compact_height>=24,"role-specific action target sizes");
    check(ds::metrics.activity_width==48 && ds::metrics.activity_row==48 && ds::metrics.activity_indicator==2,"workbench navigation targets");
    check(ds::metrics.control_height==ds::metrics.icon_size && ds::metrics.checkbox_size>=16,"control and glyph geometry");
    std::cout<<"Design tokens: explicit/system themes, text/action contrast and target sizes pass\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

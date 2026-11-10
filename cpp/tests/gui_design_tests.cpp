#include "../gui_design_tokens.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace ds=qpx::ui::design;
static void check(bool v,const char* reason) {if(!v) throw std::runtime_error(reason);}
static double luminance(std::uint32_t rgb) {
    auto channel=[](unsigned int v){const double c=v/255.;return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
    return .2126*channel((rgb>>16)&255)+.7152*channel((rgb>>8)&255)+.0722*channel(rgb&255);
}
static double contrast(std::uint32_t a,std::uint32_t b) {const auto x=luminance(a),y=luminance(b);return (std::max(x,y)+.05)/(std::min(x,y)+.05);}
int main() {try {
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

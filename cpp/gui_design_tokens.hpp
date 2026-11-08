#pragma once
#include <cstdint>

namespace qpx::ui::design {
// Port of colmap-webview/src/index.css at121bd73a. UI chrome only: scientific
// channels, object classes, image pixels and viewer overlays have separate owners.
struct Palette {
    std::uint32_t bg_void,bg_primary,bg_secondary,bg_tertiary,bg_hover,bg_input,bg_elevated;
    std::uint32_t text_primary,text_secondary,text_muted,border,border_light,border_subtle;
    std::uint32_t accent,accent_hover,success,warning,error,info;
};
inline constexpr Palette dark{
    0x0a0a0a,0x0f0f0f,0x161616,0x1e1e1e,0x262626,0x1a1a1a,0x242424,
    0xe8e8e8,0xa8a8a8,0x858585,0x2a2a2a,0x3a3a3a,0x222222,
    0xb8b8b8,0xd0d0d0,0x6b9b6b,0xb89b6b,0xb86b6b,0x6b8bb8
};
inline constexpr Palette light{
    0xffffff,0xf5f5f4,0xfafaf9,0xeeeeec,0xe3e3e0,0xffffff,0xe8e8e5,
    0x202322,0x4f5552,0x626861,0xd4d6d1,0xa1a69f,0xe1e3dd,
    0x343e38,0x202923,0x326b3b,0x805b18,0xa23c3c,0x365d93
};
struct Spacing {float xs=2,sm=4,md=8,lg=12,xl=16,xxl=24,xxxl=32;};
inline constexpr Spacing spacing;
struct Metrics {
    float font_size=14,panel_radius=8,control_radius=4,border=1;
    float action_height=32,compact_height=24,icon_size=28,sidebar_width=336;
    float dialog_width=560,overview_width=180,overview_margin=12;
};
inline constexpr Metrics metrics;
enum class Theme {Dark,Light,System};
enum class Action {Secondary,Primary,Ghost,Danger};
inline constexpr Theme resolve(Theme theme,bool system_light) {return theme==Theme::System?(system_light?Theme::Light:Theme::Dark):theme;}
inline constexpr const Palette& palette(Theme theme) {return theme==Theme::Light?light:dark;}
}

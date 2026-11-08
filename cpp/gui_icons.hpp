#pragma once
#include "gui_design_system.hpp"
#include <imgui.h>
#include <algorithm>
#include <initializer_list>

namespace qpx::ui {
// One stroke family for desktop commands. Drawn in logical units, without fonts
// or platform glyph substitutions; scientific overlay drawing is independent.
enum class Icon {
    Fit,Undo,Redo,Spatial,Reset,Save,Open,New,Refresh,Trash,Edit,Link,Copy,Paste,
    Import,Export,Settings,Channels,Check,Close,Plus,Minus,Up,Down,Left,Right,
    SelectAll,Deselect,Select,Rectangle,Ellipse,Polygon,Polyline,Points,Vertices,Hole,
    Project,Image,Annotations,Workflow
};
inline void draw_icon_to(ImDrawList* d,Icon icon,ImVec2 lo,ImVec2 hi,ImU32 ink) {
    const float k=std::min(hi.x-lo.x,hi.y-lo.y)/32;
    const ImVec2 origin{(lo.x+hi.x)/2-16*k,(lo.y+hi.y)/2-16*k};const float stroke=1.5f*k;
    auto at=[&](float x,float y){return ImVec2{origin.x+x*k,origin.y+y*k};};
    auto line=[&](float x,float y,float u,float v){d->AddLine(at(x,y),at(u,v),ink,stroke);};
    auto rect=[&](float x,float y,float u,float v){d->AddRect(at(x,y),at(u,v),ink,2*k,0,stroke);};
    auto path=[&](std::initializer_list<ImVec2> points,bool closed=false){d->AddPolyline(points.begin(),static_cast<int>(points.size()),ink,stroke,closed?ImDrawFlags_Closed:0);};
    switch(icon) {
    case Icon::Project:rect(10,10,26,27);path({at(21,6),at(21,4),at(5,4),at(5,21),at(7,21)});line(14,15,22,15);line(14,20,22,20);break;
    case Icon::Image:rect(5,6,27,26);d->AddCircle(at(11,12),2*k,ink,0,stroke);path({at(6,23),at(13,17),at(17,20),at(22,14),at(27,20)});break;
    case Icon::Annotations:path({at(7,24),at(10,7),at(25,10),at(22,25)},true);for(auto p:{at(7,24),at(10,7),at(25,10),at(22,25)})d->AddRectFilled({p.x-2*k,p.y-2*k},{p.x+2*k,p.y+2*k},ink);break;
    case Icon::Workflow:rect(5,5,12,12);rect(20,20,27,27);line(8,12,8,23);line(8,23,20,23);line(12,8,23,8);line(23,8,23,20);break;
    case Icon::Fit:
        for(float x:{7.f,25.f})for(float y:{7.f,25.f}){line(x,y,x+(x<16?6:-6),y);line(x,y,x,y+(y<16?6:-6));}break;
    case Icon::Undo:case Icon::Redo: {
        auto point=[&](float x,float y){return at(icon==Icon::Redo?32-x:x,y);};
        d->AddBezierCubic(point(25,24),point(28,9),point(12,7),point(7,15),ink,stroke);
        path({point(7,8),point(7,15),point(14,15)});break;
    }
    case Icon::Spatial:
        path({at(9,23),at(15,8),at(25,22)},true);for(auto p:{at(9,23),at(15,8),at(25,22)})d->AddCircleFilled(p,3*k,ink);break;
    case Icon::Reset:case Icon::Refresh:
        d->PathArcTo(at(16,16),9*k,-2.6f,2.7f);d->PathStroke(ink,0,stroke);path({at(7,7),at(7,15),at(15,15)});break;
    case Icon::Save:
        path({at(7,6),at(22,6),at(26,10),at(26,26),at(6,26),at(6,6)},true);rect(11,6,21,13);rect(11,19,21,26);break;
    case Icon::Open:
        path({at(6,23),at(6,8),at(13,8),at(16,11),at(25,11),at(25,15)});path({at(6,25),at(10,15),at(28,15),at(24,25)},true);break;
    case Icon::New:
        path({at(20,26),at(7,26),at(7,6),at(19,6),at(25,12),at(25,16)});path({at(19,6),at(19,12),at(25,12)});line(24,19,24,27);line(20,23,28,23);break;
    case Icon::Trash:
        line(6,9,26,9);path({at(9,9),at(10,26),at(22,26),at(23,9)});path({at(12,9),at(12,5),at(20,5),at(20,9)});line(14,13,14,22);line(18,13,18,22);break;
    case Icon::Edit:
        path({at(7,20),at(21,6),at(26,11),at(12,25),at(6,26)},true);line(18,9,23,14);break;
    case Icon::Link:
        d->AddBezierCubic(at(14,10),at(23,-1),at(33,10),at(21,18),ink,stroke);
        d->AddBezierCubic(at(18,22),at(9,33),at(-1,22),at(11,14),ink,stroke);line(11,21,21,11);break;
    case Icon::Copy:rect(11,11,26,26);path({at(21,7),at(21,5),at(5,5),at(5,21),at(7,21)});break;
    case Icon::Paste:rect(8,8,25,27);rect(12,5,21,11);line(12,16,21,16);line(12,21,21,21);break;
    case Icon::Import:case Icon::Export: {
        path({at(6,20),at(6,26),at(26,26),at(26,20)});
        const bool in=icon==Icon::Import;line(16,in?5:21,16,in?21:5);
        path({at(10,in?15:11),at(16,in?21:5),at(22,in?15:11)});break;
    }
    case Icon::Settings:
        for(float y:{9.f,16.f,23.f})line(6,y,26,y);for(auto p:{at(12,9),at(21,16),at(15,23)})d->AddCircle(p,3*k,ink,0,stroke);break;
    case Icon::Channels:rect(6,9,12,25);rect(13,5,19,25);rect(20,13,26,25);break;
    case Icon::Check:path({at(7,16),at(13,23),at(26,9)});break;
    case Icon::Close:line(9,9,23,23);line(9,23,23,9);break;
    case Icon::Plus:line(7,16,25,16);line(16,7,16,25);break;
    case Icon::Minus:line(7,16,25,16);break;
    case Icon::Up:case Icon::Down: {
        const bool up=icon==Icon::Up;line(16,up?25:7,16,up?7:25);path({at(9,up?14:18),at(16,up?7:25),at(23,up?14:18)});break;
    }
    case Icon::Left:case Icon::Right: {
        const bool left=icon==Icon::Left;line(left?25:7,16,left?7:25,16);path({at(left?14:18,9),at(left?7:25,16),at(left?14:18,23)});break;
    }
    case Icon::SelectAll:case Icon::Deselect:
        rect(6,6,26,26);if(icon==Icon::SelectAll)path({at(10,16),at(14,21),at(23,11)});else {line(11,11,21,21);line(11,21,21,11);}break;
    case Icon::Select:path({at(10,6),at(10,25),at(15,20),at(24,20)},true);break;
    case Icon::Rectangle:rect(7,9,25,23);break;
    case Icon::Ellipse:d->AddEllipse(at(16,16),{9*k,7*k},ink,0,0,stroke);break;
    case Icon::Polygon:case Icon::Polyline:path({at(7,23),at(11,8),at(25,12),at(22,24)},icon==Icon::Polygon);break;
    case Icon::Points:
        for(auto p:{at(10,11),at(23,14),at(14,24)}) {d->AddLine({p.x-3*k,p.y},{p.x+3*k,p.y},ink,stroke);d->AddLine({p.x,p.y-3*k},{p.x,p.y+3*k},ink,stroke);}break;
    case Icon::Vertices:
        rect(8,8,24,24);for(auto p:{at(8,8),at(24,8),at(8,24),at(24,24)})d->AddRectFilled({p.x-2*k,p.y-2*k},{p.x+2*k,p.y+2*k},ink);break;
    case Icon::Hole:d->AddCircle(at(16,16),9*k,ink,0,stroke);d->AddCircle(at(16,16),4*k,ink,0,stroke);break;
    }
}
inline void draw_icon(Icon icon,ImVec2 lo,ImVec2 hi,ImU32 ink) {draw_icon_to(ImGui::GetWindowDrawList(),icon,lo,hi,ink);}
}

// SPDX-License-Identifier: AGPL-3.0-only
#include "format_plugin_profile.hpp"
#include "svg_geometry.hpp"

namespace qpx_svg {
constexpr std::string_view svg_ns="http://www.w3.org/2000/svg";
inline constexpr char provider_version[]=QPX_FORMAT_PLUGIN_PROVIDER_VERSION;

std::string_view element(const Node& node) {
    const std::string_view name=node.name;
    const auto split=name.find(separator);
    valid(split!=std::string_view::npos && name.substr(0,split)==svg_ns,"Annotation element must be in the SVG namespace");
    return name.substr(split+1);
}

void attributes(const Node& node,std::initializer_list<std::string_view> specific) {
    constexpr std::string_view common[]={"id","transform","fill","stroke","fill-rule"};
    for (const auto& item : node.attrs) {
        const std::string_view key=item.first;
        valid(std::find(std::begin(common),std::end(common),key)!=std::end(common) ||
              std::find(specific.begin(),specific.end(),key)!=specific.end(),
              "Unsupported SVG attribute, including CSS, clipping or external references");
    }
}

double coord(const Node& node, std::string_view key, double fallback=0) {
    const auto* value=node.attribute(key);
    return value?decimal64(*value):fallback;
}

double extent(const Node& node,std::string_view key) {
    auto text=trim(node.get(key));
    if (text.size()>=2 && text.substr(text.size()-2)=="px") text.remove_suffix(2);
    const auto size=decimal64(text);
    valid(size>0 && size<=1e9 && std::floor(size)==size,"SVG viewport requires positive whole-pixel dimensions");
    return size;
}

Affine viewport(const Node& root, const Canvas& canvas) {
    const auto* box=root.attribute("viewBox");
    const auto aspect=root.get("preserveAspectRatio");
    valid(aspect.empty() || aspect=="none" || aspect=="xMidYMid" || aspect=="xMidYMid meet",
          "Unsupported SVG preserveAspectRatio value");
    if (!box) return {};
    Cursor v{*box};
    const auto origin=v.point();
    const auto w=v.number(true),h=v.number(true);
    valid(w>0 && h>0 && v.empty(),"SVG viewBox requires two positive extents");
    auto x=canvas.width/w,y=canvas.height/h;
    if (aspect=="none") return Affine{}*Affine{x,0,0,y,-origin.x*x,-origin.y*y};
    x=std::min(x,y);
    const auto tx=(canvas.width-w*x)/2-origin.x*x, ty=(canvas.height-h*x)/2-origin.y*x;
    return Affine{}*Affine{x,0,0,x,tx,ty};
}

struct Color { bool set; std::uint32_t rgb; };
Color color(std::string_view value) {
    if (value=="none") return {false,0};
    constexpr std::pair<std::string_view,std::uint32_t> basic[]={
        {"black",0x000000},{"silver",0xc0c0c0},{"gray",0x808080},{"white",0xffffff},
        {"maroon",0x800000},{"red",0xff0000},{"purple",0x800080},{"fuchsia",0xff00ff},
        {"green",0x008000},{"lime",0x00ff00},{"olive",0x808000},{"yellow",0xffff00},
        {"navy",0x000080},{"blue",0x0000ff},{"teal",0x008080},{"aqua",0x00ffff}
    };
    for (const auto& entry : basic)
        if (value==entry.first) return {true,entry.second};
    valid((value.size()==4 || value.size()==7) && value.front()=='#',"Unsupported SVG paint value");
    const bool short_rgb=value.size()==4;
    value.remove_prefix(1);
    std::uint32_t rgb=0;
    const auto status=std::from_chars(value.data(),value.data()+value.size(),rgb,16);
    valid(status.ec==std::errc{} && status.ptr==value.data()+value.size(),"Invalid SVG hex color");
    if (short_rgb) rgb=(rgb>>8)*17*65536+((rgb>>4)&15)*17*256+(rgb&15)*17;
    return {true,rgb};
}

struct Style {
    Color fill{true,0},stroke{false,0};
    bool even_odd=false;
};
Style style(const Node& node,Style value) {
    if (const auto* fill=node.attribute("fill")) value.fill=color(trim(*fill));
    if (const auto* stroke=node.attribute("stroke")) value.stroke=color(trim(*stroke));
    if (const auto* rule=node.attribute("fill-rule")) {
        const auto text=trim(*rule);
        valid(text=="nonzero" || text=="evenodd","Unknown SVG fill-rule");
        value.even_odd=text=="evenodd";
    }
    return value;
}

struct Decoder {
    Handle& handle;
    const Xml& xml;
    Canvas canvas;
    qpx_reader_cancel_v1 cancel;
    std::pmr::memory_resource* memory;

    Point map(const Affine& m,Point p) const {
        const auto q=m.map(p);
        canvas.check(q);
        return q;
    }

    void box(Row& row,const Affine& m,Point top,Point bottom,bool ellipse) {
        valid(top.x<bottom.x && top.y<bottom.y,"Degenerate SVG box or ellipse");
        if (m.axis_aligned()) {
            const auto a=map(m,top),b=map(m,bottom);
            GeometryEncoder wire{row.geometry};
            wire.header(ellipse?1:0);
            for (double value : {std::min(a.x,b.x),std::min(a.y,b.y),std::abs(a.x-b.x),std::abs(a.y-b.y)})
                wire.real(value);
            return;
        }
        valid(!ellipse,"A rotated or skewed SVG ellipse cannot be represented exactly");
        Points points({map(m,top),map(m,{bottom.x,top.y}),map(m,bottom),map(m,{top.x,bottom.y})},memory);
        polygon(row,points);
    }

    void shape(const Node& node,std::string_view kind,const Affine& m,Style paint,std::string_view group) {
        valid(handle.rows.size()<100000,"Too many SVG annotation shapes");
        auto& row=handle.rows.emplace_back(memory);
        if (const auto* id=node.attribute("id")) {
            valid(!id->empty() && id->size()<=256,"Invalid SVG source ID");
            row.id=*id;
        } else row.id="svg-shape-"+std::to_string(handle.rows.size());
        utf8(row.id,cancel);
        if (!node.children.empty()) {
            const auto& title=xml.nodes[node.children.front()];
            valid(node.children.size()==1 && element(title)=="title" && title.children.empty() && title.attrs.empty(),
                  "Only a simple SVG title may accompany shape geometry");
            const auto value=trim(title.text);
            valid(value.size()<=256,"SVG title exceeds 256 bytes");
            utf8(value,cancel);
            row.name=value;
        }
        fact(row.facts,"svg.element",kind);
        if (!group.empty()) fact(row.facts,"svg.group",group);
        const bool line=kind=="line" || kind=="polyline";
        const auto selected=(!line && paint.fill.set)?paint.fill:paint.stroke;
        valid(selected.set,"SVG shape has no visible paint");
        row.flags=QPX_ANNOTATION_OBJECT_COLOR_RGB;
        row.color=selected.rgb;

        if (kind=="rect") {
            attributes(node,{"x","y","width","height","rx","ry"});
            valid(coord(node,"rx")==0 && coord(node,"ry")==0,"Rounded SVG rectangles are unsupported");
            const Point a{coord(node,"x"),coord(node,"y")};
            box(row,m,a,{a.x+coord(node,"width"),a.y+coord(node,"height")},false);
        } else if (kind=="ellipse" || kind=="circle") {
            const bool circle=kind=="circle";
            if (circle) attributes(node,{"cx","cy","r"});
            else attributes(node,{"cx","cy","rx","ry"});
            const Point center{coord(node,"cx"),coord(node,"cy")};
            const auto rx=coord(node,circle?"r":"rx"),ry=circle?rx:coord(node,"ry");
            box(row,m,{center.x-rx,center.y-ry},{center.x+rx,center.y+ry},true);
        } else if (kind=="line") {
            attributes(node,{"x1","x2","y1","y2"});
            Points points({map(m,{coord(node,"x1"),coord(node,"y1")}),
                          map(m,{coord(node,"x2"),coord(node,"y2")})},memory);
            polyline(row,points);
        } else if (kind=="polygon" || kind=="polyline") {
            attributes(node,{"points"});
            valid(!line || !paint.fill.set,"SVG polyline requires fill=none");
            Points points(memory);
            Cursor input{node.get("points")};
            while (!input.empty()) {
                poll(cancel);
                valid(points.size()<max_vertices,"SVG points exceed the vertex limit");
                points.push_back(map(m,input.point(!points.empty())));
            }
            if (line) polyline(row,points);
            else { strip_repeat(points); polygon(row,points); }
        } else if (kind=="path") {
            attributes(node,{"d"});
            path(row,node.get("d"),canvas,m,paint.even_odd,!paint.fill.set,cancel);
        } else fail("Unsupported SVG annotation element");
        valid(row.geometry.size()<=1024u*1024,"SVG geometry exceeds one MiB");
    }

    void visit(std::size_t index,const Affine& parent,Style paint,std::string_view group) {
        poll(cancel);
        const auto& node=xml.nodes[index];
        const auto kind=element(node);
        valid(trim(node.text).empty(),"Unexpected SVG geometry text");
        if (kind=="defs") {
            valid(node.parent==0 && node.children.empty() && node.attrs.empty(),"Only an empty root SVG defs is supported");
            return;
        }
        const auto m=parent*transform(node.get("transform"));
        paint=style(node,paint);
        if (kind!="g") {
            shape(node,kind,m,paint,group);
            return;
        }
        attributes(node,{});
        if (const auto* id=node.attribute("id")) {
            valid(!id->empty() && id->size()<=256,"Invalid SVG group ID");
            utf8(*id,cancel);
            group=*id;
        }
        for (auto child : node.children) visit(child,m,paint,group);
    }
};

struct Definition {
    static constexpr const char *id="org.qupathxx.svg-annotations",*format="svg-annotations",
        *name="SVG pixel annotations",*contract="svg-level-zero-vector/2",*profile=QPX_FORMAT_PLUGIN_PROFILE_ID;
    static void validate_options(qpx_reader_string_v1 v) {
        require(!v.size,"SVG annotations use image-pixel coordinates and empty provider options");
    }
    static void decode(Handle& h,Bytes bytes,const qpx_reader_open_v1& request) {
        Xml xml(&h.arena.memory,request.cancel);
        xml.parse(bytes,true);
        const auto& root=xml.nodes.front();
        valid(element(root)=="svg" && trim(root.text).empty(),"Expected a standalone SVG document");
        attributes(root,{"width","height","viewBox","preserveAspectRatio","version","baseProfile"});
        valid(!root.attribute("transform"),"SVG root transforms are unsupported; transform a child group");
        const Canvas canvas{extent(root,"width"),extent(root,"height")};
        h.canvas_width=std::uint64_t(canvas.width);
        h.canvas_height=std::uint64_t(canvas.height);
        Decoder decoder{h,xml,canvas,request.cancel,&h.arena.memory};
        const auto m=viewport(root,canvas);
        const auto paint=style(root,{});
        for (auto child : root.children) decoder.visit(child,m,paint,{});
        valid(!h.rows.empty(),"SVG contains no supported annotations");
    }
};

qpx_reader_status_v1 QPX_FORMAT_CALL canvas(void* value,qpx_annotation_canvas_v1* out,
        qpx_reader_cancel_v1 cancel,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]{
        require(value && out && out->struct_size>=sizeof(*out),"Invalid SVG canvas request");
        poll(cancel);
        const auto& source=*static_cast<const Handle*>(value);
        *out={sizeof(*out),0,source.canvas_width,source.canvas_height};
        return QPX_READER_OK;
    });
}
} // namespace qpx_svg

extern "C" QPX_ANNOTATION_CANVAS_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL
qpx_annotation_format_entry_v6(std::uint32_t abi,qpx_annotation_format_api_v6* out,qpx_reader_text_v1* error) {
    using Base=qpx_annotation_source::Plugin<qpx_svg::Definition,QPX_ANNOTATION_ABI_V6,qpx_svg::provider_version>;
    return qpx_binary::boundary(error,[&]{
        qpx_binary::require(abi==QPX_ANNOTATION_ABI_V6 && out && out->struct_size>=sizeof(*out),"Invalid SVG annotation ABI");
        *out={sizeof(*out),QPX_ANNOTATION_ABI_V6,Base::source_table(),qpx_svg::canvas};
        return QPX_READER_OK;
    });
}

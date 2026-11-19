// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "../format-common/annotation_xml.hpp"

namespace qpx_svg {
using namespace qpx_annotation_xml;

inline constexpr std::size_t max_vertices = 65000;
struct Point {
    double x, y;
    bool operator==(const Point& p) const noexcept { return x == p.x && y == p.y; }
};

struct Affine {
    double a=1,b=0,c=0,d=1,x=0,y=0;
    Point map(Point p) const { return {a*p.x+c*p.y+x,b*p.x+d*p.y+y}; }
    Affine operator*(const Affine& r) const {
        const auto t = map({r.x,r.y});
        const Affine m{a*r.a+c*r.b,b*r.a+d*r.b,a*r.c+c*r.d,b*r.c+d*r.d,t.x,t.y};
        for (double v : {m.a,m.b,m.c,m.d,m.x,m.y})
            valid(std::isfinite(v), "Nonfinite SVG transform result");
        const auto determinant=m.a*m.d-m.b*m.c;
        valid(std::isfinite(determinant) && determinant != 0, "Singular SVG transform");
        return m;
    }
    bool axis_aligned() const { return (!b && !c) || (!a && !d); }
};

struct Cursor {
    std::string_view left;
    bool empty() { left=trim(left); return left.empty(); }
    double number(bool separated=false) {
        left=trim(left);
        if (separated && !left.empty() && left.front()==',') left=trim(left.substr(1));
        valid(!left.empty(), "Missing SVG number");
        const bool plus=left.front()=='+';
        if (plus) left.remove_prefix(1);
        valid(!left.empty() && (!plus || left.front()!='-'), "Invalid SVG numeric sign");
        double value=0;
        const auto r=std::from_chars(left.data(),left.data()+left.size(),value,std::chars_format::general);
        valid(r.ec==std::errc{} && std::isfinite(value), "Malformed or nonfinite SVG number");
        left.remove_prefix(std::size_t(r.ptr-left.data()));
        return value;
    }
    Point point(bool separated=false) { const auto x=number(separated); return {x,number(true)}; }
};

inline Affine transform(std::string_view text) {
    Affine result;
    for (unsigned count=0; !(text=trim(text)).empty(); ++count) {
        const auto begin=text.find('('), end=text.find(')');
        valid(count<64 && begin<end && end<text.size(), "Malformed SVG transform list");
        const auto op=trim(text.substr(0,begin));
        Cursor args{text.substr(begin+1,end-begin-1)};
        Affine local;
        const auto first=args.number();
        if (op=="translate") {
            local.x=first;
            if (!args.empty()) local.y=args.number(true);
        } else if (op=="scale") {
            local.a=first;
            local.d=args.empty()?first:args.number(true);
        } else if (op=="matrix") {
            local={first,args.number(true),args.number(true),args.number(true),args.number(true),args.number(true)};
        } else fail("Unsupported SVG transform function; use matrix, translate or scale");
        valid(args.empty(), "Extra SVG transform arguments");
        result=result*local;
        text=trim(text.substr(end+1));
        if (!text.empty() && text.front()==',') {
            text=trim(text.substr(1));
            valid(!text.empty(), "Trailing SVG transform separator");
        }
    }
    return result;
}

struct Canvas {
    double width,height;
    void check(Point p) const {
        valid(std::isfinite(p.x) && std::isfinite(p.y) && p.x>=0 && p.y>=0 &&
              p.x<=width && p.y<=height, "SVG geometry would be clipped by its viewport");
    }
};

using Points=std::pmr::vector<Point>;
using Paths=std::pmr::vector<Points>;

inline void vertices(GeometryEncoder& wire, const Points& points) {
    wire.u32(std::uint32_t(points.size()));
    for (const auto& p : points) { wire.real(p.x); wire.real(p.y); }
}

inline void polygon(Row& row, const Points& points) {
    valid(points.size()>=3,"SVG polygon requires at least three vertices");
    GeometryEncoder wire{row.geometry};
    wire.header(2);
    wire.u32(1);
    vertices(wire,points);
    wire.u32(0);
}
inline void polyline(Row& row, const Points& points) {
    valid(points.size()>=2,"SVG line requires at least two vertices");
    GeometryEncoder wire{row.geometry};
    wire.header(4);
    vertices(wire,points);
}
inline void strip_repeat(Points& points) {
    if (points.size()>1 && points.front()==points.back()) points.pop_back();
}

inline void path(Row& row, std::string_view s, const Canvas& canvas, const Affine& m,
                 bool even_odd, bool unfilled, qpx_reader_cancel_v1 cancel) {
    auto* r=row.geometry.get_allocator().resource();
    Paths rings(r);
    Cursor cursor{s};
    char cmd=0;
    Point current{},origin{};
    bool active=false;
    std::size_t n=0;
    while (!cursor.empty()) {
        if (!(n&1023)) poll(cancel);
        const auto first=cursor.left.front();
        const bool letter=(first>='a' && first<='z') || (first>='A' && first<='Z');
        if (letter) { cmd=first; cursor.left.remove_prefix(1); }
        const bool relative=cmd>='a' && cmd<='z';
        const char op=relative?char(cmd-'a'+'A'):cmd;
        if (op=='Z') {
            valid(active,"SVG path closes without an active subpath");
            strip_repeat(rings.back());
            valid(rings.back().size()>=3,"SVG closed path needs three vertices");
            active=false;
            current=origin;
            cmd=0;
            continue;
        }
        Point next=current;
        switch (op) {
        case 'M': case 'L': {
            const auto p=cursor.point(!letter);
            next=relative?Point{p.x+current.x,p.y+current.y}:p;
            break;
        }
        case 'H': {
            const auto x=cursor.number(!letter);
            next.x=relative?x+current.x:x;
            break;
        }
        case 'V': {
            const auto y=cursor.number(!letter);
            next.y=relative?y+current.y:y;
            break;
        }
        default: fail("SVG path requires straight M/L/H/V/Z commands");
        }
        if (op=='M') {
            valid(!active && rings.size()<1024, "Unsupported SVG path subpath count or open subpath");
            rings.emplace_back();
            active=true;
            origin=next;
            cmd=relative?'l':'L';
        }
        valid(active && ++n<=max_vertices,"SVG path has no starting move or exceeds vertex limit");
        const auto mapped=m.map(next);
        canvas.check(mapped);
        rings.back().push_back(mapped);
        current=next;
    }
    valid(!rings.empty(),"Empty SVG path");
    if (rings.size()==1) {
        if (active) {
            valid(unfilled,"Open SVG paths require fill=none");
            polyline(row,rings.front());
        } else polygon(row,rings.front());
        return;
    }
    valid(!active && even_odd,"Multiple SVG rings require closed even-odd fill");
    row.encoding=QPX_FORMAT_GEOMETRY_EVEN_ODD_RINGS1;
    row.geometry={'Q','E','C',1};
    GeometryEncoder wire{row.geometry};
    wire.u32(std::uint32_t(rings.size()));
    for (const auto& ring : rings) vertices(wire,ring);
}
} // namespace qpx_svg

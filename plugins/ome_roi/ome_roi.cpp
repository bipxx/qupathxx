// SPDX-License-Identifier: AGPL-3.0-only
// OME specification attribution/CC-BY-4.0 and Expat MIT/CC0 terms are in
// reference-lock.json and the package notices. No image reader/runtime
// involved.
#include "format_plugin_profile.hpp"
#include "mask_compression.hpp"
#include "options.hpp"
#include "xml.hpp"
#include <array>
#include <charconv>
#include <map>
#include <set>
namespace qpx_ome {
inline double real(std::string_view value) { return decimal64(value); }
std::int64_t integer(std::string_view s) {
  s = trim(s);
  if (!s.empty() && s.front() == '+') {
    s.remove_prefix(1);
    valid(!s.empty() && s.front() != '-' && s.front() != '+',
          "Invalid OME integer sign");
  }
  std::int64_t x = 0;
  const auto p = std::from_chars(s.data(), s.data() + s.size(), x);
  valid(!s.empty() && p.ec == std::errc{} && p.ptr == s.data() + s.size(),
        "Invalid OME integer");
  return x;
}
bool boolean(std::string_view s) {
  s = trim(s);
  valid(s == "true" || s == "false" || s == "1" || s == "0",
        "Invalid OME boolean");
  return s == "true" || s == "1";
}
std::string_view required(const Node &n, std::string_view key) {
  valid(n.attribute(key) != nullptr, "Missing required OME attribute");
  return n.get(key);
}
void whitespace(const Node &n) {
  valid(trim(n.text).empty(), "Unexpected OME element text");
}
void id(std::string_view value) {
  valid(!value.empty() && value.size() <= 256,
        "OME source ID exceeds native identifier contract");
  utf8(value, {});
}
void attributes(const Node &n, std::string_view names) {
  // A delimiter at both ends makes matching whole attribute names explicit.
  for (const auto &[k, v] : n.attrs) {
    std::string key = "|";
    key.append(k);
    key.push_back('|');
    valid(names.find(key) != std::string_view::npos,
          "Unknown/namespaced OME geometry attribute");
  }
}
struct Point {
  double x, y;
};
struct Transform {
  // OME row equations; this is source interpretation, not a registration API.
  std::array<double, 6> m{1, 0, 0, 1, 0, 0}; // A00,A01,A10,A11,A02,A12
  bool identity() const { return m == std::array<double, 6>{1, 0, 0, 1, 0, 0}; }
  Point map(Point p) const {
    if (identity())
      return p; // Preserve binary64 and signed-zero source values.
    return {m[0] * p.x + m[1] * p.y + m[4], m[2] * p.x + m[3] * p.y + m[5]};
  }
  bool diagonal() const { return m[1] == 0 && m[2] == 0; }
  bool exchanged() const { return m[0] == 0 && m[3] == 0; }
};
struct Decoder {
  Handle &h;
  Xml &xml;
  const Options &options;
  qpx_reader_cancel_v1 cancel;
  std::pmr::memory_resource *r;
  std::pmr::map<std::string_view, std::size_t, std::less<>> ids;
  std::pmr::vector<std::size_t> images, rois, selected;
  std::pmr::set<std::size_t> referenced;
  std::size_t image = SIZE_MAX, pixels = SIZE_MAX;
  std::array<std::uint64_t, 3> dimensions{};
  bool bundled_channels = false;
  mutable std::byte* mask_workspace=nullptr;
  Decoder(Handle &handle, Xml &source, const Options &o, qpx_reader_cancel_v1 c)
      : h(handle), xml(source), options(o), cancel(c), r(&h.arena.memory),
        ids(r), images(r), rois(r), selected(r), referenced(r) {}
  const Node &node(std::size_t i) const { return xml.nodes[i]; }
  std::size_t lookup(std::string_view value) const {
    const auto it = ids.find(value);
    valid(it != ids.end(), "Unresolved OME ID reference");
    return it->second;
  }
  void chunk_tree(Facts &facts, std::string_view key, std::size_t index) const {
    xml.tree_facts(facts, key, index);
  }
  void attrs(Facts &facts, std::string_view prefix, const Node &n) const {
    xml.attribute_facts(facts, prefix, n);
  }
  void discover() {
    for (std::size_t i = 0; i < xml.nodes.size(); ++i) {
      poll(cancel);
      const auto &n = node(i);
      const auto type = local(n.name);
      if (ome(n.name, type) && n.attribute("ID") &&
          !(type.size() >= 3 && type.substr(type.size() - 3) == "Ref")) {
        id(n.get("ID"));
        valid(ids.emplace(n.get("ID"), i).second, "Duplicate OME source ID");
      }
    }
    for (auto child : node(0).children) {
      if (ome(node(child).name, "Image"))
        images.push_back(child);
      else if (ome(node(child).name, "ROI"))
        rois.push_back(child);
    }
    valid(!rois.empty(), "OME document contains no ROI definitions");
    for (auto source_image : images) {
      const auto &im = node(source_image);
      id(required(im, "ID"));
      std::pmr::set<std::size_t> per_image(r);
      for (auto child : im.children)
        if (ome(node(child).name, "ROIRef")) {
          const auto &ref = node(child);
          attributes(ref, "|ID|");
          whitespace(ref);
          valid(ref.children.empty(), "OME ROIRef cannot have children");
          const auto roi = lookup(required(ref, "ID"));
          valid(node(roi).parent == 0 && ome(node(roi).name, "ROI") &&
                    per_image.insert(roi).second,
                "OME image ROI reference is invalid or repeated");
          referenced.insert(roi);
        }
    }
    if (options.unbound) {
      for (auto roi : rois)
        if (!referenced.count(roi))
          selected.push_back(roi);
    } else if (!options.image.empty()) {
      image = lookup(options.image);
      valid(node(image).parent == 0 && ome(node(image).name, "Image"),
            "OME image_id does not select a source Image");
    } else if (images.size() == 1)
      image = images.front();
    else if (images.empty())
      selected = rois;
    else
      fail("Multi-image OME XML requires explicit image_id or unbound_rois "
           "selection",
           QPX_READER_INVALID_REQUEST);
    if (image != SIZE_MAX) {
      for (auto child : node(image).children) {
        if (ome(node(child).name, "ROIRef"))
          selected.push_back(lookup(required(node(child), "ID")));
        if (ome(node(child).name, "Pixels")) {
          valid(pixels == SIZE_MAX, "Repeated OME Pixels");
          pixels = child;
        }
      }
      valid(pixels != SIZE_MAX, "Selected OME Image has no Pixels context");
      const auto &p = node(pixels);
      id(required(p, "ID"));
      for (const auto *key : {"SizeX", "SizeY", "SizeZ", "SizeC", "SizeT"}) {
        const auto value = integer(required(p, key));
        valid(value > 0 && value <= INT32_MAX,
              "OME image dimensions exceed profile");
      }
      dimensions = {std::uint64_t(integer(p.get("SizeC"))),
                    std::uint64_t(integer(p.get("SizeZ"))),
                    std::uint64_t(integer(p.get("SizeT")))};
      for (auto child : p.children)
        if (ome(node(child).name, "Channel")) {
          const auto &channel = node(child);
          if (channel.attribute("SamplesPerPixel")) {
            const auto samples = integer(channel.get("SamplesPerPixel"));
            valid(samples > 0, "Invalid OME channel SamplesPerPixel");
            if (samples != 1)
              bundled_channels = true;
          }
        }
    }
    valid(!selected.empty(), "OME source selection contains no ROIs; "
                             "unreferenced ROIs require explicit unbound_rois");
  }
  Transform transform(const Node &shape,bool mask) const {
    Transform out;
    bool seen = false;
    for (auto child : shape.children) {
      const auto &n = node(child);
      if (ome(n.name, "Transform")) {
        valid(!seen && n.children.empty(), "Repeated/non-value OME transform");
        seen = true;
        attributes(n, "|A00|A01|A10|A11|A02|A12|");
        whitespace(n);
        const char *keys[] = {"A00", "A01", "A10", "A11", "A02", "A12"};
        for (unsigned i = 0; i < 6; ++i)
          out.m[i] = real(required(n, keys[i]));
      } else
        valid(ome(n.name, "AnnotationRef") || (mask && ome(n.name,"BinData")),
              "Unknown OME shape child/profile");
    }
    return out;
  }
  static std::int64_t mask_integral(double value) {
    valid(value>=INT32_MIN && value<=INT32_MAX && std::trunc(value)==value,
          "OME mask requires integral origin, size and translation");
    return static_cast<std::int64_t>(value);
  }
  void mask_geometry(Row& row,const Node& shape,const Transform& tr) const {
    valid(pixels!=SIZE_MAX,"OME mask requires a selected Image Pixels canvas");
    const auto& canvas=node(pixels);
    constexpr std::uint64_t payload_limit=1024*1024-12;
    const auto canvas_x=std::uint64_t(integer(required(canvas,"SizeX")));
    const auto canvas_y=std::uint64_t(integer(required(canvas,"SizeY")));
    valid(canvas_x<=1000000 && canvas_y<=1000000 &&
          canvas_x*canvas_y<=payload_limit*8,"OME mask canvas exceeds native bounds");
    valid((tr.diagonal() && std::abs(tr.m[0])==1 && std::abs(tr.m[3])==1) ||
          (tr.exchanged() && std::abs(tr.m[1])==1 && std::abs(tr.m[2])==1),
          "OME mask transform requires a grid-preserving XY orientation");
    const auto a=std::int64_t(tr.m[0]),b=std::int64_t(tr.m[1]);
    const auto c=std::int64_t(tr.m[2]),d=std::int64_t(tr.m[3]);
    const auto x=mask_integral(real(required(shape,"X")));
    const auto y=mask_integral(real(required(shape,"Y")));
    const auto w=mask_integral(real(required(shape,"Width")));
    const auto height=mask_integral(real(required(shape,"Height")));
    valid(w>0 && height>0,"OME mask crop lies outside its canvas");
    // A reflected unit cell starts one pixel below its transformed integer corner.
    const auto origin_x=a*x+b*y+mask_integral(tr.m[4])-(a+b<0);
    const auto origin_y=c*x+d*y+mask_integral(tr.m[5])-(c+d<0);
    const auto delta_x=a*(w-1)+b*(height-1);
    const auto delta_y=c*(w-1)+d*(height-1);
    const auto fits=[](std::int64_t first,std::int64_t delta,std::uint64_t size) {
      const auto last=first+delta;
      return std::min(first,last)>=0 && std::max(first,last)<std::int64_t(size);
    };
    valid(fits(origin_x,delta_x,canvas_x) && fits(origin_y,delta_y,canvas_y),
          "OME mask crop lies outside its canvas");
    const Node* data=nullptr;
    for(auto child:shape.children)if(ome(node(child).name,"BinData")) {
        valid(!data,"Repeated OME mask BinData");
        data=&node(child);
    }
    valid(data && data->children.empty(),"OME mask requires one BinData value");
    attributes(*data,"|Length|BigEndian|Compression|");
    (void)boolean(required(*data,"BigEndian"));
    const auto compression=data->attribute("Compression")?data->get("Compression"):"none";
    valid(compression=="none" || compression=="zlib" || compression=="bzip2",
          "Unsupported OME mask BinData compression");
    const auto pixels_count=std::uint64_t(w*height);
    const auto bytes_count=(pixels_count+7)/8;
    const auto length=integer(required(*data,"Length"));
    valid(length>0 && length%4==0 && length<=std::int64_t(mask_max_encoded_chars),
          "Invalid OME mask BinData Length");
    if(compression=="none")
        valid(length==std::int64_t(4*((bytes_count+2)/3)),
              "OME mask BinData Length differs from expected base64 character count");
    auto bytes=mask_bytes(data->text,std::size_t(length),r,cancel);
    if(compression!="none") {
        if(!mask_workspace)mask_workspace=static_cast<std::byte*>(r->allocate(mask_codec_work,alignof(std::max_align_t)));
        bytes=decompress_mask(compression,bytes,bytes_count,mask_workspace,r,cancel);
    }
    valid(bytes.size()==bytes_count,"Invalid OME mask decoded length");
    const auto tail=unsigned(pixels_count%8);
    valid(!tail || !(bytes.back() & ((1u<<(8-tail))-1)),"Nonzero unused OME mask bits");
    row.encoding=QPX_FORMAT_GEOMETRY_BINARY_MASK1;
    row.geometry={'Q','B','M',1};
    GeometryEncoder encoder{row.geometry};
    encoder.u32(std::uint32_t(canvas_x));
    encoder.u32(std::uint32_t(canvas_y));
    row.geometry.resize(12+(canvas_x*canvas_y+7)/8);
    bool foreground=false;
    std::uint64_t i=0;
    for(std::int64_t v=0;v<height;++v) {
      auto target_x=origin_x+b*v;
      auto target_y=origin_y+d*v;
      for(std::int64_t u=0;u<w;++u,++i,target_x+=a,target_y+=c) {
        if(!(i%4096))poll(cancel);
        if(!(bytes[i/8] & (128u>>(i%8))))continue;
        const auto target=std::uint64_t(target_y)*canvas_x+std::uint64_t(target_x);
        row.geometry[12+target/8]|=std::uint8_t(128u>>(target%8));
        foreground=true;
      }
    }
    valid(foreground,"OME mask coverage is empty");
    attrs(row.facts,"ome.mask.bindata.attributes",*data);
  }
  void references(Facts &facts, std::size_t roi, std::size_t shape) const {
    std::pmr::vector<std::size_t> pending(r), found(r);
    if (image != SIZE_MAX)
      pending.push_back(image);
    pending.push_back(roi);
    pending.push_back(shape);
    for (std::size_t at = 0; at < pending.size(); ++at) {
      poll(cancel);
      valid(pending.size() <= 64,
            "OME annotation reference graph exceeds profile");
      for (auto child : node(pending[at]).children)
        if (ome(node(child).name, "AnnotationRef")) {
          const auto &ref = node(child);
          attributes(ref, "|ID|");
          whitespace(ref);
          valid(ref.children.empty(), "OME AnnotationRef must be a value");
          const auto target = lookup(required(ref, "ID"));
          valid(
              node(target).parent != SIZE_MAX &&
                  ome(node(node(target).parent).name, "StructuredAnnotations"),
              "OME AnnotationRef does not select a structured annotation");
          if (std::find(found.begin(), found.end(), target) == found.end()) {
            found.push_back(target);
            pending.push_back(target);
          }
        }
    }
    number(facts, "ome.annotations.count", found.size());
    for (std::size_t i = 0; i < found.size(); ++i)
      chunk_tree(facts, "ome.annotation." + std::to_string(i), found[i]);
  }
  void geometry(Row &row, const Node &shape) const {
    const auto type = local(shape.name);
    const bool mask=type=="Mask";
    const auto common =
        "|ID|TheC|TheZ|TheT|Text|Locked|FillColor|FillRule|StrokeColor|"
        "StrokeWidth|StrokeWidthUnit|StrokeDashArray|FontFamily|FontSize|"
        "FontSizeUnit|FontStyle|";
    std::string names(common);
    if (type == "Point" || type == "Label")
      names += "X|Y|";
    else if (type == "Rectangle" || mask)
      names += "X|Y|Width|Height|";
    else if (type == "Ellipse")
      names += "X|Y|RadiusX|RadiusY|";
    else if (type == "Line")
      names += "X1|Y1|X2|Y2|MarkerStart|MarkerEnd|";
    else if (type == "Polygon")
      names += "Points|";
    else if (type == "Polyline")
      names += "Points|MarkerStart|MarkerEnd|";
    else
      fail("OME ROI contains an unsupported shape");
    valid(ome(shape.name, type), "Foreign/unknown OME shape namespace");
    attributes(shape, names);
    whitespace(shape);
    const auto tr = transform(shape,mask);
    const auto get = [&](const char *key) {
      return real(required(shape, key));
    };
    GeometryEncoder encoder{row.geometry};
    const auto point = [&](Point p) {
      p = tr.map(p);
      encoder.real(p.x);
      encoder.real(p.y);
    };
    const auto ring = [&](const std::pmr::vector<Point> &points) {
      encoder.u32(std::uint32_t(points.size()));
      for (auto p : points)
        point(p);
    };
    if (mask) {
      mask_geometry(row,shape,tr);
    } else if (type == "Point" || type == "Label") {
      encoder.header(5);
      encoder.u32(1);
      point({get("X"), get("Y")});
    } else if (type == "Line") {
      encoder.header(4);
      encoder.u32(2);
      point({get("X1"), get("Y1")});
      point({get("X2"), get("Y2")});
    } else if (type == "Rectangle") {
      const auto x = get("X"), y = get("Y"), w = get("Width"),
                 z = get("Height");
      valid(w > 0 && z > 0, "OME rectangle must have positive width/height");
      if (tr.identity()) {
        encoder.header(0);
        for (auto v : {x, y, w, z})
          encoder.real(v);
      } else {
        // A parallelogram preserves every transformed corner without a bounding
        // box approximation, including shear, rotation and reflection.
        encoder.header(2);
        encoder.u32(1);
        encoder.u32(4);
        for (Point p : {Point{x, y}, Point{x + w, y}, Point{x + w, y + z},
                        Point{x, y + z}})
          point(p);
        encoder.u32(0);
      }
    } else if (type == "Ellipse") {
      const auto center = tr.map({get("X"), get("Y")});
      auto rx = get("RadiusX"), ry = get("RadiusY");
      valid(rx > 0 && ry > 0 && (tr.diagonal() || tr.exchanged()),
            "Rotated/sheared/degenerate OME ellipse is not exactly "
            "representable");
      if (!tr.identity()) {
        const auto a =
            tr.diagonal() ? std::abs(tr.m[0] * rx) : std::abs(tr.m[1] * ry);
        const auto b =
            tr.diagonal() ? std::abs(tr.m[3] * ry) : std::abs(tr.m[2] * rx);
        rx = a;
        ry = b;
      }
      valid(rx > 0 && ry > 0, "Degenerate OME ellipse transform");
      encoder.header(1);
      for (auto v : {center.x - rx, center.y - ry, 2 * rx, 2 * ry})
        encoder.real(v);
    } else {
      auto s = trim(required(shape, "Points"));
      std::pmr::vector<Point> points(r);
      while (!s.empty()) {
        poll(cancel);
        valid(points.size() < 65000,
              "OME point count exceeds geometry profile");
        const auto comma = s.find(',');
        valid(comma != std::string_view::npos, "OME Points requires x,y pairs");
        const auto x = real(s.substr(0, comma));
        s.remove_prefix(comma + 1);
        const auto end = s.find_first_of(" \t\r\n");
        const auto y = real(s.substr(0, end));
        points.push_back({x, y});
        if (end == std::string_view::npos)
          s = {};
        else {
          s.remove_prefix(end);
          s = trim(s);
        }
      }
      if (type == "Polygon") {
        encoder.header(2);
        encoder.u32(1);
        ring(points);
        encoder.u32(0);
      } else {
        encoder.header(4);
        ring(points);
      }
    }
    valid(row.geometry.size() <= 1024 * 1024,
          "OME geometry exceeds one-MiB native page");
    if (shape.attribute("Locked") && boolean(shape.get("Locked")))
      row.flags |= QPX_ANNOTATION_OBJECT_LOCKED;
    if (shape.attribute("StrokeWidth"))
      valid(real(shape.get("StrokeWidth")) >= 0, "Negative OME stroke width");
    if (shape.attribute("FontSize"))
      valid(integer(shape.get("FontSize")) >= 0, "Negative OME font size");
    // Preserve signed packed RGBA in the source tree. Native has one preferred
    // RGB, not per-stroke/fill alpha. Only map an opaque primary source color.
    const char *primary =
        shape.attribute("StrokeColor") ? "StrokeColor" : "FillColor";
    for (const char *key : {"StrokeColor", "FillColor"})
      if (shape.attribute(key)) {
        const auto v = integer(shape.get(key));
        valid(v >= INT32_MIN && v <= INT32_MAX,
              "OME color must be signed packed RGBA Int32");
        const auto rgba = std::uint32_t(v);
        if (std::string_view(key) == primary && (rgba & 255) == 255) {
          row.flags |= QPX_ANNOTATION_OBJECT_COLOR_RGB;
          row.color = rgba >> 8;
        }
      }
    std::int32_t *planes[] = {&row.c, &row.z, &row.t};
    const char *keys[] = {"TheC", "TheZ", "TheT"};
    for (unsigned i = 0; i < 3; ++i)
      if (shape.attribute(keys[i])) {
        const auto v = integer(shape.get(keys[i]));
        valid(v >= 0 && v < 65536 &&
                  (!dimensions[i] || std::uint64_t(v) < dimensions[i]),
              "OME shape plane is outside source dimensions/profile");
        valid(i != 0 || !bundled_channels,
              "OME TheC with bundled sample channels requires an explicit "
              "component mapping; unsupported profile");
        *planes[i] = std::int32_t(v);
      }
  }
  void run() {
    discover();
    for (auto roi : selected) {
      poll(cancel);
      const auto &source_roi = node(roi);
      attributes(source_roi, "|ID|Name|");
      id(required(source_roi, "ID"));
      whitespace(source_roi);
      std::size_t union_index = SIZE_MAX;
      for (auto child : source_roi.children) {
        if (ome(node(child).name, "Union")) {
          valid(union_index == SIZE_MAX, "Repeated OME ROI Union");
          union_index = child;
        } else
          valid(ome(node(child).name, "Description") ||
                    ome(node(child).name, "AnnotationRef"),
                "Unsupported OME ROI child/profile");
      }
      valid(union_index != SIZE_MAX && !node(union_index).children.empty(),
            "OME ROI requires a nonempty Union");
      attributes(node(union_index), "|");
      whitespace(node(union_index));
      for (auto shape : node(union_index).children) {
        valid(h.rows.size() < 100000, "OME shape count exceeds profile");
        h.rows.emplace_back(r);
        auto &row = h.rows.back();
        const auto &s = node(shape);
        id(required(s, "ID"));
        row.id = s.get("ID");
        row.name = s.attribute("Text")            ? s.get("Text")
                   : source_roi.attribute("Name") ? source_roi.get("Name")
                                                  : std::string_view(row.id);
        valid(row.name.size() <= 4096,
              "OME display name exceeds native bounds");
        geometry(row, s);
        fact(row.facts, "ome.namespace", ns);
        fact(row.facts, "ome.roi_id", source_roi.get("ID"));
        fact(row.facts, "ome.shape_id", row.id);
        fact(row.facts, "ome.shape_type", local(s.name));
        fact(row.facts, "ome.coordinate_profile",
             "level-zero-image-pixels/binary64");
        fact(row.facts, "ome.label_profile",
             ome(s.name, "Label") ? "anchor-and-text/source-font-metadata"
                                  : "not-label");
        fact(row.facts, "ome.selection",
             options.unbound     ? "unbound-rois"
             : image == SIZE_MAX ? "roi-only-document"
                                 : "source-image-roi-refs");
        fact(row.facts, "ome.image_id",
             image == SIZE_MAX ? std::string_view{} : node(image).get("ID"));
        number(row.facts, "ome.source_images", images.size());
        number(row.facts, "ome.source_rois", rois.size());
        number(row.facts, "ome.selected_rois", selected.size());
        number(row.facts, "ome.unselected_rois", rois.size() - selected.size());
        attrs(row.facts, "ome.roi.attributes", source_roi);
        for (auto child : source_roi.children)
          if (ome(node(child).name, "Description"))
            chunk_tree(row.facts, "ome.roi.description", child);
        if(ome(s.name,"Mask")) {
          attrs(row.facts,"ome.shape.attributes",s);
          for(auto child:s.children)if(ome(node(child).name,"Transform"))
            attrs(row.facts,"ome.mask.transform",node(child));
        }else chunk_tree(row.facts, "ome.shape.tree", shape);
        if (image != SIZE_MAX) {
          attrs(row.facts, "ome.image.attributes", node(image));
          attrs(row.facts, "ome.pixels.attributes", node(pixels));
          std::size_t channel = 0;
          for (auto child : node(pixels).children)
            if (ome(node(child).name, "Channel"))
              attrs(row.facts, "ome.channel." + std::to_string(channel++),
                    node(child));
          number(row.facts, "ome.channels", channel);
        }
        references(row.facts, roi, shape);
      }
    }
  }
};
struct Definition {
  static constexpr const char *id = "org.qupathxx.ome-roi";
  static constexpr const char *format = "ome-roi";
  static constexpr const char *name = "OME ROI XML";
  static constexpr const char *contract = "ome-2016-06-vector-mask-roi/4";
  static constexpr const char *profile = QPX_FORMAT_PLUGIN_PROFILE_ID;
  static constexpr char version[] = QPX_FORMAT_PLUGIN_PROVIDER_VERSION;
  static void validate_options(qpx_reader_string_v1 options) {
    require(options.size <= 4096, "OME options exceed 4096 bytes");
  }
  static void decode(Handle &h, Bytes input,
                     const qpx_reader_open_v1 &request) {
    OptionsReader reader{{request.options.data ? request.options.data : "",
                          std::size_t(request.options.size)},
                         0,
                         &h.arena.memory};
    const auto options = reader.parse();
    Xml xml(&h.arena.memory, request.cancel);
    xml.parse(input);
    valid(ome(xml.nodes[0].name, "OME"), "Expected OME 2016-06 namespace/root");
    Decoder decoder(h, xml, options, request.cancel);
    decoder.run();
  }
};
} // namespace qpx_ome
extern "C" QPX_ANNOTATION_MASK_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL
qpx_annotation_format_entry_v5(std::uint32_t abi,
                               qpx_annotation_format_api_v5 *out,
                               qpx_reader_text_v1 *error) {
  return qpx_annotation_source::Plugin<qpx_ome::Definition, 5,qpx_ome::Definition::version>::entry(abi, out,
                                                                      error);
}

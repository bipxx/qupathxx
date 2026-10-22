#include "qupathxx/interchange.hpp"
#include "atomic_export.hpp"
#include "qupath_precision.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <random>
#include <sstream>
#include <stdexcept>
#ifdef QUPATHXX_HAS_INTERCHANGE
#include <nlohmann/json.hpp>
#endif

namespace qpx {
namespace {
namespace fs=std::filesystem;
constexpr std::size_t byte_limit=256*1024*1024;
void valid_text(const std::string& s,std::size_t limit=4096) {
    if(s.size()>limit || s.find('\0')!=std::string::npos) throw std::invalid_argument("invalid interchange text");
}
std::vector<std::uint64_t> selected(const Document& doc,const std::vector<std::uint64_t>& ids) {
    if(ids.empty()) { std::vector<std::uint64_t> result; result.reserve(doc.object_count()); for(const auto& [id,a]:doc.objects()) result.push_back(id); return result; }
    auto result=ids; std::sort(result.begin(),result.end()); result.erase(std::unique(result.begin(),result.end()),result.end());
    for(auto id:result) if(!doc.objects().count(id)) throw std::out_of_range("export object does not exist");
    return result;
}
const char* object_kind(ObjectKind kind) { return kind==ObjectKind::Cell?"cell":(kind==ObjectKind::Detection?"detection":"annotation"); }
const char* roi_kind(RoiKind kind) {
    switch(kind) { case RoiKind::Rectangle:return "rectangle"; case RoiKind::Ellipse:return "ellipse"; case RoiKind::Polygon:return "polygon";
        case RoiKind::MultiPolygon:return "multipolygon"; case RoiKind::Polyline:return "polyline"; case RoiKind::Points:return "points"; }
    throw std::invalid_argument("unknown ROI kind");
}
void bounded(std::ostringstream& out) { if(out.tellp()>static_cast<std::streamoff>(byte_limit)) throw std::length_error("export exceeds 256 MiB limit"); }
void atomic_write(const std::string& path,const std::string& bytes) {
    valid_text(path,32768); if(path.empty()) throw std::invalid_argument("export path must not be empty");
    if(bytes.size()>byte_limit) throw std::length_error("export exceeds 256 MiB limit");
    detail::AtomicExportFile file(path,byte_limit);file.write(bytes.data(),bytes.size());file.commit();
}

std::string read_file(const std::string& path) {
    valid_text(path,32768); auto file=fs::u8path(path); auto count=fs::file_size(file);
    if(count>byte_limit) throw std::length_error("GeoJSON exceeds 256 MiB limit");
    std::ifstream input(file,std::ios::binary); if(!input) throw std::runtime_error("cannot open GeoJSON");
    std::string result(static_cast<std::size_t>(count),'\0');
    if(!input.read(result.data(),static_cast<std::streamsize>(count)) || input.peek()!=std::char_traits<char>::eof()) throw std::runtime_error("GeoJSON changed during read");
    return result;
}
void csv_text(std::ostream& out,const std::string& text) { out<<'"'; for(char c:text) { if(c=='"') out<<'"'; out<<c; } out<<'"'; }
}
bool has_interchange() {
#ifdef QUPATHXX_HAS_INTERCHANGE
    return true;
#else
    return false;
#endif
}
std::string measurements_csv(const Document& doc,const std::vector<std::uint64_t>& ids,bool intensity) {
    auto objects=selected(doc,ids); std::ostringstream out; out.imbue(std::locale::classic()); out<<std::setprecision(17);
    out<<"object_id,parent_id,name,object_type,class_name,roi_type,x_px,y_px,width_px,height_px,area_px2,length_px,area_um2,nucleus_area_px2,nucleus_area_um2,mean_rgb,nucleus_mean_rgb\r\n";
    const bool calibrated=doc.pixel_width_um().has_value() && doc.pixel_height_um().has_value();
    for(auto id:objects) {
        const auto& a=doc.objects().at(id); auto b=a.geometry.bounds();
        out<<id<<','<<a.parent_id<<','; csv_text(out,a.name); out<<','<<object_kind(a.kind)<<','; csv_text(out,a.class_name);
        out<<','<<roi_kind(a.geometry.kind())<<','<<b.x<<','<<b.y<<','<<b.width<<','<<b.height<<','<<a.geometry.area()<<','<<a.geometry.perimeter()<<',';
        if(calibrated) { auto value=doc.area_um2(id); if(!std::isfinite(value)) throw std::overflow_error("physical area exceeds numeric range"); out<<value; } out<<',';
        if(a.nucleus) out<<a.nucleus->area(); out<<',';
        if(a.nucleus && calibrated) { auto value=doc.nucleus_area_um2(id); if(!std::isfinite(value)) throw std::overflow_error("physical nucleus area exceeds numeric range"); out<<value; } out<<',';
        if(intensity && a.geometry.is_area()) out<<doc.mean_intensity(id); out<<',';
        if(intensity && a.nucleus) out<<doc.nucleus_mean_intensity(id); out<<"\r\n"; bounded(out);
    }
    return out.str();
}
void save_measurements_csv(const Document& doc,const std::string& path,const std::vector<std::uint64_t>& ids,bool intensity) { atomic_write(path,measurements_csv(doc,ids,intensity)); }
InterchangeReport save_geojson(const Document& doc,const std::string& path,const std::vector<std::uint64_t>& ids,bool losses) {
    auto report=export_geojson(doc,ids,losses); atomic_write(path,report.text); return report;
}
InterchangeReport load_geojson(Document& doc,const std::string& path,bool pixels,bool losses,bool qupath_precision) { return import_geojson(doc,read_file(path),pixels,losses,qupath_precision); }
#ifndef QUPATHXX_HAS_INTERCHANGE
InterchangeReport export_geojson(const Document&,const std::vector<std::uint64_t>&,bool) { throw std::runtime_error("GeoJSON requires QUPATHXX_BUILD_INTERCHANGE=ON"); }
InterchangeReport import_geojson(Document&,const std::string&,bool,bool,bool) { throw std::runtime_error("GeoJSON requires QUPATHXX_BUILD_INTERCHANGE=ON"); }
#else
namespace {
using Json=nlohmann::json;
// Validate keys/depth in a streaming pass. The library's DOM callback parser
// scans the parent array after each object, making large feature arrays quadratic.
// A plain DOM parse following this pass retains strict input checks and linear scans.
struct StrictJson : nlohmann::json_sax<Json> {
    struct Frame { bool object; std::set<std::string> keys; };
    std::vector<Frame> frames;
    bool null() override { return true; }
    bool boolean(bool) override { return true; }
    bool number_integer(number_integer_t) override { return true; }
    bool number_unsigned(number_unsigned_t) override { return true; }
    bool number_float(number_float_t,const string_t&) override { return true; }
    bool string(string_t&) override { return true; }
    bool binary(binary_t&) override { return true; }
    bool start(bool object) {
        if(frames.size()>=128) throw std::length_error("GeoJSON nesting exceeds 128 levels");
        frames.push_back({object,{}}); return true;
    }
    bool start_object(std::size_t) override { return start(true); }
    bool start_array(std::size_t) override { return start(false); }
    bool key(string_t& key) override {
        if(!frames.back().keys.insert(key).second) throw std::invalid_argument("duplicate JSON member");
        return true;
    }
    bool end_object() override { frames.pop_back(); return true; }
    bool end_array() override { frames.pop_back(); return true; }
    bool parse_error(std::size_t,const std::string&,const nlohmann::detail::exception& error) override {
        throw std::invalid_argument(std::string("invalid GeoJSON: ")+error.what());
    }
};
struct Notices {
    bool allowed;
    std::map<std::string,InterchangeIssue> values;
    void add(std::string code,std::string message,bool loss=false) {
        if(loss && !allowed) throw std::invalid_argument(message+"; enable allow_losses to proceed");
        auto& item=values[code]; item.code=std::move(code); item.message=std::move(message); ++item.count;
    }
    std::vector<InterchangeIssue> finish() const { std::vector<InterchangeIssue> result; for(const auto& [key,value]:values) result.push_back(value); return result; }
};
double real(const Json& value) { if(!value.is_number()) throw std::invalid_argument("coordinate must be numeric"); auto v=value.get<double>(); if(!std::isfinite(v)) throw std::invalid_argument("coordinate must be finite"); return v; }
std::string text(const Json& value) { if(!value.is_string()) throw std::invalid_argument("expected a JSON string"); auto result=value.get<std::string>(); valid_text(result); return result; }
std::string identifier(const Json& value) {
    if(value.is_string()) { auto result=text(value); if(result.empty()) throw std::invalid_argument("empty feature ID"); return result; }
    if(value.is_number_unsigned()) return std::to_string(value.get<std::uint64_t>());
    if(value.is_number_integer() && value.get<std::int64_t>()>=0) return std::to_string(value.get<std::int64_t>());
    throw std::invalid_argument("feature IDs must be strings or nonnegative integers");
}
Json point(Point p) { return Json::array({p.x,p.y}); }
Json ring(Ring points,bool shell) {
    long double area=0;
    for(std::size_t i=0;i<points.size();++i) { auto a=points[i],b=points[(i+1)%points.size()]; area+=(static_cast<long double>(a.x)-points[0].x)*(b.y-points[0].y)-(static_cast<long double>(b.x)-points[0].x)*(a.y-points[0].y); }
    if((area>0)!=shell) std::reverse(points.begin(),points.end());
    Json result=Json::array(); for(auto p:points) result.push_back(point(p)); result.push_back(point(points.front())); return result;
}
Json geometry(const Roi& roi,Notices& notices) {
    Json out;
    if(roi.kind()==RoiKind::Points) {
        auto points=roi.paths().front(); out["type"]=points.size()==1?"Point":"MultiPoint";
        if(points.size()==1) out["coordinates"]=point(points[0]);
        else { out["coordinates"]=Json::array(); for(auto p:points) out["coordinates"].push_back(point(p)); }
    } else if(roi.kind()==RoiKind::Polyline) { out["type"]="LineString"; out["coordinates"]=Json::array(); auto paths=roi.paths(); for(auto p:paths.front()) out["coordinates"].push_back(point(p)); }
    else {
        auto parts=roi.polygon_parts(); if(parts.empty()) parts.emplace_back(roi.paths().front());
        Json polygons=Json::array();
        for(const auto& p:parts) { Json rings=Json::array({ring(p.shell,true)}); for(const auto& h:p.holes) rings.push_back(ring(h,false)); polygons.push_back(std::move(rings)); }
        out["type"]=parts.size()==1?"Polygon":"MultiPolygon"; out["coordinates"]=parts.size()==1?polygons[0]:polygons;
        if(roi.kind()==RoiKind::Rectangle || roi.kind()==RoiKind::Ellipse) {
            auto b=roi.bounds(); out["qupathxx"]={{"kind",roi_kind(roi.kind())},{"bounds",Json::array({b.x,b.y,b.width,b.height})}};
            if(roi.kind()==RoiKind::Ellipse) { out["isEllipse"]=true; notices.add("ellipse_polygonized","Ellipse exported as a 512-edge polygon with an analytic hint",true); }
        }
    }
    return out;
}
std::uint32_t color(const Json& value) {
    if(value.is_array() && value.size()==3) {
        std::uint32_t result=0;
        for(const auto& channel:value) { if(!channel.is_number_integer()) throw std::invalid_argument("class color channels must be integers"); auto c=channel.get<std::int64_t>(); if(c<0 || c>255) throw std::invalid_argument("class color channel out of range"); result=(result<<8)|static_cast<std::uint32_t>(c); }
        return result;
    }
    if(value.is_number_integer()) {
        if(value.is_number_unsigned() && value.get<std::uint64_t>()>0xffffff) throw std::invalid_argument("class color out of range");
        auto c=value.get<std::int64_t>(); if(c<std::numeric_limits<std::int32_t>::min() || c>0xffffff) throw std::invalid_argument("class color out of range");
        if(c<0 && (static_cast<std::uint32_t>(c)>>24)!=255) throw std::invalid_argument("transparent packed class colors are not supported");
        return static_cast<std::uint32_t>(c)&0xffffff;
    }
    throw std::invalid_argument("class color must be RGB or a packed integer");
}
struct Reader {
    const Document& document; Notices notices;
    std::vector<Annotation> objects; std::vector<std::string> source_ids;
    std::map<std::string,std::uint64_t> identifiers;
    std::vector<std::pair<std::size_t,std::string>> parents;
    std::map<std::string,ObjectClass> classes;
    std::size_t vertex_count=0;
    bool qupath_precision=false;
    Point point(const Json& value) {
        if(!value.is_array() || value.size()!=2) throw std::invalid_argument("only 2D XY coordinates are supported");
        return {real(value[0]),real(value[1])};
    }
    Ring points(const Json& value,bool closed=false) {
        if(!value.is_array() || value.size()>1000001) throw std::invalid_argument("invalid coordinate sequence");
        auto count=value.size()-(closed && !value.empty()?1:0);
        if(count>1000000 || vertex_count>1000000-count) throw std::length_error("geometry exceeds one million vertices");
        vertex_count+=count;
        Ring result; result.reserve(value.size()); for(const auto& p:value) result.push_back(point(p));
        if(closed) {
            if(result.size()<4 || result.front().x!=result.back().x || result.front().y!=result.back().y) throw std::invalid_argument("polygon rings must be explicitly closed");
            result.pop_back();
        }
        return result;
    }
    PolygonPart polygon(const Json& value) {
        if(!value.is_array() || value.empty()) throw std::invalid_argument("polygon requires a shell");
        auto shell=points(value[0],true); std::vector<Ring> holes;
        for(std::size_t i=1;i<value.size();++i) holes.push_back(points(value[i],true)); return {std::move(shell),std::move(holes)};
    }
    Roi geometry(const Json& value) {
        vertex_count=0;
        if(!value.is_object() || !value.contains("type") || !value.contains("coordinates")) throw std::invalid_argument("geometry is missing type/coordinates");
        for(const auto& member:value.items()) if(member.key()!="type" && member.key()!="coordinates" && member.key()!="plane" && member.key()!="qupathxx" && member.key()!="isEllipse" && member.key()!="bbox")
            notices.add("geometry_members_dropped","Unsupported foreign geometry members will be dropped",true);
        if(value.contains("plane")) {
            const auto& plane=value["plane"];
            if(!plane.is_object()) throw std::invalid_argument("invalid image plane");
            for(const auto& item:plane.items()) if((item.key()!="c" && item.key()!="z" && item.key()!="t") || !item.value().is_number_integer() || item.value()!=(item.key()=="c"?-1:0))
                throw std::invalid_argument("non-default Z/T/channel planes are not supported");
        }
        auto type=text(value["type"]); const auto& c=value["coordinates"];
        if(type=="Point") return Roi::points({point(c)});
        if(type=="MultiPoint") return Roi::points(points(c));
        if(type=="LineString") return Roi::polyline(points(c));
        if(type!="Polygon" && type!="MultiPolygon") throw std::invalid_argument("unsupported geometry type: "+type);
        if(value.contains("qupathxx")) {
            const auto& hint=value["qupathxx"];
            if(!hint.is_object() || !hint.contains("kind") || !hint.contains("bounds") || !hint["bounds"].is_array() || hint["bounds"].size()!=4 || type!="Polygon") throw std::invalid_argument("invalid analytic ROI hint");
            for(const auto& member:hint.items()) if(member.key()!="kind" && member.key()!="bounds") notices.add("extensions_dropped","Unsupported native extension fields will be dropped",true);
            auto kind=text(hint["kind"]); const auto& b=hint["bounds"]; Rect bounds{real(b[0]),real(b[1]),real(b[2]),real(b[3])};
            auto roi=kind=="rectangle"?Roi::rectangle(bounds):(kind=="ellipse"?Roi::ellipse(bounds):throw std::invalid_argument("unknown analytic ROI hint"));
            auto p=polygon(c); auto expected=roi.paths().front();
            if(!p.holes.empty() || p.shell.size()!=expected.size()) throw std::invalid_argument("analytic hint disagrees with geometry");
            auto matches=[&](bool reverse) {
                for(std::size_t start=0;start<expected.size();++start) {
                    bool valid=true;
                    for(std::size_t i=0;i<expected.size();++i) { auto a=p.shell[i],b=expected[(start+(reverse?expected.size()-i:i))%expected.size()]; if(a.x!=b.x || a.y!=b.y) { valid=false; break; } }
                    if(valid) return true;
                }
                return false;
            };
            if(!matches(false) && !matches(true)) throw std::invalid_argument("analytic hint disagrees with coordinates");
            return roi;
        }
        std::vector<PolygonPart> parts;
        if(type=="Polygon") parts.push_back(polygon(c));
        else { if(!c.is_array() || c.empty()) throw std::invalid_argument("empty multipolygon"); for(const auto& p:c) parts.push_back(polygon(p)); }
        if(value.value("isEllipse",false)) {
            if(parts.size()!=1 || !parts[0].holes.empty()) throw std::invalid_argument("invalid QuPath ellipse hint");
            auto p=parts[0].shell; double x=p[0].x,y=p[0].y,r=x,b=y;
            for(auto v:p) { x=std::min(x,v.x); y=std::min(y,v.y); r=std::max(r,v.x); b=std::max(b,v.y); }
            Rect bounds{x,y,r-x,b-y};
            if(qupath_precision) {
                const auto reduced=detail::qupath_ellipse_bounds(p);
                if(reduced.x!=bounds.x || reduced.y!=bounds.y || reduced.width!=bounds.width || reduced.height!=bounds.height)
                    notices.add("qupath_ellipse_precision_changed","Ellipse bounds changed to match QuPath's precision-reduced float envelope",true);
                bounds=reduced;
            }
            notices.add("ellipse_hint","Ellipse reconstructed from QuPath's polygon envelope"); return Roi::ellipse(bounds);
        }
        return Roi::multipolygon(std::move(parts));
    }
    void definition(const std::string& name,std::uint32_t rgb) {
        if(name.empty()) throw std::invalid_argument("class name must not be empty"); valid_text(name);
        auto existing=classes.find(name); if(existing!=classes.end() && existing->second.color!=rgb) throw std::invalid_argument("conflicting class colors in import");
        classes.insert_or_assign(name,ObjectClass{name,rgb});
    }
    void feature(const Json& value,std::uint64_t parent=0,int depth=0) {
        if(depth>256 || objects.size()>=1000000 || !value.is_object() || value.value("type","")!="Feature") throw std::invalid_argument("invalid feature or hierarchy limit");
        auto roi=geometry(value.at("geometry"));
        for(const auto& member:value.items()) if(member.key()!="type" && member.key()!="id" && member.key()!="geometry" && member.key()!="properties" && member.key()!="nucleusGeometry" && member.key()!="bbox")
            notices.add("feature_members_dropped","Unsupported foreign feature members will be dropped",true);
        Json empty=Json::object(); const auto& properties=value.contains("properties") && !value["properties"].is_null()?value["properties"]:empty;
        if(!properties.is_object()) throw std::invalid_argument("properties must be an object or null");
        auto type=properties.contains("objectType")?text(properties["objectType"]):"annotation";
        auto kind=type=="annotation"?ObjectKind::Annotation:(type=="detection"?ObjectKind::Detection:(type=="cell"?ObjectKind::Cell:throw std::invalid_argument("unsupported objectType: "+type)));
        std::optional<Roi> nucleus; if(value.contains("nucleusGeometry") && !value["nucleusGeometry"].is_null()) nucleus=geometry(value["nucleusGeometry"]);
        std::string name=properties.contains("name")?text(properties["name"]):"Annotation",classification;
        if(properties.contains("classification") && !properties["classification"].is_null()) {
            const auto& c=properties["classification"];
            if(!c.is_object() || !c.contains("name")) throw std::invalid_argument("only flat named classifications are supported");
            for(const auto& member:c.items()) if(member.key()!="name" && member.key()!="color") notices.add("classification_members_dropped","Unsupported classification fields will be dropped",true);
            classification=text(c["name"]);
            auto existing=document.classes().find(classification);
            auto rgb=c.contains("color")?color(c["color"]):(existing==document.classes().end()?ObjectClass{}.color:existing->second.color);
            definition(classification,rgb);
        }
        for(const auto& p:properties.items()) if(p.key()!="name" && p.key()!="objectType" && p.key()!="classification" && p.key()!="qupathxx" && p.key()!="childObjects" && p.key()!="children")
            notices.add("properties_dropped","Unsupported properties (measurements, metadata, locks or custom fields) will be dropped",true);
        std::string source=value.contains("id")?identifier(value["id"]):"";
        auto local=static_cast<std::uint64_t>(objects.size()+1);
        if(!source.empty() && !identifiers.emplace(source,local).second) throw std::invalid_argument("duplicate feature ID");
        objects.push_back({local,std::move(name),std::move(roi),parent,std::move(classification),0,kind,std::move(nucleus)}); source_ids.push_back(source);
        if(properties.contains("qupathxx")) {
            const auto& q=properties["qupathxx"];
            if(!q.is_object()) throw std::invalid_argument("invalid object extension");
            for(const auto& member:q.items()) if(member.key()!="parentId") notices.add("extensions_dropped","Unsupported native extension fields will be dropped",true);
            if(q.contains("parentId") && !q["parentId"].is_null()) {
                if(parent) throw std::invalid_argument("nested and explicit parent both specified");
                auto id=identifier(q["parentId"]); if(id!="0") parents.emplace_back(objects.size()-1,std::move(id));
            }
        }
        if(properties.contains("childObjects") && properties.contains("children")) throw std::invalid_argument("both children aliases specified");
        auto children=properties.find("childObjects"); if(children==properties.end()) children=properties.find("children");
        if(children!=properties.end()) { if(!children->is_array()) throw std::invalid_argument("children must be an array"); for(const auto& child:*children) feature(child,local,depth+1); }
    }
};
}
InterchangeReport export_geojson(const Document& doc,const std::vector<std::uint64_t>& ids,bool losses) {
    auto objects=selected(doc,ids); std::set<std::uint64_t> exported(objects.begin(),objects.end()); Notices notices{losses,{}};
    Json metadata={{"version",1},{"coordinateSpace","image-pixels"},{"origin","top-left"},{"imageWidth",doc.metadata().width},{"imageHeight",doc.metadata().height},{"classes",Json::array()}};
    std::set<std::string> used; for(auto id:objects) used.insert(doc.objects().at(id).class_name);
    for(const auto& [name,c]:doc.classes()) if(ids.empty() || used.count(name)) metadata["classes"].push_back({{"name",name},{"color",c.color}});
    std::ostringstream out; out.imbue(std::locale::classic()); out<<"{\"type\":\"FeatureCollection\",\"qupathxx\":"<<metadata.dump()<<",\"features\":[";
    bool first=true;
    for(auto id:objects) {
        const auto& a=doc.objects().at(id); Json properties={{"name",a.name},{"objectType",object_kind(a.kind)}};
        if(!a.class_name.empty()) { auto c=doc.object_class(a.class_name).color; properties["classification"]={{"name",a.class_name},{"color",Json::array({(c>>16)&255,(c>>8)&255,c&255})}}; }
        if(a.cell_feature_link) notices.add("cell_features_dropped","Cell feature links and run archives are not represented in GeoJSON",true);
        auto parent=a.parent_id; if(parent && !exported.count(parent)) { notices.add("parent_not_exported","Parent omitted because it is not in the exported subset",true); parent=0; }
        properties["qupathxx"]={{"parentId",std::to_string(parent)}};
        Json feature={{"type","Feature"},{"id",std::to_string(id)},{"geometry",geometry(a.geometry,notices)},{"properties",std::move(properties)}};
        if(a.nucleus) feature["nucleusGeometry"]=geometry(*a.nucleus,notices);
        if(!first) out<<','; first=false; out<<feature.dump(); bounded(out);
    }
    out<<"]}\n"; bounded(out); InterchangeReport report; report.text=out.str(); report.features=objects.size(); report.issues=notices.finish(); return report;
}
InterchangeReport import_geojson(Document& doc,const std::string& input,bool pixels,bool losses,bool qupath_precision) {
    if(input.size()>byte_limit) throw std::length_error("GeoJSON exceeds 256 MiB limit");
    if(input.find('\0')!=std::string::npos) throw std::invalid_argument("GeoJSON contains NUL bytes");
    try {
        StrictJson validator; Json::sax_parse(input,&validator);
        auto root=Json::parse(input); Reader reader{doc,Notices{losses,{}},{},{},{},{},{}}; reader.qupath_precision=qupath_precision;
        if(!root.is_object()) throw std::invalid_argument("expected a Feature or FeatureCollection");
        if(root.contains("crs") && !root["crs"].is_null()) throw std::invalid_argument("geographic/projected CRS is not supported; convert to image pixels first");
        if(root.contains("qupathxx")) {
            const auto& m=root["qupathxx"];
            if(!m.is_object() || !m.contains("version") || !m["version"].is_number_integer() || m["version"]!=1 || m.value("coordinateSpace","")!="image-pixels" || m.value("origin","")!="top-left") throw std::invalid_argument("unsupported native GeoJSON metadata");
            if(m.at("imageWidth")!=doc.metadata().width || m.at("imageHeight")!=doc.metadata().height) throw std::invalid_argument("GeoJSON image dimensions do not match document");
            for(const auto& member:m.items()) if(member.key()!="version" && member.key()!="coordinateSpace" && member.key()!="origin" && member.key()!="imageWidth" && member.key()!="imageHeight" && member.key()!="classes") reader.notices.add("extensions_dropped","Unsupported native extension fields will be dropped",true);
            if(m.contains("classes")) {
                if(!m["classes"].is_array()) throw std::invalid_argument("classes must be an array");
                for(const auto& c:m["classes"]) reader.definition(text(c.at("name")),color(c.at("color")));
            }
        } else if(!pixels) throw std::invalid_argument("untagged GeoJSON requires assume_pixels=true");
        auto type=root.value("type","");
        if(type=="FeatureCollection") {
            for(const auto& member:root.items()) if(member.key()!="type" && member.key()!="features" && member.key()!="qupathxx" && member.key()!="bbox" && member.key()!="crs") reader.notices.add("collection_members_dropped","Unsupported collection fields will be dropped",true);
            const auto& features=root.at("features"); if(!features.is_array()) throw std::invalid_argument("features must be an array"); for(const auto& f:features) reader.feature(f);
        }
        else if(type=="Feature") reader.feature(root);
        else throw std::invalid_argument("expected FeatureCollection or Feature");
        for(const auto& [index,parent]:reader.parents) { auto id=reader.identifiers.find(parent); if(id==reader.identifiers.end()) throw std::invalid_argument("parent ID is absent from imported features"); reader.objects[index].parent_id=id->second; }
        std::vector<ObjectClass> classes; for(const auto& [name,c]:reader.classes) classes.push_back(c);
        InterchangeReport report; report.features=reader.objects.size(); report.source_ids=std::move(reader.source_ids); report.issues=reader.notices.finish();
        report.imported_ids=doc.append_objects(reader.objects,classes);
        return report;
    } catch(const Json::exception& error) { throw std::invalid_argument(std::string("invalid GeoJSON: ")+error.what()); }
}
#endif
}

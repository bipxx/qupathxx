#include "qupath_saved_data.hpp"
#include <stdexcept>
#if defined(QUPATHXX_HAS_PROJECT) && defined(QUPATHXX_HAS_INTERCHANGE)
#include "strict_json.hpp"
#include "utf8_text.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>
#include <string_view>

// Data-only reader for the pinned QuPath ObjectOutputStream representation.
// Oracle serialization protocol chapter 6 defines the framing/handles. No Java
// classes, constructors, readObject/readResolve methods or scripts are executed.
namespace qpx::detail {
namespace {
using Json=nlohmann::json;
using Id=std::size_t;
[[noreturn]] void invalid(const std::string& message) {throw std::invalid_argument("QuPath saved data: "+message);}
struct Node {
    enum Kind {Null,Text,Primitive,Descriptor,Object,Array,Block,Enum} kind=Null;
    std::string text,bytes;
    char type=0;std::uint64_t bits=0;
    Id descriptor=0,parent=0;unsigned flags=0;
    std::vector<std::pair<char,std::string>> schema;
    std::map<std::string,Id> fields;
    std::vector<Id> values;
    std::map<std::string,std::vector<Id>> extras;
};
class Stream {
    std::string_view input;std::size_t offset=0,allocated=0;unsigned depth=0;
    std::vector<Id> handles;const std::atomic<bool>* cancelled;
    void charge(std::size_t n) {if(n>256ull*1024*1024-allocated)invalid("decoded storage exceeds 256 MiB");allocated+=n;}
    void poll() const {if(cancelled && cancelled->load())throw std::runtime_error("QuPath import cancelled");}
    Id add(Node value,bool handle=false) {
        if(nodes.size()>=1000000)invalid("too many serialized values");
        charge(sizeof(Node)+value.text.size()+value.bytes.size());nodes.push_back(std::move(value));
        const auto id=nodes.size()-1;if(handle)handles.push_back(id);return id;
    }
    std::string take(std::size_t n) {
        poll();if(n>input.size()-offset)invalid("truncated stream");
        std::string out(input.substr(offset,n));offset+=n;return out;
    }
    std::string utf(std::size_t n) {
        const auto bytes=take(n);std::vector<std::uint16_t> units;units.reserve(n);
        for(std::size_t i=0;i<n;) {
            unsigned a=static_cast<unsigned char>(bytes[i++]),u=0;
            if(a>0 && a<128)u=a;
            else if((a&0xe0)==0xc0) {
                if(i>=n)invalid("truncated modified UTF-8");const unsigned b=static_cast<unsigned char>(bytes[i++]);
                if((b&0xc0)!=0x80)invalid("invalid modified UTF-8");u=((a&31)<<6)|(b&63);
                if(u<128 && !(a==0xc0 && b==0x80))invalid("overlong modified UTF-8");
            } else if((a&0xf0)==0xe0) {
                if(n-i<2)invalid("truncated modified UTF-8");const unsigned b=static_cast<unsigned char>(bytes[i++]),c=static_cast<unsigned char>(bytes[i++]);
                if((b&0xc0)!=0x80 || (c&0xc0)!=0x80)invalid("invalid modified UTF-8");u=((a&15)<<12)|((b&63)<<6)|(c&63);if(u<2048)invalid("overlong modified UTF-8");
            } else invalid("invalid modified UTF-8 byte");
            units.push_back(std::uint16_t(u));
        }
        std::string out;
        for(std::size_t i=0;i<units.size();++i) {
            unsigned u=units[i];
            if(u>=0xd800 && u<=0xdbff) {if(i+1==units.size() || units[i+1]<0xdc00 || units[i+1]>0xdfff)invalid("unpaired UTF-16 surrogate");u=0x10000+((u-0xd800)<<10)+(units[++i]-0xdc00);}
            else if(u>=0xdc00 && u<=0xdfff)invalid("unpaired UTF-16 surrogate");
            if(u<128)out+=char(u);
            else if(u<2048) {out+=char(0xc0|(u>>6));out+=char(0x80|(u&63));}
            else if(u<65536) {out+=char(0xe0|(u>>12));out+=char(0x80|((u>>6)&63));out+=char(0x80|(u&63));}
            else {out+=char(0xf0|(u>>18));out+=char(0x80|((u>>12)&63));out+=char(0x80|((u>>6)&63));out+=char(0x80|(u&63));}
        }
        return out;
    }
    std::string short_utf() {return utf(std::size_t(integer(2)));}
    static unsigned width(char type) {
        switch(type) {case 'B':case 'Z':return 1;case 'C':case 'S':return 2;case 'F':case 'I':return 4;case 'D':case 'J':return 8;default:invalid("unknown primitive field type");}
    }
    Id primitive(char type) {Node n;n.kind=Node::Primitive;n.type=type;n.bits=integer(width(type));return add(std::move(n));}
    std::vector<Id> annotation() {
        std::vector<Id> out;
        while(true) {if(offset==input.size())invalid("unterminated object data");if(static_cast<unsigned char>(input[offset])==0x78) {++offset;return out;}charge(sizeof(Id));out.push_back(value());}
    }
    Id class_descriptor() {auto id=value();if(id && nodes[id].kind!=Node::Descriptor)invalid("class descriptor expected");return id;}
    void fields(Id object,Id descriptor) {
        if(!descriptor) return;
        const auto parent=nodes[descriptor].parent;fields(object,parent);
        const auto schema=nodes[descriptor].schema;
        for(const auto& [type,name]:schema) {
            Id id=(type=='L' || type=='[')?value():primitive(type);
            if(!nodes[object].fields.emplace(name,id).second)invalid("shadowed serialized field");charge(name.size()+64);
        }
        if(nodes[descriptor].flags&1) {auto extra=annotation();nodes[object].extras.emplace(nodes[descriptor].text,std::move(extra));}
    }
public:
    std::vector<Node> nodes{Node{}};
    Stream(const std::string& bytes,const std::atomic<bool>* flag):input(bytes),cancelled(flag) {
        if(bytes.size()>256ull*1024*1024)invalid("input exceeds 256 MiB");
        if(integer(2)!=0xaced || integer(2)!=5)invalid("unsupported stream header");
    }
    std::uint64_t integer(unsigned n) {poll();if(n>input.size()-offset)invalid("truncated stream");std::uint64_t v=0;while(n--)v=(v<<8)|static_cast<unsigned char>(input[offset++]);return v;}
    bool done() const {return offset==input.size();}
    Id value() {
        if(depth>=128)invalid("serialization nesting exceeds 128 levels");
        ++depth;struct Pop {unsigned& value;~Pop(){--value;}} pop{depth};
        const auto token=integer(1);
        if(token==0x70)return 0;
        if(token==0x71) {auto h=integer(4);if(h<0x7e0000 || h-0x7e0000>=handles.size())invalid("invalid reference handle");return handles[std::size_t(h-0x7e0000)];}
        if(token==0x74 || token==0x7c) {auto count=integer(token==0x74?2:8);if(count>16*1024*1024)invalid("serialized string exceeds 16 MiB");Node n;n.kind=Node::Text;n.text=utf(std::size_t(count));return add(std::move(n),true);}
        if(token==0x77 || token==0x7a) {auto count=integer(token==0x77?1:4);Node n;n.kind=Node::Block;n.bytes=take(std::size_t(count));return add(std::move(n));}
        if(token==0x72) {
            Node n;n.kind=Node::Descriptor;n.text=short_utf();n.bits=integer(8);
            if(n.text.rfind("qupath.",0)==0 && n.bits!=1 && n.bits!=0)invalid("unsupported QuPath class version");
            const auto id=add(std::move(n),true);nodes[id].flags=unsigned(integer(1));const auto flags=nodes[id].flags;
            if(flags!=2 && flags!=3 && flags!=12 && flags!=18)invalid("unsupported class serialization flags");
            auto count=integer(2);if(count>256)invalid("too many class fields");
            std::set<std::string> seen;
            while(count--) {
                const auto type=char(integer(1));const auto name=short_utf();
                if(!seen.insert(name).second)invalid("duplicate class field");
                if(type=='L' || type=='[') {const auto ref=value();if(!ref || nodes[ref].kind!=Node::Text)invalid("invalid field descriptor");}
                else width(type);
                charge(name.size()+64);nodes[id].schema.emplace_back(type,name);
            }
            if(!annotation().empty())invalid("annotated class descriptors unsupported");
            const auto parent=class_descriptor();
            unsigned ancestors=0;for(auto p=parent;p;p=nodes[p].parent)if(p==id || ++ancestors>128)invalid("cyclic or excessive superclass descriptors");
            nodes[id].parent=parent;return id;
        }
        if(token==0x73 || token==0x75 || token==0x7e) {
            const auto desc=class_descriptor();if(!desc)invalid("missing object class");
            Node n;n.kind=token==0x75?Node::Array:(token==0x7e?Node::Enum:Node::Object);n.descriptor=desc;
            const auto id=add(std::move(n),true);const auto name=nodes[desc].text;
            if(token==0x7e) {if(nodes[desc].flags!=18)invalid("invalid enum class");const auto text=value();if(!text || nodes[text].kind!=Node::Text)invalid("invalid enum name");nodes[id].text=nodes[text].text;return id;}
            if(token==0x75) {
                if(name.size()<2 || name[0]!='[')invalid("invalid array descriptor");
                const auto count=integer(4);if(count>8000000)invalid("array exceeds element limit");
                if(name[1]=='L' || name[1]=='[') {
                    charge(std::size_t(count)*sizeof(Id));nodes[id].values.reserve(std::size_t(count));
                    for(std::size_t i=0;i<count;++i) {const auto item=value();nodes[id].values.push_back(item);}
                } else {const auto bytes=std::size_t(count)*width(name[1]);charge(bytes);nodes[id].bytes=take(bytes);}
            } else if(nodes[desc].flags==12) {auto extra=annotation();nodes[id].extras.emplace(name,std::move(extra));}
            else {if(nodes[desc].flags&16)invalid("enum encoded as object");fields(id,desc);}
            return id;
        }
        invalid("unsupported serialization token "+std::to_string(token));
    }
    const Node& at(Id id) const {if(id>=nodes.size())invalid("invalid value reference");return nodes[id];}
    std::string name(Id id) const {return id && at(id).descriptor?at(at(id).descriptor).text:std::string{};}
};
// Externalizable and writeObject primitive bytes may span multiple block records.
class Data {
    const Stream& stream;const std::vector<Id>& values;std::size_t pos=0,byte=0;
public:
    Data(const Stream& s,const std::vector<Id>& v):stream(s),values(v) {}
    Id object() {if(byte || pos==values.size() || stream.at(values[pos]).kind==Node::Block)invalid("object expected in external data");return values[pos++];}
    std::uint64_t integer(unsigned n) {
        std::uint64_t result=0;
        while(n--) {
            if(pos==values.size() || stream.at(values[pos]).kind!=Node::Block)invalid("primitive block expected");
            const auto& bytes=stream.at(values[pos]).bytes;
            if(byte==bytes.size()) {++pos;byte=0;++n;continue;}
            result=(result<<8)|static_cast<unsigned char>(bytes[byte++]);if(byte==bytes.size()) {++pos;byte=0;}
        }
        return result;
    }
    bool done() const {return pos==values.size() && byte==0;}
    void finish() const {if(!done())invalid("unexpected trailing external data");}
};
double floating(std::uint64_t bits,bool single=false) {
    if(single) {auto v=std::uint32_t(bits);float f;std::memcpy(&f,&v,4);return f;}
    double d;std::memcpy(&d,&bits,8);return d;
}

class Decoder {
    const Stream& stream;QuPathSavedData result;std::set<Id> objects;std::size_t count=0;
    const Node& at(Id id) const {return stream.at(id);}
    Id field(Id id,const std::string& key) const {const auto& fields=at(id).fields;const auto it=fields.find(key);if(it==fields.end())invalid("missing field "+key+" in "+stream.name(id));return it->second;}
    const std::vector<Id>& extra(Id id) const {
        const auto& n=at(id);auto found=n.extras.find(stream.name(id));if(found==n.extras.end())invalid("missing external data for "+stream.name(id));return found->second;
    }
    std::string text(Id id,bool nullable=false) const {
        if(!id && nullable)return {};
        if(at(id).kind!=Node::Text)invalid("string expected");bounded_utf8(at(id).text,4096);return at(id).text;
    }
    std::uint64_t number(Id id,char type) const {if(at(id).kind!=Node::Primitive || at(id).type!=type)invalid("primitive type mismatch");return at(id).bits;}
    std::int32_t int_field(Id id,const std::string& key) const {return std::int32_t(number(field(id,key),'I'));}
    double double_field(Id id,const std::string& key) const {const auto v=floating(number(field(id,key),'D'));if(!std::isfinite(v))invalid("non-finite coordinate");return v;}
    void issue(const std::string& code,const std::string& message) {for(auto& i:result.issues)if(i.code==code && i.message==message) {++i.count;return;}result.issues.push_back({code,message,1});}
    std::map<std::string,std::string> metadata(Id id) const {
        if(stream.name(id)!="qupath.lib.objects.MetadataMap")invalid("unsupported object metadata representation");
        Data d(stream,extra(id));const auto version=d.object();
        if(stream.name(version)!="java.lang.Integer" || number(field(version,"value"),'I')!=1)invalid("unsupported metadata version");
        const auto keys=d.object(),values=d.object();d.finish();
        if(at(keys).kind!=Node::Array || at(values).kind!=Node::Array || at(keys).values.size()!=at(values).values.size())invalid("invalid metadata arrays");
        std::map<std::string,std::string> out;
        for(std::size_t i=0;i<at(keys).values.size();++i)if(!out.emplace(text(at(keys).values[i]),text(at(values).values[i])).second)invalid("duplicate metadata key");
        return out;
    }
    std::vector<Id> list(Id id) const {
        const auto name=stream.name(id);
        if(name=="java.util.ArrayList") {
            Data d(stream,extra(id));const auto n=d.integer(4);if(n>4096 || int_field(id,"size")!=std::int32_t(n))invalid("invalid measurement list size");
            std::vector<Id> out;while(out.size()<n)out.push_back(d.object());d.finish();return out;
        }
        if(name=="java.util.CollSer") {
            if(int_field(id,"tag")!=1)invalid("unsupported measurement name collection");
            Data d(stream,extra(id));const auto n=d.integer(4);if(n>4096)invalid("measurement list exceeds 4096 entries");
            std::vector<Id> out;while(out.size()<n)out.push_back(d.object());d.finish();return out;
        }
        if(name=="java.util.Arrays$ArrayList") {const auto array=field(id,"a");if(at(array).kind!=Node::Array || at(array).values.size()>4096)invalid("invalid measurement name array");return at(array).values;}
        invalid("unsupported measurement list representation: "+name);
    }
    Json measurement_json(double v) const {
        if(std::isnan(v))return "NaN";if(std::isinf(v))return v>0?"Infinity":"-Infinity";return v;
    }
    Json measurements(Id id) const {
        Json out=Json::object();if(!id)return out;const auto name=stream.name(id);
        auto put=[&](const std::string& key,double value) {if(out.contains(key))invalid("duplicate saved measurement");out[key]=measurement_json(value);};
        if(name=="qupath.lib.measurements.DefaultMeasurementList") {
            for(auto measurement:list(field(id,"list"))) {
                const auto type=stream.name(measurement);const bool single=type=="qupath.lib.measurements.FloatMeasurement";
                if(!single && type!="qupath.lib.measurements.DoubleMeasurement")invalid("unsupported saved measurement type: "+type);
                put(text(field(measurement,"name")),floating(number(field(measurement,"value"),single?'F':'D'),single));
            }
        } else if(name=="qupath.lib.measurements.NumericMeasurementList$DoubleList" || name=="qupath.lib.measurements.NumericMeasurementList$FloatList") {
            const bool single=name.find("FloatList")!=std::string::npos;const auto names=list(field(id,"names"));const auto values=field(id,"values");
            if(stream.name(values)!=(single?"[F":"[D"))invalid("invalid numeric measurement values");const auto& bytes=at(values).bytes;const unsigned width=single?4:8;
            if(bytes.size()/width<names.size())invalid("short numeric measurement array");
            for(std::size_t i=0;i<names.size();++i) {std::uint64_t n=0;for(unsigned j=0;j<width;++j)n=(n<<8)|static_cast<unsigned char>(bytes[i*width+j]);put(text(names[i]),floating(n,single));}
        } else invalid("unsupported measurement storage: "+name);
        return out;
    }
    Json classification(Id id,unsigned depth=0) {
        if(!id)return nullptr;
        if(depth>=32 || stream.name(id)!="qupath.lib.objects.classes.PathClass")invalid("unsupported/cyclic classification");
        auto name=text(field(id,"name"),true);const auto parent=field(id,"parentClass");
        if(parent) {auto base=classification(parent,depth+1);if(!base.is_null())name=base.at("name").get<std::string>()+": "+name;issue("classification_hierarchy_flattened","Derived class retained as its full display name without class-parent semantics");}
        if(name.empty())return nullptr;
        Json out={{"name",name}};const auto color=field(id,"colorRGB");
        if(color) {if(stream.name(color)!="java.lang.Integer")invalid("invalid class color");const auto c=number(field(color,"value"),'I');out["color"]=Json::array({(c>>16)&255,(c>>8)&255,c&255});}
        return out;
    }
    Ring coordinates(Id id) const {
        const auto xs=field(id,"x"),ys=field(id,"y");
        if(stream.name(xs)!="[F" || stream.name(ys)!="[F" || at(xs).bytes.size()!=at(ys).bytes.size())invalid("invalid coordinate arrays");
        Ring points;const auto& x=at(xs).bytes;const auto& y=at(ys).bytes;
        for(std::size_t i=0;i<x.size();i+=4) {
            auto read=[&](const std::string& b){std::uint32_t bits=0;for(unsigned k=0;k<4;++k)bits=(bits<<8)|static_cast<unsigned char>(b[i+k]);const auto v=floating(bits,true);if(!std::isfinite(v))invalid("non-finite coordinate");return v;};
            points.emplace_back(read(x),read(y));
        }
        return points;
    }
    Json roi_json(const Roi& roi) const {
        Json out;auto paths=roi.paths();
        auto point=[](Point p){return Json::array({p.x,p.y});};
        if(roi.kind()==RoiKind::Points) {
            out["type"]=paths[0].size()==1?"Point":"MultiPoint";
            if(paths[0].size()==1)out["coordinates"]=point(paths[0][0]);
            else {out["coordinates"]=Json::array();for(auto p:paths[0])out["coordinates"].push_back(point(p));}
        } else if(roi.kind()==RoiKind::Polyline) {
            out["type"]="LineString";out["coordinates"]=Json::array();for(auto p:paths[0])out["coordinates"].push_back(point(p));
        } else {
            Json ring=Json::array();for(auto p:paths[0])ring.push_back(point(p));ring.push_back(point(paths[0].front()));
            out={{"type","Polygon"},{"coordinates",Json::array({ring})}};
            if(roi.kind()==RoiKind::Rectangle || roi.kind()==RoiKind::Ellipse) {const auto b=roi.bounds();out["qupathxx"]={{"kind",roi.kind()==RoiKind::Rectangle?"rectangle":"ellipse"},{"bounds",Json::array({b.x,b.y,b.width,b.height})}};}
        }
        return out;
    }
    Json geometry(Id id) const {
        if(!id)invalid("missing ROI");
        const auto c=int_field(id,"c"),z=int_field(id,"z"),t=int_field(id,"t");
        if(c!=-1 || z!=0 || t!=0)invalid("non-default Z/T/channel planes are not supported");
        const auto name=stream.name(id);
        if(name=="qupath.lib.roi.RectangleROI$SerializationProxy" || name=="qupath.lib.roi.EllipseROI$SerializationProxy") {
            const auto x=double_field(id,"x"),y=double_field(id,"y"),x2=double_field(id,"x2"),y2=double_field(id,"y2");const Rect b{x,y,x2-x,y2-y};
            return roi_json(name.find("RectangleROI")!=std::string::npos?Roi::rectangle(b):Roi::ellipse(b));
        }
        if(name=="qupath.lib.roi.PolygonROI$SerializationProxy")return roi_json(Roi::polygon(coordinates(id)));
        if(name=="qupath.lib.roi.PolylineROI$SerializationProxy")return roi_json(Roi::polyline(coordinates(id)));
        if(name=="qupath.lib.roi.PointsROI$SerializationProxy")return roi_json(Roi::points(coordinates(id)));
        if(name=="qupath.lib.roi.GeometryROI$WKBSerializationProxy") {
            const auto wkb=field(id,"wkb");if(stream.name(wkb)!="[B")invalid("invalid WKB byte array");return Wkb(at(wkb).bytes).decode();
        }
        invalid("unsupported ROI representation: "+name);
    }
    class Wkb {
        const std::string& bytes;std::size_t pos=0,points=0;unsigned depth=0;
        std::uint64_t uint(unsigned n,bool little) {
            if(n>bytes.size()-pos)invalid("truncated WKB");std::uint64_t v=0;
            for(unsigned i=0;i<n;++i) {auto b=static_cast<unsigned char>(bytes[pos++]);if(little)v|=std::uint64_t(b)<<(8*i);else v=(v<<8)|b;}return v;
        }
        Json point(bool little) {if(++points>1000000)invalid("too many WKB points");auto x=floating(uint(8,little)),y=floating(uint(8,little));if(!std::isfinite(x) || !std::isfinite(y))invalid("non-finite WKB point");return Json::array({x,y});}
        Json sequence(bool little) {const auto n=uint(4,little);if(n>1000000-points || n>(bytes.size()-pos)/16)invalid("invalid WKB coordinate count");Json out=Json::array();for(std::size_t i=0;i<n;++i)out.push_back(point(little));return out;}
        Json value() {
            if(depth>=32)invalid("WKB nesting exceeds 32 levels");++depth;
            const auto endian=uint(1,false);if(endian>1)invalid("invalid WKB byte order");const bool little=endian==1;
            const auto type=uint(4,little);Json out;
            if(type==1)out={{"type","Point"},{"coordinates",point(little)}};
            else if(type==2)out={{"type","LineString"},{"coordinates",sequence(little)}};
            else if(type==3) {auto n=uint(4,little);if(n>1000000 || n>(bytes.size()-pos)/4)invalid("invalid WKB ring count");Json rings=Json::array();while(n--)rings.push_back(sequence(little));out={{"type","Polygon"},{"coordinates",std::move(rings)}};}
            else if(type>=4 && type<=6) {
                auto n=uint(4,little);if(n>1000000 || n>(bytes.size()-pos)/5)invalid("invalid WKB part count");Json coordinates=Json::array();
                const std::string child=type==4?"Point":type==5?"LineString":"Polygon";
                while(n--) {auto part=value();if(part.at("type")!=child)invalid("incompatible WKB multipart member");coordinates.push_back(std::move(part["coordinates"]));}
                out={{"type",type==4?"MultiPoint":type==5?"MultiLineString":"MultiPolygon"},{"coordinates",std::move(coordinates)}};
            } else invalid("unsupported WKB geometry type");
            --depth;return out;
        }
    public:
        explicit Wkb(const std::string& input):bytes(input) {}
        Json decode() {auto out=value();if(pos!=bytes.size())invalid("trailing WKB bytes");return out;}
    };
    Json object(Id id,unsigned depth,bool root=false) {
        if(depth>32 || ++count>1000000)invalid("hierarchy exceeds object/depth limit");
        if(!objects.insert(id).second)invalid("cyclic or shared hierarchy object");
        const auto name=stream.name(id);
        const bool cell=name=="qupath.lib.objects.PathCellObject",detection=name=="qupath.lib.objects.PathDetectionObject";
        if(root?name!="qupath.lib.objects.PathRootObject":(!cell && !detection && name!="qupath.lib.objects.PathAnnotationObject"))invalid("unsupported hierarchy object: "+name);
        Data d(stream,extra(id));const auto label=d.object(),color=d.object(),meta=d.object();
        auto attributes=metadata(meta);const auto found=attributes.find("Object ID");if(found==attributes.end())invalid("missing saved object identity");
        const auto uuid=found->second;
        if(uuid.size()!=36)invalid("invalid saved object UUID");
        for(std::size_t i=0;i<uuid.size();++i) {const bool dash=i==8 || i==13 || i==18 || i==23;const auto c=uuid[i];if(dash?c!='-':!((c>='0' && c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))invalid("invalid saved object UUID");}
        attributes.erase(found);
        const auto measured=measurements(d.object());
        const auto n=d.integer(4);if(n>1000000)invalid("invalid hierarchy child count");
        Json children=Json::array();for(std::size_t i=0;i<n;++i)children.push_back(object(d.object(),depth+1));
        if(root) {if(!attributes.empty() || !measured.empty())issue("root_properties_dropped","Root metadata/measurements have no image-object counterpart");d.finish();return children;}
        Json properties={{"objectType",cell?"cell":detection?"detection":"annotation"}};
        if(!attributes.empty())properties["metadata"]=std::move(attributes);
        if(!measured.empty())properties["measurements"]=measured;
        if(label)properties["name"]=text(label);
        if(color) {if(stream.name(color)!="java.lang.Integer")invalid("invalid saved object color");properties["color"]=std::int32_t(number(field(color,"value"),'I'));}
        const auto locked=d.object();if(stream.name(locked)!="java.lang.Boolean")invalid("invalid ROI lock flag");
        const auto lock_value=number(field(locked,"value"),'Z');if(lock_value>1)invalid("invalid ROI lock value");if(lock_value)properties["isLocked"]=true;
        const auto cls=classification(d.object());if(!cls.is_null())properties["classification"]=cls;
        auto roi=geometry(d.object());const auto probability=floating(d.integer(8));
        if(!std::isnan(probability))properties["qupathxx"]={{"classProbability",measurement_json(probability)}};
        if(!children.empty())properties["childObjects"]=std::move(children);
        Json out={{"type","Feature"},{"id",uuid},{"geometry",std::move(roi)},{"properties",std::move(properties)}};
        if(cell) {const auto nucleus=d.object();if(nucleus)out["nucleusGeometry"]=geometry(nucleus);}
        d.finish();return out;
    }
    void properties(Id id) {
        if(stream.name(id)!="java.util.HashMap")invalid("unsupported image properties map");
        Data d(stream,extra(id));d.integer(4);const auto n=d.integer(4);if(n>1000000)invalid("too many image properties");
        for(std::size_t i=0;i<n;++i) {auto key=text(d.object());d.object();if(key!="PROJECT_ENTRY_ID")issue("image_settings_dropped","ImageData properties are not migrated");}d.finish();
    }
public:
    explicit Decoder(const Stream& s):stream(s) {}
    QuPathSavedData decode(const std::vector<Id>& values) {
        if(values.size()<3 || at(values[0]).kind!=Node::Block)invalid("missing data-file version");
        const auto& header=at(values[0]).bytes;const std::string version="Data file version 3";
        if(header.size()!=version.size()+2 || static_cast<unsigned char>(header[0])!=0 || static_cast<unsigned char>(header[1])!=version.size() || header.substr(2)!=version)invalid("unsupported data-file version");
        if(at(values[1]).kind!=Node::Text)invalid("missing server metadata");
        StrictJson strict("QuPath saved server");Json::sax_parse(at(values[1]).text,&strict);const auto server=Json::parse(at(values[1]).text);
        if(server.at("dataVersion")!=3 || server.at("qupathVersion")!="0.7.0")invalid("unsupported saved server version");
        const auto& metadata=server.at("server").at("metadata");result.server_metadata=metadata.dump();
        if(metadata.contains("pixelCalibration")) {
            const auto& calibration=metadata["pixelCalibration"];
            auto axis=[&](const char* key)->std::optional<double> {if(!calibration.contains(key))return {};const auto& quantity=calibration[key];if(quantity.at("unit")=="px")return {};if(quantity.at("unit")!=u8"\u00b5m") {issue("calibration_unit","Unsupported saved XY calibration unit");return {};}const auto& v=quantity.at("value");if(v.is_string() && v=="NaN")return {};if(v.is_null())return {};const auto x=v.get<double>();if(!std::isfinite(x) || x<=0)invalid("invalid saved XY calibration");return x;};
            result.pixel_width_um=axis("pixelWidth");result.pixel_height_um=axis("pixelHeight");
        }
        if(metadata.value("sizeZ",1)>1 || metadata.value("sizeT",1)>1)issue("image_settings_dropped","Source Z/time calibration is retained; saved ImageData calibration overrides are not applied");
        bool hierarchy=false,eof=false;
        for(std::size_t i=2;i<values.size();++i) {
            const auto id=values[i];if(!id)continue;const auto name=stream.name(id);
            if(at(id).kind==Node::Text && at(id).text=="EOF") {if(i+1!=values.size())invalid("trailing data after EOF");eof=true;break;}
            if(name=="java.util.Locale")continue;
            if(name=="qupath.lib.images.ImageData$ImageType") {if(at(id).text!="UNSET")issue("image_settings_dropped","Image type/analysis defaults are not migrated");}
            else if(name=="qupath.lib.plugins.workflow.Workflow") {Data d(stream,extra(id));if(d.integer(4)!=1)invalid("unsupported workflow version");const auto count=d.integer(4);if(count>1000000)invalid("too many workflow steps");for(std::size_t n=0;n<count;++n)d.object();d.finish();if(count)issue("workflow_dropped","Saved workflow history is not executable in the native project");}
            else if(name=="qupath.lib.objects.hierarchy.PathObjectHierarchy") {
                if(hierarchy)invalid("multiple hierarchies");hierarchy=true;
                if(field(id,"tmaGrid"))issue("tma_grid_dropped","Saved TMA grid structure is not migrated by this decoder");
                Json collection={{"type","FeatureCollection"},{"features",object(field(id,"rootObject"),0,true)}};
                result.geojson=collection.dump();if(result.geojson.size()>256ull*1024*1024)invalid("decoded geometry exceeds 256 MiB");
            } else if(name=="java.util.HashMap")properties(id);
            else if(name=="qupath.lib.color.ColorDeconvolutionStains")issue("image_settings_dropped","Image stain defaults are not migrated");
            else invalid("unsupported top-level saved record: "+name);
        }
        if(!hierarchy || !eof)invalid("missing hierarchy or terminal EOF record");return std::move(result);
    }
};
}
QuPathSavedData decode_qupath_saved_data(const std::string& bytes,const std::atomic<bool>* cancelled) {
    try {Stream stream(bytes,cancelled);std::vector<Id> values;while(!stream.done()) {if(values.size()>=1024)invalid("too many top-level records");values.push_back(stream.value());}return Decoder(stream).decode(values);}
    catch(const Json::exception& e) {invalid(std::string("invalid saved metadata: ")+e.what());}
}
}
#else
namespace qpx::detail {
QuPathSavedData decode_qupath_saved_data(const std::string&,const std::atomic<bool>*) {throw std::runtime_error("QuPath saved-data import requires project and interchange support");}
}
#endif

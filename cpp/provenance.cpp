#include "qupathxx/provenance.hpp"
#include "atomic_export.hpp"
#include "utf8_text.hpp"
#include "build_manifest.hpp"
#include <charconv>
#include <chrono>
#include <cmath>
#include <limits>
#ifdef QUPATHXX_HAS_GEOS
#include <geos_c.h>
#endif
#ifdef QUPATHXX_HAS_PROJECT
#include <sqlite3.h>
#endif
#ifdef QUPATHXX_HAS_OPENSLIDE
#include <openslide.h>
#endif
namespace qpx {
namespace {
void text(const std::string& s,std::size_t max=32768) {detail::bounded_utf8(s,max);}
bool hex(const std::string& s) {return s.size()==64 && s.find_first_not_of("0123456789abcdef")==std::string::npos;}
void calibration(std::optional<double> v) {if(v && (!std::isfinite(*v) || *v<=0)) throw std::invalid_argument("invalid provenance calibration");}
struct Json {
    std::string out;
    void string(const std::string& s) {out+='"';constexpr char hex[]="0123456789abcdef";for(unsigned char c:s) {if(c=='"' || c=='\\') {out+='\\';out+=char(c);}else if(c<32) {out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);}out+='"';}
    void integer(std::uint64_t v) {char b[32];auto r=std::to_chars(b,b+32,v);if(r.ec!=std::errc{}) throw std::runtime_error("provenance integer format failed");out.append(b,r.ptr);}
    void real(double v) {char b[128];auto r=std::to_chars(b,b+128,v,std::chars_format::general,std::numeric_limits<double>::max_digits10);if(r.ec!=std::errc{}) throw std::runtime_error("provenance real format failed");out.append(b,r.ptr);}
    void optional(std::optional<double> v) {if(v) real(*v);else out+="null";}
    void field(const char* key,const std::string& value) {string(key);out+=':';string(value);out+=',';}
};
}
std::size_t BuildEnvironment::payload_bytes() const {std::size_t n=sizeof(*this);for(const auto* s:{&app_version,&source_tree_sha256,&git_revision,&compiler,&compiler_flags,&configuration,&platform,&cmake_version,&geos_version,&openslide_version,&sqlite_version}) n+=s->capacity();return n;}
void BuildEnvironment::validate() const {
    for(const auto* s:{&app_version,&source_tree_sha256,&git_revision,&compiler,&compiler_flags,&configuration,&platform,&cmake_version,&geos_version,&openslide_version,&sqlite_version}) text(*s);
    if(app_version.empty() || compiler.empty() || platform.empty() || configuration.empty() || !hex(source_tree_sha256)) throw std::invalid_argument("invalid provenance environment");
}
std::shared_ptr<const BuildEnvironment> build_environment() {
    static const auto value=[] {
        auto e=std::make_shared<BuildEnvironment>();e->app_version=build_info::app_version;e->source_tree_sha256=build_info::source_sha256;e->git_revision=build_info::git_revision;
        e->compiler=build_info::compiler;e->compiler_flags=build_info::flags;e->configuration=QPX_BUILD_CONFIGURATION;if(e->configuration.empty()) e->configuration="unspecified";e->platform=build_info::platform;e->cmake_version=build_info::cmake_version;
        e->gui=QPX_ENV_GUI;e->python=QPX_ENV_PYTHON;e->interchange=QPX_ENV_INTERCHANGE;
#ifdef QUPATHXX_HAS_GEOS
        e->geos_version=GEOSversion();
#endif
#ifdef QUPATHXX_HAS_PROJECT
        e->sqlite_version=sqlite3_libversion();
#endif
#ifdef QUPATHXX_HAS_OPENSLIDE
        e->openslide_version=openslide_get_version();
#endif
        e->validate();return std::shared_ptr<const BuildEnvironment>(e);
    }();return value;
}
std::size_t AnalysisProvenance::payload_bytes() const {return sizeof(*this)+source.vendor.capacity()+source.levels.capacity()*sizeof(ImageLevel)+reader.capacity()+locator.capacity()+(environment?environment->payload_bytes():0);}
void AnalysisProvenance::validate() const {
    text(reader);text(locator);text(source.vendor,4096);
    if(captured_unix_ms>std::uint64_t(std::numeric_limits<std::int64_t>::max()) || !object_id || !object_revision || object_revision>document_revision || !environment || reader.empty() || source.width<=0 || source.height<=0 || source.levels.empty() || source.levels.size()>65536 || source.pixel_type!=PixelType::UInt8 || source.layout!=PixelLayout::RGBA)
        throw std::invalid_argument("invalid analysis provenance");
    if(source.levels[0].width!=source.width || source.levels[0].height!=source.height || source.levels[0].downsample!=1) throw std::invalid_argument("invalid provenance pyramid");
    for(const auto& l:source.levels) if(l.width<=0 || l.height<=0 || !std::isfinite(l.downsample) || l.downsample<1) throw std::invalid_argument("invalid provenance level");
    for(auto v:{source.pixel_width_um,source.pixel_height_um,effective_pixel_width_um,effective_pixel_height_um}) calibration(v);
    environment->validate();if(payload_bytes()>4u*1024u*1024u) throw std::length_error("analysis provenance exceeds 4 MiB");
}
std::shared_ptr<const AnalysisProvenance> capture_provenance(const ImageServer& server,std::uint64_t id,std::uint64_t revision,std::uint64_t document,bool nucleus,std::optional<double> x,std::optional<double> y) {
    auto p=std::make_shared<AnalysisProvenance>();p->object_id=id;p->object_revision=revision;p->document_revision=document;p->nucleus=nucleus;
    const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();if(now<0) throw std::runtime_error("invalid provenance capture clock");p->captured_unix_ms=static_cast<std::uint64_t>(now);
    p->source=server.metadata();p->reader=server.reader_id();p->locator=server.source_locator();p->effective_pixel_width_um=x;p->effective_pixel_height_um=y;p->environment=build_environment();p->validate();return p;
}
std::string provenance_json(const AnalysisProvenance& p) {
    p.validate();const auto& e=*p.environment;Json j;j.out="{";j.field("schema","qpx.analysis-provenance/1");
    j.out+="\"object_id\":";j.integer(p.object_id);j.out+=",\"object_revision\":";j.integer(p.object_revision);j.out+=",\"document_revision\":";j.integer(p.document_revision);j.out+=",\"captured_unix_ms\":";j.integer(p.captured_unix_ms);j.out+=",\"nucleus\":";j.out+=p.nucleus?"true":"false";
    j.out+=",\"source\":{";j.field("reader",p.reader);j.field("locator",p.locator);j.field("vendor",p.source.vendor);j.field("pixel_contract","qpx.device-rgba8/1");j.out+="\"width\":";j.integer(p.source.width);j.out+=",\"height\":";j.integer(p.source.height);
    j.out+=",\"pixel_width_um\":";j.optional(p.source.pixel_width_um);j.out+=",\"pixel_height_um\":";j.optional(p.source.pixel_height_um);j.out+=",\"effective_pixel_width_um\":";j.optional(p.effective_pixel_width_um);j.out+=",\"effective_pixel_height_um\":";j.optional(p.effective_pixel_height_um);j.out+=",\"levels\":[";
    bool first=true;for(const auto& l:p.source.levels) {if(!first) j.out+=',';first=false;j.out+="{\"width\":";j.integer(l.width);j.out+=",\"height\":";j.integer(l.height);j.out+=",\"downsample\":";j.real(l.downsample);j.out+='}';}
    j.out+=" ]},\"environment\":{";j.field("app_version",e.app_version);j.field("source_tree_sha256",e.source_tree_sha256);j.field("git_revision",e.git_revision);j.field("compiler",e.compiler);j.field("compiler_flags",e.compiler_flags);j.field("configuration",e.configuration);j.field("platform",e.platform);j.field("cmake_version",e.cmake_version);j.field("geos_version",e.geos_version);j.field("openslide_version",e.openslide_version);j.field("sqlite_version",e.sqlite_version);
    j.out+="\"gui\":";j.out+=e.gui?"true":"false";j.out+=",\"python\":";j.out+=e.python?"true":"false";j.out+=",\"interchange\":";j.out+=e.interchange?"true":"false";j.out+="}}\n";
    if(j.out.size()>16u*1024u*1024u) throw std::length_error("provenance JSON exceeds 16 MiB");return j.out;
}
void save_provenance(const AnalysisProvenance& p,const std::string& path) {const auto data=provenance_json(p);detail::AtomicExportFile file(path,16u*1024u*1024u);file.write(data.data(),data.size());file.commit();}
}

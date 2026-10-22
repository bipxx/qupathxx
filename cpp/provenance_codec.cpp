#include "provenance_codec.hpp"
#include "sha256.hpp"
#include "qupathxx/source_identity.hpp"
#include <cstring>
#include <limits>
#include <stdexcept>
namespace qpx {
namespace {
struct Writer {
    std::vector<std::uint8_t> data={'Q','P','V',2};
    void integer(std::uint64_t v) {for(int i=0;i<8;++i) data.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
    void real(double v) {std::uint64_t bits;std::memcpy(&bits,&v,8);integer(bits);}
    void string(const std::string& s) {integer(s.size());data.insert(data.end(),s.begin(),s.end());}
    void optional(std::optional<double> v) {integer(bool(v));if(v) real(*v);}
};
struct Reader {
    const std::uint8_t* p;std::size_t left;
    std::uint64_t integer(std::uint64_t max=std::numeric_limits<std::uint64_t>::max()) {if(left<8) throw std::invalid_argument("truncated provenance");std::uint64_t v=0;for(int i=0;i<8;++i) v|=std::uint64_t(p[i])<<(8*i);p+=8;left-=8;if(v>max) throw std::invalid_argument("invalid provenance integer");return v;}
    double real() {const auto bits=integer();double v;std::memcpy(&v,&bits,8);return v;}
    std::string string() {const auto n=integer(32768);if(n>left) throw std::invalid_argument("truncated provenance text");std::string s(reinterpret_cast<const char*>(p),static_cast<std::size_t>(n));p+=n;left-=n;return s;}
    std::optional<double> optional() {if(!integer(1)) return {};return real();}
};
}
std::vector<std::uint8_t> ProvenanceCodec::encode(const AnalysisProvenance& p) {
    p.validate();Writer w;for(auto v:{p.object_id,p.object_revision,p.document_revision,p.captured_unix_ms,std::uint64_t(p.nucleus),std::uint64_t(p.source.width),std::uint64_t(p.source.height)}) w.integer(v);
    w.string(p.reader);w.string(p.locator);w.string(p.source.vendor);for(auto v:{p.source.pixel_width_um,p.source.pixel_height_um,p.effective_pixel_width_um,p.effective_pixel_height_um}) w.optional(v);
    w.integer(p.source.levels.size());for(const auto& l:p.source.levels) {w.integer(l.width);w.integer(l.height);w.real(l.downsample);}
    const auto& e=*p.environment;for(const auto* s:{&e.app_version,&e.source_tree_sha256,&e.git_revision,&e.compiler,&e.compiler_flags,&e.configuration,&e.platform,&e.cmake_version,&e.geos_version,&e.openslide_version,&e.sqlite_version}) w.string(*s);
    w.integer(e.gui);w.integer(e.python);w.integer(e.interchange);
    w.integer(bool(p.reader_source));if(p.reader_source) {
        const auto& source=*p.reader_source;for(const auto* text:{&source.generation,&source.reader,&source.locator,&source.scope}) w.string(*text);
        w.integer(bool(source.primary));if(source.primary) {const auto& v=*source.primary;for(const auto* text:{&v.path,&v.identity_scheme,&v.identity,&v.stamp_scheme,&v.modified_token,&v.change_token}) w.string(*text);w.integer(v.bytes);w.integer(v.observed_unix_ms);}
    }
    Sha256 digest;digest.update(w.data.data(),w.data.size());const auto hex=digest.finish();w.data.insert(w.data.end(),hex.begin(),hex.end());
    if(w.data.size()>4u*1024u*1024u) throw std::length_error("encoded provenance exceeds 4 MiB");return w.data;
}
std::shared_ptr<const AnalysisProvenance> ProvenanceCodec::decode(const std::uint8_t* data,std::size_t size) {
    if(!data || size<68 || size>4u*1024u*1024u || (std::memcmp(data,"QPV",3) || (data[3]!=1 && data[3]!=2))) throw std::invalid_argument("invalid provenance encoding");
    Sha256 digest;digest.update(data,size-64);if(digest.finish()!=std::string(reinterpret_cast<const char*>(data+size-64),64)) throw std::invalid_argument("provenance checksum mismatch");
    Reader r{data+4,size-68};auto p=std::make_shared<AnalysisProvenance>();p->object_id=r.integer();p->object_revision=r.integer();p->document_revision=r.integer();p->captured_unix_ms=r.integer(std::numeric_limits<std::int64_t>::max());p->nucleus=r.integer(1)!=0;
    p->source.width=static_cast<std::int64_t>(r.integer(std::numeric_limits<std::int64_t>::max()));p->source.height=static_cast<std::int64_t>(r.integer(std::numeric_limits<std::int64_t>::max()));
    p->reader=r.string();p->locator=r.string();p->source.vendor=r.string();p->source.pixel_width_um=r.optional();p->source.pixel_height_um=r.optional();p->effective_pixel_width_um=r.optional();p->effective_pixel_height_um=r.optional();
    const auto n=r.integer(65536);if(n>r.left/24) throw std::invalid_argument("truncated provenance levels");p->source.levels.reserve(static_cast<std::size_t>(n));
    for(std::uint64_t i=0;i<n;++i) {const auto w=r.integer(std::numeric_limits<std::int64_t>::max()),h=r.integer(std::numeric_limits<std::int64_t>::max());p->source.levels.push_back({static_cast<std::int64_t>(w),static_cast<std::int64_t>(h),r.real()});}
    auto e=std::make_shared<BuildEnvironment>();for(auto* s:{&e->app_version,&e->source_tree_sha256,&e->git_revision,&e->compiler,&e->compiler_flags,&e->configuration,&e->platform,&e->cmake_version,&e->geos_version,&e->openslide_version,&e->sqlite_version}) *s=r.string();
    e->gui=r.integer(1)!=0;e->python=r.integer(1)!=0;e->interchange=r.integer(1)!=0;
    if(data[3]==2 && r.integer(1)) {
        auto source=std::make_shared<ReaderSource>();for(auto* text:{&source->generation,&source->reader,&source->locator,&source->scope}) *text=r.string();
        if(r.integer(1)) {auto v=std::make_shared<FileVersion>();for(auto* text:{&v->path,&v->identity_scheme,&v->identity,&v->stamp_scheme,&v->modified_token,&v->change_token}) *text=r.string();v->bytes=r.integer(std::numeric_limits<std::int64_t>::max());v->observed_unix_ms=r.integer(std::numeric_limits<std::int64_t>::max());source->primary=std::move(v);}
        source->validate();p->reader_source=std::move(source);
    }
    if(r.left) throw std::invalid_argument("trailing provenance bytes");p->environment=std::move(e);p->validate();return p;
}
void ProvenanceCodec::attach(const std::shared_ptr<const TissueResult>& result,std::shared_ptr<const AnalysisProvenance> p) {
    if(p && (p->object_id!=result->object_id() || p->object_revision!=result->object_revision() || p->document_revision!=result->document_revision() || p->nucleus!=result->nucleus() || p->source.width!=result->image_bounds().width || p->source.height!=result->image_bounds().height || result->level()<0 || std::size_t(result->level())>=p->source.levels.size() || p->source.levels[result->level()].downsample!=result->downsample())) throw std::invalid_argument("tissue/cell provenance identity mismatch");
    std::const_pointer_cast<TissueResult>(result)->provenance_=std::move(p);
}
}

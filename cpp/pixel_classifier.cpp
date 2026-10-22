#include "qupathxx/pixel_classifier.hpp"
#include "sha256.hpp"
#include "atomic_export.hpp"
#include "reader_file_guard.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
namespace qpx {
namespace {
constexpr std::size_t limit=64u*1024*1024+4096;
void stopped(const std::atomic<bool>* c) {if(c && c->load()) throw std::runtime_error("pixel classification cancelled");}
std::string hash(const std::uint8_t* p,std::size_t n) {Sha256 h;h.update(p,n);return h.finish();}
struct Writer {
    std::vector<std::uint8_t> b={'Q','P','C',1};
    void integer(std::uint64_t v) {for(int i=0;i<8;++i) b.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
    void real(double v) {std::uint64_t bits;std::memcpy(&bits,&v,8);integer(bits);}
};
struct Reader {
    const std::uint8_t* p;std::size_t left;
    std::uint64_t integer(std::uint64_t max=std::numeric_limits<std::uint64_t>::max()) {if(left<8) throw std::invalid_argument("truncated pixel classifier recipe");std::uint64_t n=0;for(int i=0;i<8;++i) n|=std::uint64_t(p[i])<<(8*i);p+=8;left-=8;if(n>max) throw std::invalid_argument("invalid pixel classifier recipe integer");return n;}
    double real() {const auto bits=integer();double v;std::memcpy(&v,&bits,8);if(!std::isfinite(v)) throw std::invalid_argument("nonfinite pixel classifier recipe parameter");return v;}
};
}
struct PixelClassifier::Data {
    ForestModel forest;
    PixelFeatureOptions options;
    StainTransform transform;
    std::vector<std::uint8_t> bytes;
    std::string sha;
    Data(const ForestModel& f,PixelFeatureOptions o,const StainTransform& t):forest(f),options(std::move(o)),transform(t) {}
};
PixelClassifier::PixelClassifier(const ForestModel& forest,PixelFeatureOptions options,const StainTransform& transform) {
    const auto schema=pixel_feature_schema(options,transform);const auto& actual=forest.features();if(schema.size()!=actual.size()) throw std::invalid_argument("pixel classifier feature schema mismatch");
    for(std::size_t i=0;i<schema.size();++i) if(schema[i].name!=actual[i].name || schema[i].unit!=actual[i].unit) throw std::invalid_argument("pixel classifier feature schema mismatch");
    auto data=std::make_shared<Data>(forest,std::move(options),transform);const auto& o=data->options;Writer w;
    w.integer(std::uint64_t(o.rgb)|(std::uint64_t(o.optical_density)<<1)|(std::uint64_t(o.stains)<<2)|(std::uint64_t(o.gradient_magnitude)<<3)|(std::uint64_t(o.laplacian)<<4));
    w.integer(o.sigmas.size());for(auto sigma:o.sigmas) w.real(sigma);w.integer(o.work_bytes);
    for(auto v:transform.white()) w.real(v);for(const auto& row:transform.stains()) for(auto v:row) w.real(v);
    const auto bytes=forest.bytes();w.integer(bytes.size());w.b.insert(w.b.end(),bytes.begin(),bytes.end());const auto digest=hash(w.b.data(),w.b.size());w.b.insert(w.b.end(),digest.begin(),digest.end());
    if(w.b.size()>limit) throw std::length_error("pixel classifier recipe exceeds 64 MiB plus 4 KiB");w.b.shrink_to_fit();data->bytes=std::move(w.b);data->sha=hash(data->bytes.data(),data->bytes.size());data_=std::move(data);
}
const ForestModel& PixelClassifier::forest() const {return data_->forest;}
PixelFeatureOptions PixelClassifier::options() const {return data_->options;}
const StainTransform& PixelClassifier::transform() const {return data_->transform;}
const std::vector<ClassifierFeature>& PixelClassifier::features() const {return forest().features();}
const std::vector<std::string>& PixelClassifier::classes() const {return forest().classes();}
const std::vector<std::uint8_t>& PixelClassifier::bytes() const {return data_->bytes;}
const std::string& PixelClassifier::sha256() const {return data_->sha;}
PixelClassifier PixelClassifier::from_bytes(const std::vector<std::uint8_t>& bytes) {
    if(bytes.size()<68 || bytes.size()>limit || std::memcmp(bytes.data(),"QPC\1",4)) throw std::invalid_argument("unsupported pixel classifier recipe encoding");
    if(hash(bytes.data(),bytes.size()-64)!=std::string(reinterpret_cast<const char*>(bytes.data()+bytes.size()-64),64)) throw std::invalid_argument("pixel classifier recipe checksum mismatch");
    Reader r{bytes.data()+4,bytes.size()-68};PixelFeatureOptions o;const auto flags=r.integer(31);o.rgb=flags&1;o.optical_density=flags&2;o.stains=flags&4;o.gradient_magnitude=flags&8;o.laplacian=flags&16;
    const auto n=r.integer(8);if(!n) throw std::invalid_argument("pixel classifier recipe has no scales");o.sigmas.clear();for(std::size_t i=0;i<n;++i) o.sigmas.push_back(r.real());o.work_bytes=static_cast<std::size_t>(r.integer(1024ULL*1024*1024));o.validate();
    ColorVector white;std::array<ColorVector,3> stains;for(auto& v:white) v=r.real();for(auto& row:stains) for(auto& v:row) v=r.real();const auto transform=StainTransform::from_normalized(stains,white);
    const auto size=r.integer(64u*1024*1024);if(size!=r.left) throw std::invalid_argument("invalid pixel classifier forest payload length");
    const auto forest=ForestModel::from_bytes(std::vector<std::uint8_t>(r.p,r.p+size));PixelClassifier result(forest,std::move(o),transform);
    if(result.bytes()!=bytes) throw std::invalid_argument("noncanonical pixel classifier recipe");return result;
}
std::shared_ptr<const PixelClassification> classify_pixels(const PixelBuffer& input,const PixelClassifier& classifier,double minimum,std::optional<PixelFeatureWindow> window,std::size_t budget,const std::atomic<bool>* cancelled) {
    const auto started=std::chrono::steady_clock::now();stopped(cancelled);if(!std::isfinite(minimum) || minimum<0 || minimum>1) throw std::invalid_argument("pixel classifier minimum tree vote must be in 0..1");
    if(minimum==0) minimum=0; // Canonical positive zero in result identity.
    auto options=classifier.options();if(budget) options.work_bytes=budget;options.validate();
    const auto feature_work=pixel_feature_work_bytes(input.width(),input.height(),options,window);
    const auto width=window?window->width:input.width(),height=window?window->height:input.height();const auto count=static_cast<std::size_t>(width)*static_cast<std::size_t>(height),columns=classifier.features().size(),classes=classifier.classes().size(),batch=std::min<std::size_t>(512,count);
    const auto prediction_work=262144+count*(columns*sizeof(float)+10)+batch*(sizeof(ClassifierPrediction)+sizeof(std::size_t)+columns*sizeof(float)+classes*sizeof(std::uint32_t));
    if(prediction_work>options.work_bytes) throw std::length_error("pixel classification output/vote scratch exceeds memory budget");
    const auto features=extract_pixel_features(input,classifier.transform(),options,window,cancelled);auto out=std::shared_ptr<PixelClassification>(new PixelClassification());
    out->window_=features->window();out->model_sha256_=classifier.sha256();out->feature_sha256_=features->sha256();out->input_sha256_=features->input_sha256();out->minimum_=minimum;out->work_=std::max(feature_work,prediction_work);out->classes_=classifier.classes();out->counts_.resize(classes);out->valid_=features->valid();out->valid_pixels_=features->valid_pixels();out->labels_.resize(count);out->fractions_.assign(count,std::numeric_limits<float>::quiet_NaN());
    std::vector<std::size_t> indices;std::vector<float> matrix;indices.reserve(batch);matrix.reserve(batch*columns);
    auto flush=[&] {if(indices.empty()) return;stopped(cancelled);const auto predictions=classifier.forest().predict(matrix,classifier.features());for(std::size_t i=0;i<indices.size();++i) {const auto row=indices[i];const auto& p=predictions[i];out->fractions_[row]=static_cast<float>(p.vote_fraction);if(p.vote_fraction>=minimum) {out->labels_[row]=p.class_index+1;++out->counts_[p.class_index];++out->classified_pixels_;}}indices.clear();matrix.clear();};
    for(std::size_t row=0;row<count;++row) {if((row&511)==0) stopped(cancelled);if(!out->valid_[row]) continue;indices.push_back(row);matrix.insert(matrix.end(),features->values().begin()+row*columns,features->values().begin()+(row+1)*columns);if(indices.size()==batch) flush();}flush();
    Sha256 h;const std::string tag="qpx.pixel-classification/1";h.update(tag.data(),tag.size());h.update(out->model_sha256_.data(),out->model_sha256_.size());h.update(out->feature_sha256_.data(),out->feature_sha256_.size());std::uint64_t bits;std::memcpy(&bits,&minimum,8);h.integer(bits);h.update(out->valid_.data(),out->valid_.size());
    for(std::size_t i=0;i<count;++i) {if((i&4095)==0) stopped(cancelled);h.integer(out->labels_[i]);std::uint32_t value=0x7fc00000;if(std::isfinite(out->fractions_[i])) std::memcpy(&value,&out->fractions_[i],4);h.integer(value);}out->sha256_=h.finish();out->elapsed_ms_=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();stopped(cancelled);return out;
}
PixelClassifier load_pixel_classifier(const std::string& input,const std::atomic<bool>* cancelled) {
    stopped(cancelled);detail::bounded_utf8(input,32768);const auto path=detail::export_path(input);detail::ReaderFileGuard guard(path);const auto size=guard.version()->bytes;
    if(size>limit) throw std::length_error("pixel classifier recipe file exceeds 64 MiB plus 4 KiB");std::ifstream file(std::filesystem::u8path(path),std::ios::binary);if(!file) throw std::runtime_error("cannot open pixel classifier recipe");std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    for(std::size_t at=0;at<bytes.size();) {stopped(cancelled);const auto n=std::min<std::size_t>(65536,bytes.size()-at);file.read(reinterpret_cast<char*>(bytes.data()+at),static_cast<std::streamsize>(n));if(file.gcount()!=static_cast<std::streamsize>(n)) throw std::runtime_error("pixel classifier recipe changed or read failed");at+=n;}
    if(file.peek()!=std::char_traits<char>::eof() || file.bad()) throw std::runtime_error("pixel classifier recipe changed or read failed");guard.verify();auto model=PixelClassifier::from_bytes(bytes);guard.verify();stopped(cancelled);return model;
}
void save_pixel_classifier(const PixelClassifier& model,const std::string& input,bool replace,const std::atomic<bool>* cancelled) {
    stopped(cancelled);detail::bounded_utf8(input,32768);const auto path=detail::export_path(input);std::unique_ptr<detail::ReaderFileGuard> guard;
    if(replace && std::filesystem::exists(std::filesystem::u8path(path))) {guard=std::make_unique<detail::ReaderFileGuard>(path);(void)load_pixel_classifier(path,cancelled);guard->verify();}
    detail::AtomicExportFile file(path,limit,cancelled);file.write(reinterpret_cast<const char*>(model.bytes().data()),model.bytes().size());const bool replacing=bool(guard);if(guard) {guard->verify();guard.reset();}file.commit(replacing);
}
}

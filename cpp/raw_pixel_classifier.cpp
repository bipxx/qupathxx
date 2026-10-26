#include "qupathxx/raw_pixel_classifier.hpp"
#include "raw_pixel_classifier_internal.hpp"
#include "raw_pixel_features.hpp"
#include "atomic_export.hpp"
#include "reader_file_guard.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>

namespace qpx {
namespace {
constexpr std::size_t model_limit=64u*1024*1024+128u*1024;
void stopped(const std::atomic<bool>* c) {if(c && c->load(std::memory_order_relaxed)) throw std::runtime_error("raw pixel classification cancelled");}
void text(Sha256& hash,const std::string& value) {hash.integer(value.size());hash.update(value.data(),value.size());}
std::string digest(const std::uint8_t* bytes,std::size_t size) {Sha256 h;h.update(bytes,size);return h.finish();}
bool equal(GridFraction a,GridFraction b) {return a.numerator()==b.numerator() && a.denominator()==b.denominator();}
std::vector<std::string> selected_channel_names(const RawImageMetadata& m,const ScientificRawOptions& s) {
    if(m.channel_names.empty()) return std::vector<std::string>(s.channels);
    if(m.channel_names.size()!=m.channels) throw std::invalid_argument("raw classifier channel labels do not match metadata");
    std::vector<std::string> out;out.reserve(s.channels);std::size_t bytes=0;
    for(std::uint32_t c=0;c<s.channels;++c) {const auto& name=m.channel_names[s.first_channel+c];detail::bounded_utf8(name,65536);bytes+=name.size();if(bytes>65536) throw std::length_error("raw classifier channel labels exceed 64 KiB");out.push_back(name);}return out;
}
void metadata_selection(const RawImageMetadata& m,const ScientificGrid& grid,const ScientificRawOptions& s) {
    s.validate();sample_bytes(m.sample_type);
    if(m.width!=grid.image_width() || m.height!=grid.image_height() || !m.series_count || m.series>=m.series_count || !m.channels || s.first_channel>=m.channels || s.channels>m.channels-s.first_channel || s.z>=m.z_planes || s.t>=m.timepoints) throw std::invalid_argument("raw classifier grid or channel/axis selection mismatch");
}
double spacing(std::optional<double> calibration,GridFraction step) {
    if(!calibration || !std::isfinite(*calibration) || *calibration<=0) throw std::invalid_argument("raw physical classifier requires known positive calibration on both axes");
    const auto value=*calibration*step.binary64();if(!std::isfinite(value) || value<=0) throw std::invalid_argument("raw classifier sampled physical spacing is invalid");return value;
}
struct Writer {
    std::vector<std::uint8_t> bytes{'Q','R','M',1};
    void integer(std::uint64_t value) {for(int i=0;i<8;++i) bytes.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
    void real(double value) {std::uint64_t bits;std::memcpy(&bits,&value,8);integer(bits);}
    void string(const std::string& value) {integer(value.size());bytes.insert(bytes.end(),value.begin(),value.end());}
};
struct Reader {
    const std::uint8_t* data;std::size_t left;
    std::uint64_t integer(std::uint64_t maximum=std::numeric_limits<std::uint64_t>::max()) {if(left<8) throw std::invalid_argument("truncated raw classifier model");std::uint64_t value=0;for(int i=0;i<8;++i) value|=std::uint64_t(data[i])<<(8*i);data+=8;left-=8;if(value>maximum) throw std::invalid_argument("raw classifier integer exceeds limit");return value;}
    double real() {const auto bits=integer();double value;std::memcpy(&value,&bits,8);if(!std::isfinite(value)) throw std::invalid_argument("raw classifier model has nonfinite parameter");return value;}
    std::string string() {const auto size=integer(65536);if(size>left) throw std::invalid_argument("truncated raw classifier text");std::string value(reinterpret_cast<const char*>(data),std::size_t(size));data+=size;left-=size;detail::bounded_utf8(value,65536);return value;}
};
}
struct RawPixelRecipe::Data {
    RawPixelFeatureOptions requested,resolved;
    ScientificRawOptions selection;
    SampleType type=SampleType::UInt8;
    std::uint32_t series=0;
    std::vector<std::string> names;
    GridFraction x{1,1},y{1,1};
    bool physical=false;
    std::optional<double> width_um,height_um;
    void resolve() {
        selection.validate();sample_bytes(type);
        if(selection.channels>128 || requested.offsets.size()!=selection.channels || requested.scales.size()!=selection.channels || requested.sigmas.empty() || requested.sigmas.size()>8) throw std::invalid_argument("invalid raw classifier feature dimensions");
        if(x.numerator()<=0 || y.numerator()<=0 || x.numerator()>ScientificGridAxis::coordinate_limit || y.numerator()>ScientificGridAxis::coordinate_limit || x.denominator()>1000000 || y.denominator()>1000000 || names.size()!=selection.channels || physical!=bool(width_um) || physical!=bool(height_um)) throw std::invalid_argument("invalid raw classifier sampling recipe");
        std::size_t bytes=0;for(const auto& name:names) {detail::bounded_utf8(name,65536);bytes+=name.size();if(bytes>65536) throw std::length_error("raw classifier channel labels exceed 64 KiB");}
        resolved=requested;
        if(physical) {
            if(!std::isfinite(*width_um) || !std::isfinite(*height_um) || *width_um<=0 || *height_um<=0) throw std::invalid_argument("invalid raw classifier physical spacing");
            for(auto& sigma:resolved.sigmas) {sigma.x/=*width_um;sigma.y/=*height_um;}
        }
        resolved.validate(selection.channels);
    }
};
RawPixelRecipe::RawPixelRecipe(const RawImageMetadata& m,const ScientificGrid& grid,ScientificRawOptions selection,RawPixelFeatureOptions options,bool physical) {
    metadata_selection(m,grid,selection);if(selection.channels>128) throw std::invalid_argument("raw classifier exceeds 128 channels");auto d=std::make_shared<Data>();d->selection=selection;d->requested=std::move(options);d->type=m.sample_type;d->series=m.series;d->names=selected_channel_names(m,selection);d->x={grid.x().extent(),grid.x().samples()};d->y={grid.y().extent(),grid.y().samples()};d->physical=physical;
    if(physical) {d->width_um=spacing(m.pixel_width_um,d->x);d->height_um=spacing(m.pixel_height_um,d->y);}d->resolve();data_=std::move(d);
}
RawPixelRecipe::RawPixelRecipe(std::shared_ptr<const Data> data):data_(std::move(data)) {}
const RawPixelFeatureOptions& RawPixelRecipe::requested_features() const {return data_->requested;}
const RawPixelFeatureOptions& RawPixelRecipe::features() const {return data_->resolved;}
const ScientificRawOptions& RawPixelRecipe::selection() const {return data_->selection;}
SampleType RawPixelRecipe::sample_type() const {return data_->type;}
std::uint32_t RawPixelRecipe::series() const {return data_->series;}
const std::vector<std::string>& RawPixelRecipe::channel_names() const {return data_->names;}
GridFraction RawPixelRecipe::step_x() const {return data_->x;}
GridFraction RawPixelRecipe::step_y() const {return data_->y;}
bool RawPixelRecipe::physical_sigmas() const {return data_->physical;}
std::optional<double> RawPixelRecipe::sampled_pixel_width_um() const {return data_->width_um;}
std::optional<double> RawPixelRecipe::sampled_pixel_height_um() const {return data_->height_um;}
std::vector<ClassifierFeature> RawPixelRecipe::feature_schema() const {RawImageMetadata metadata;metadata.sample_type=data_->type;return detail::raw_feature_schema(metadata,data_->selection,data_->resolved);}
bool RawPixelRecipe::matches(const RawImageMetadata& m,const ScientificGrid& grid,const ScientificRawOptions& s) const {
    metadata_selection(m,grid,s);const auto& d=*data_;
    if(m.sample_type!=d.type || m.series!=d.series || s.first_channel!=d.selection.first_channel || s.channels!=d.selection.channels || s.z!=d.selection.z || s.t!=d.selection.t || selected_channel_names(m,s)!=d.names) return false;
    const GridFraction x{grid.x().extent(),grid.x().samples()},y{grid.y().extent(),grid.y().samples()};
    if(d.physical) return spacing(m.pixel_width_um,x)==*d.width_um && spacing(m.pixel_height_um,y)==*d.height_um;
    return equal(x,d.x) && equal(y,d.y);
}
struct RawPixelModelCodec {
    static void write(Writer& w,const RawPixelRecipe& recipe) {
        const auto& d=*recipe.data_;const auto& o=d.requested;const auto& s=d.selection;
        w.integer(std::uint64_t(d.type));w.integer(d.series);w.integer(s.first_channel);w.integer(s.channels);w.integer(s.z);w.integer(s.t);w.integer(s.work_bytes);
        for(const auto axis:{d.x,d.y}) {w.integer(std::uint64_t(axis.numerator()));w.integer(std::uint64_t(axis.denominator()));}
        w.integer(d.physical);if(d.physical) {w.real(*d.width_um);w.real(*d.height_um);}
        for(const auto& name:d.names) w.string(name);
        for(std::size_t c=0;c<s.channels;++c) {w.real(o.offsets[c]);w.real(o.scales[c]);}
        w.integer(o.sigmas.size());for(const auto sigma:o.sigmas) {w.real(sigma.x);w.real(sigma.y);}
        w.integer(std::uint64_t(o.gradient_magnitude)|(std::uint64_t(o.laplacian)<<1));w.integer(o.work_bytes);
    }
    static RawPixelRecipe read(Reader& r) {
        auto d=std::make_shared<RawPixelRecipe::Data>();d->type=SampleType(r.integer(9));d->series=std::uint32_t(r.integer(std::numeric_limits<std::uint32_t>::max()));
        auto& s=d->selection;s.first_channel=std::uint32_t(r.integer(65535));s.channels=std::uint32_t(r.integer(128));s.z=std::uint32_t(r.integer(65535));s.t=std::uint32_t(r.integer(65535));s.work_bytes=std::size_t(r.integer(1024ULL*1024*1024));s.validate();
        // Read in sequenced statements: argument evaluation order must never
        // swap a numerator/denominator or an X/Y axis on another compiler.
        const auto xn=r.integer(ScientificGridAxis::coordinate_limit),xd=r.integer(1000000),yn=r.integer(ScientificGridAxis::coordinate_limit),yd=r.integer(1000000);d->x={std::int64_t(xn),std::int64_t(xd)};d->y={std::int64_t(yn),std::int64_t(yd)};
        d->physical=r.integer(1)!=0;if(d->physical) {d->width_um=r.real();d->height_um=r.real();}
        std::size_t name_bytes=0;for(std::uint32_t c=0;c<s.channels;++c) {auto name=r.string();name_bytes+=name.size();if(name_bytes>65536) throw std::length_error("raw classifier channel labels exceed 64 KiB");d->names.push_back(std::move(name));}
        auto& o=d->requested;for(std::uint32_t c=0;c<s.channels;++c) {o.offsets.push_back(r.real());o.scales.push_back(r.real());}
        const auto count=r.integer(8);if(!count) throw std::invalid_argument("raw classifier has no feature scales");o.sigmas.clear();for(std::uint64_t i=0;i<count;++i) {const auto x=r.real(),y=r.real();o.sigmas.push_back({x,y});}
        const auto flags=r.integer(3);o.gradient_magnitude=flags&1;o.laplacian=flags&2;o.work_bytes=std::size_t(r.integer(1024ULL*1024*1024));d->resolve();return RawPixelRecipe(std::move(d));
    }
};
struct RawPixelClassifier::Data {
    ForestModel forest;RawPixelRecipe recipe;std::vector<std::uint8_t> bytes;std::string sha;
    Data(const ForestModel& f,const RawPixelRecipe& r):forest(f),recipe(r) {}
};
std::vector<std::uint8_t> RawPixelRecipe::bytes() const {
    Writer w;w.bytes={'Q','X','S',1};RawPixelModelCodec::write(w,*this);const auto hash=digest(w.bytes.data(),w.bytes.size());w.bytes.insert(w.bytes.end(),hash.begin(),hash.end());if(w.bytes.size()>128*1024) throw std::length_error("raw pixel recipe exceeds 128 KiB");return std::move(w.bytes);
}
RawPixelRecipe RawPixelRecipe::from_bytes(const std::vector<std::uint8_t>& bytes) {
    if(bytes.size()<68 || bytes.size()>128*1024 || std::memcmp(bytes.data(),"QXS\1",4) || digest(bytes.data(),bytes.size()-64)!=std::string(reinterpret_cast<const char*>(bytes.data()+bytes.size()-64),64)) throw std::invalid_argument("invalid raw pixel recipe encoding");
    Reader r{bytes.data()+4,bytes.size()-68};auto recipe=RawPixelModelCodec::read(r);if(r.left || recipe.bytes()!=bytes) throw std::invalid_argument("noncanonical raw pixel recipe encoding");return recipe;
}
RawPixelClassifier::RawPixelClassifier(const ForestModel& forest,const RawPixelRecipe& recipe) {
    const auto expected=recipe.feature_schema();const auto& actual=forest.features();if(expected.size()!=actual.size()) throw std::invalid_argument("raw classifier forest schema mismatch");
    for(std::size_t i=0;i<expected.size();++i) if(expected[i].name!=actual[i].name || expected[i].unit!=actual[i].unit) throw std::invalid_argument("raw classifier forest schema mismatch");
    auto d=std::make_shared<Data>(forest,recipe);Writer w;RawPixelModelCodec::write(w,recipe);const auto bytes=forest.bytes();w.integer(bytes.size());w.bytes.insert(w.bytes.end(),bytes.begin(),bytes.end());const auto checksum=digest(w.bytes.data(),w.bytes.size());w.bytes.insert(w.bytes.end(),checksum.begin(),checksum.end());
    if(w.bytes.size()>model_limit) throw std::length_error("raw classifier model exceeds limit");w.bytes.shrink_to_fit();d->bytes=std::move(w.bytes);d->sha=digest(d->bytes.data(),d->bytes.size());data_=std::move(d);
}
const ForestModel& RawPixelClassifier::forest() const {return data_->forest;}
const RawPixelRecipe& RawPixelClassifier::recipe() const {return data_->recipe;}
const std::vector<std::uint8_t>& RawPixelClassifier::bytes() const {return data_->bytes;}
const std::string& RawPixelClassifier::sha256() const {return data_->sha;}
RawPixelClassifier RawPixelClassifier::from_bytes(const std::vector<std::uint8_t>& bytes) {
    if(bytes.size()<68 || bytes.size()>model_limit || (std::memcmp(bytes.data(),"QRM\1",4) && std::memcmp(bytes.data(),"QPR\1",4))) throw std::invalid_argument("unsupported raw classifier model encoding");
    if(digest(bytes.data(),bytes.size()-64)!=std::string(reinterpret_cast<const char*>(bytes.data()+bytes.size()-64),64)) throw std::invalid_argument("raw classifier checksum mismatch");
    Reader r{bytes.data()+4,bytes.size()-68};const auto recipe=RawPixelModelCodec::read(r);const auto size=r.integer(64u*1024*1024);
    if(size!=r.left) throw std::invalid_argument("raw classifier forest size mismatch");const auto forest=ForestModel::from_bytes(std::vector<std::uint8_t>(r.data,r.data+r.left));RawPixelClassifier model(forest,recipe);
    if(bytes[1]=='P') {
        // Explicit compatibility with the pre-release raw QPR1 model. RGB
        // pixel records also use QPR1; never infer the type from that header.
        auto legacy=model.bytes();legacy[1]='P';legacy[2]='R';legacy.resize(legacy.size()-64);const auto checksum=digest(legacy.data(),legacy.size());legacy.insert(legacy.end(),checksum.begin(),checksum.end());
        if(legacy!=bytes) throw std::invalid_argument("noncanonical legacy raw classifier model encoding");
        auto data=std::make_shared<Data>(forest,recipe);data->bytes=bytes;data->sha=digest(bytes.data(),bytes.size());model.data_=std::move(data);
    }else if(model.bytes()!=bytes) throw std::invalid_argument("noncanonical raw classifier model encoding");return model;
}
RawPixelClassificationPlan RawPixelClassifierKernel::plan(const RawImageMetadata& source,const ScientificGrid& grid,const ScientificRawOptions& selection,const RawPixelClassifier& model,std::size_t budget,std::optional<RawClassifierCalibration> calibration) {
    auto metadata=source;if(calibration) {metadata.pixel_width_um=calibration->x;metadata.pixel_height_um=calibration->y;}
    if(!model.recipe().matches(metadata,grid,selection)) throw std::invalid_argument("raw classifier channel, dtype, selection or sampling recipe mismatch");
    auto options=model.recipe().features();if(!budget) budget=options.work_bytes;if(budget<1024*1024 || budget>1024ULL*1024*1024) throw std::invalid_argument("raw classifier budget must be 1..1024 MiB");
    const auto count=std::size_t(grid.x().samples())*std::size_t(grid.y().samples()),columns=model.forest().features().size(),classes=model.forest().classes().size();
    const auto batch=std::min<std::size_t>(512,count);const auto fixed=262144+2*model.bytes().size()+batch*(sizeof(ClassifierPrediction)+sizeof(std::size_t)+4*(columns+classes));
    if(fixed>=budget || count>(budget-fixed)/9) throw std::length_error("raw classifier output/votes exceed work budget");const auto retained=fixed+9*count;
    if(budget-retained<1024*1024) throw std::length_error("raw classifier feature budget is too small");options.work_bytes=std::min(options.work_bytes,budget-retained);
    return {options,detail::raw_feature_work_bytes(metadata,grid,selection,options)+retained};
}
std::shared_ptr<const RawPixelClassification> classify_raw_pixels(const ScientificRawGrid& input,const RawPixelClassifier& model,double minimum,std::size_t budget,const std::atomic<bool>* cancelled) {return RawPixelClassifierKernel::run(input,model,minimum,budget,cancelled);}
std::shared_ptr<const RawPixelClassification> RawPixelClassifierKernel::run(const ScientificRawGrid& input,const RawPixelClassifier& model,double minimum,std::size_t budget,const std::atomic<bool>* cancelled,std::optional<RawClassifierCalibration> calibration) {
    stopped(cancelled);if(!std::isfinite(minimum) || minimum<0 || minimum>1) throw std::invalid_argument("raw classifier minimum vote fraction must be 0..1");if(minimum==0) minimum=0;
    const auto admitted=plan(input.metadata(),input.grid(),input.options(),model,budget,calibration);const auto& options=admitted.features;const auto work=admitted.work;
    const auto count=std::size_t(input.grid().x().samples())*std::size_t(input.grid().y().samples()),columns=model.forest().features().size(),classes=model.forest().classes().size(),batch=std::min<std::size_t>(512,count);
    const auto features=detail::extract_raw_pixel_features(input,options,cancelled);auto output=std::shared_ptr<RawPixelClassification>(new RawPixelClassification(input.grid()));output->work_=work;output->valid_=features->rows_valid;output->labels_.assign(count,0);output->fractions_.assign(count,std::numeric_limits<float>::quiet_NaN());output->counts_.resize(classes);output->model_sha_=model.sha256();output->feature_sha_=features->sha256;output->input_sha_=features->input_sha256;
    std::vector<std::size_t> indices;std::vector<float> matrix;indices.reserve(batch);matrix.reserve(batch*columns);
    auto flush=[&] {if(indices.empty()) return;stopped(cancelled);const auto predictions=model.forest().predict(matrix,model.forest().features());for(std::size_t i=0;i<indices.size();++i) {const auto row=indices[i];const auto& prediction=predictions[i];output->fractions_[row]=static_cast<float>(prediction.vote_fraction);if(prediction.vote_fraction>=minimum) {output->labels_[row]=prediction.class_index+1;++output->counts_[prediction.class_index];}}indices.clear();matrix.clear();};
    for(std::size_t row=0;row<count;++row) {if((row&511)==0) stopped(cancelled);if(!output->valid_[row]) continue;indices.push_back(row);matrix.insert(matrix.end(),features->values.begin()+row*columns,features->values.begin()+(row+1)*columns);if(indices.size()==batch) flush();}flush();
    Sha256 hash;text(hash,"qpx.raw-pixel-classification/1");text(hash,output->model_sha_);text(hash,output->feature_sha_);std::uint64_t threshold;std::memcpy(&threshold,&minimum,8);hash.integer(threshold);
    for(std::size_t row=0;row<count;++row) {if((row&4095)==0) stopped(cancelled);hash.integer(output->valid_[row]);hash.integer(output->labels_[row]);std::uint32_t bits=0x7fc00000;if(output->valid_[row]) std::memcpy(&bits,&output->fractions_[row],4);hash.integer(bits);}output->sha_=hash.finish();stopped(cancelled);return output;
}
RawPixelClassifier load_raw_pixel_classifier(const std::string& input,const std::atomic<bool>* cancelled) {
    stopped(cancelled);detail::bounded_utf8(input,32768);const auto path=detail::export_path(input);detail::ReaderFileGuard guard(path);const auto size=guard.version()->bytes;
    if(size>model_limit) throw std::length_error("raw classifier model exceeds limit");std::ifstream stream(std::filesystem::u8path(path),std::ios::binary);if(!stream) throw std::runtime_error("cannot open raw classifier model");std::vector<std::uint8_t> bytes(std::size_t(size),0);
    for(std::size_t offset=0;offset<bytes.size();) {stopped(cancelled);const auto n=std::min<std::size_t>(65536,bytes.size()-offset);stream.read(reinterpret_cast<char*>(bytes.data()+offset),std::streamsize(n));if(stream.gcount()!=std::streamsize(n)) throw std::runtime_error("raw classifier model read failed or changed");offset+=n;}
    if(stream.peek()!=std::char_traits<char>::eof() || stream.bad()) throw std::runtime_error("raw classifier model changed");guard.verify();auto model=RawPixelClassifier::from_bytes(bytes);guard.verify();stopped(cancelled);return model;
}
void save_raw_pixel_classifier(const RawPixelClassifier& model,const std::string& input,bool replace,const std::atomic<bool>* cancelled) {
    stopped(cancelled);detail::bounded_utf8(input,32768);const auto path=detail::export_path(input);std::unique_ptr<detail::ReaderFileGuard> guard;
    if(replace && std::filesystem::exists(std::filesystem::u8path(path))) {guard=std::make_unique<detail::ReaderFileGuard>(path);(void)load_raw_pixel_classifier(path,cancelled);guard->verify();}
    detail::AtomicExportFile file(path,model_limit,cancelled);file.write(reinterpret_cast<const char*>(model.bytes().data()),model.bytes().size());const bool replacing=bool(guard);if(guard) {guard->verify();guard.reset();}file.commit(replacing);
}
}

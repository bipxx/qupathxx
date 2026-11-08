#include "qupathxx/raw_image.hpp"
#include "image_axis_calibration_codec.hpp"
#include "sha256.hpp"
#include "qupathxx/core.hpp"
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>
namespace qpx {
ImageAxisCalibration::ImageAxisCalibration(std::optional<double> z,std::vector<double> times):z_(z),times_(std::move(times)) {
    if(z_ && (!std::isfinite(*z_) || *z_<=0))throw std::invalid_argument("Z spacing must be finite and positive");
    if(times_.size()>max_timepoints)throw std::invalid_argument("image calibration exceeds 65536 timestamps");
    for(auto& t:times_) {
        if(!std::isfinite(t))throw std::invalid_argument("image timestamps must be finite");
        if(t==0)t=0; // Canonical zero; offsets/order otherwise stay exact.
    }
}
std::optional<double> ImageAxisCalibration::timepoint_seconds(std::size_t i) const {
    return i<times_.size()?std::optional<double>(times_[i]):std::nullopt;
}
std::optional<double> Document::z_spacing_um() const {
    if(image_axis_calibration_)return image_axis_calibration_->z_spacing_um();
    const auto raw=server_->raw_metadata();return raw?raw->z_spacing_um:std::nullopt;
}
std::optional<double> Document::z_position_um(std::uint32_t index) const {
    const auto raw=server_->raw_metadata();if(index>=(raw?raw->z_planes:1))throw std::out_of_range("Z index exceeds source domain");
    const auto spacing=image_axis_calibration_?image_axis_calibration_->z_spacing_um():(raw?raw->z_spacing_um:std::nullopt);
    if(!spacing)return {};const auto position=*spacing*index;if(!std::isfinite(position))throw std::overflow_error("physical Z position overflow");return position;
}
std::optional<double> Document::timepoint_seconds(std::uint32_t index) const {
    const auto raw=server_->raw_metadata();if(index>=(raw?raw->timepoints:1))throw std::out_of_range("time index exceeds source domain");
    if(image_axis_calibration_)return image_axis_calibration_->timepoint_seconds(index);
    if(!raw || !raw->time_interval_seconds)return {};const auto time=*raw->time_interval_seconds*index;
    if(!std::isfinite(time))throw std::overflow_error("physical time position overflow");return time;
}
namespace detail {
namespace {
void integer(std::vector<std::uint8_t>& out,std::uint64_t value) {for(int i=0;i<8;++i)out.push_back(std::uint8_t(value>>(8*i)));}
void real(std::vector<std::uint8_t>& out,double value) {std::uint64_t bits;static_assert(sizeof(bits)==sizeof(value));std::memcpy(&bits,&value,8);integer(out,bits);}
struct Reader {
    const std::uint8_t* p;std::size_t left;
    std::uint64_t integer() {
        if(left<8)throw std::invalid_argument("truncated image axis calibration");
        std::uint64_t value=0;for(int i=0;i<8;++i)value|=std::uint64_t(p[i])<<(8*i);p+=8;left-=8;return value;
    }
    double real() {const auto bits=integer();double value;std::memcpy(&value,&bits,8);return value;}
};
}
std::vector<std::uint8_t> encode_image_axis_calibration(const ImageAxisCalibration& value) {
    std::vector<std::uint8_t> out{'Q','A','C',1,std::uint8_t(value.z_spacing_um().has_value())};
    if(value.z_spacing_um())real(out,*value.z_spacing_um());
    integer(out,value.timepoints_seconds().size());for(double t:value.timepoints_seconds())real(out,t);
    Sha256 hash;hash.update(out.data(),out.size());const auto digest=hash.finish();out.insert(out.end(),digest.begin(),digest.end());return out;
}
ImageAxisCalibration decode_image_axis_calibration(const void* data,std::size_t n) {
    constexpr auto limit=85+8*ImageAxisCalibration::max_timepoints;
    if(!data || n<77 || n>limit)throw std::invalid_argument("invalid image axis calibration payload size");
    const auto* p=static_cast<const std::uint8_t*>(data);
    if(std::memcmp(p,"QAC\1",4) || p[4]>1)throw std::invalid_argument("unsupported image axis calibration encoding");
    Sha256 hash;hash.update(p,n-64);
    if(hash.finish()!=std::string(reinterpret_cast<const char*>(p+n-64),64))throw std::invalid_argument("image axis calibration checksum mismatch");
    Reader reader{p+5,n-69};std::optional<double> z;if(p[4])z=reader.real();
    const auto count=reader.integer();
    if(count>ImageAxisCalibration::max_timepoints || reader.left!=count*8)throw std::invalid_argument("invalid image timestamp count or trailing data");
    std::vector<double> times;times.reserve(std::size_t(count));for(std::uint64_t i=0;i<count;++i)times.push_back(reader.real());
    ImageAxisCalibration result(z,std::move(times));const auto canonical=encode_image_axis_calibration(result);
    if(canonical.size()!=n || std::memcmp(canonical.data(),data,n))throw std::invalid_argument("noncanonical image axis calibration");return result;
}
}
}

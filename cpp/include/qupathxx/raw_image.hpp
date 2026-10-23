#pragma once
#include "qupathxx/image_server.hpp"
#include <array>
#include <cmath>
#include <limits>
#include <variant>

namespace qpx {
enum class SampleType { UInt8,UInt16,Int8,Int16,UInt32,Int32,UInt64,Int64,Float32,Float64 };
std::size_t sample_bytes(SampleType);
std::string sample_name(SampleType);
struct RawImageMetadata {
    std::int64_t width=0,height=0;
    std::uint32_t channels=0,z_planes=1,timepoints=1,series=0,series_count=1;
    SampleType sample_type=SampleType::UInt8;
    std::vector<ImageLevel> levels;
    std::vector<std::string> channel_names;
    std::optional<double> pixel_width_um,pixel_height_um,z_spacing_um,time_interval_seconds;
    std::string dimension_order;
    std::size_t payload_bytes() const;
};
struct RawRegionRequest {
    RegionRequest region{0,0,0,1,1};
    std::uint32_t first_channel=0,channels=1,z=0,t=0;
    void validate(const RawImageMetadata&) const;
};
using RawPixelStorage=std::variant<std::vector<std::uint8_t>,std::vector<std::uint16_t>,
    std::vector<std::int8_t>,std::vector<std::int16_t>,std::vector<std::uint32_t>,std::vector<std::int32_t>,
    std::vector<std::uint64_t>,std::vector<std::int64_t>,std::vector<float>,std::vector<double>>;
// Native-endian HWC data. Validity distinguishes out-of-bounds zero padding
// from actual zero samples; NaN/Inf in valid source pixels are preserved.
class RawPixelBuffer {
public:
    RawPixelBuffer(std::int64_t width,std::int64_t height,std::uint32_t channels,SampleType);
    std::int64_t width() const {return width_;}std::int64_t height() const {return height_;}
    std::uint32_t channels() const {return channels_;}SampleType sample_type() const {return type_;}
    std::size_t payload_bytes() const;
    const RawPixelStorage& storage() const {return values_;}
    const std::vector<std::uint8_t>& valid() const {return valid_;}
    RawPixelStorage& writable_storage() {return values_;}
    std::vector<std::uint8_t>& writable_valid() {return valid_;}
private:
    std::int64_t width_,height_;std::uint32_t channels_;SampleType type_;
    RawPixelStorage values_;std::vector<std::uint8_t> valid_;
};
struct TiffView {
    std::uint32_t series=0,z=0,t=0;
    // -2 means automatic selection on open; -1 disables that output color.
    std::array<std::int32_t,3> channels{-2,-2,-2};
    std::array<double,3> lower{NAN,NAN,NAN},upper{NAN,NAN,NAN};
    void validate(bool resolved=false) const;
};
struct TiffOpenOptions {
    TiffView view;
    std::size_t decode_bytes=64*1024*1024,cache_bytes=64*1024*1024;
    void validate() const;
};
struct TiffReadStats {
    std::uint64_t decoded_blocks=0,cache_hits=0,decoded_bytes=0,directory_switches=0;
    std::size_t cache_bytes=0,peak_cache_bytes=0;
};
std::shared_ptr<ImageServer> open_tiff(const std::string& path,TiffOpenOptions options={},const std::atomic<bool>* cancelled=nullptr);
bool has_tiff();
bool is_ome_tiff_path(const std::string&);
std::string tiff_library_version();
std::string tiff_view_encode(const TiffView&);
TiffView tiff_view_decode(const std::string&);
void save_raw_numpy(const RawPixelBuffer&,const std::string& path,bool replace_existing=false,const std::atomic<bool>* cancelled=nullptr);
struct RawRegionStatistics {
    std::uint64_t pixels=0,valid_pixels=0;
    std::vector<std::uint64_t> finite_samples,nonfinite_samples;
    std::vector<std::optional<double>> means,minima,maxima;
};
class Roi;
RawRegionStatistics raw_region_statistics(const ImageServer&,const RawRegionRequest&,std::size_t tile_size=256,
    const Roi* mask=nullptr,std::uint64_t max_pixels=1000000000,const std::atomic<bool>* cancelled=nullptr);
}

#pragma once
#include "qupathxx/raw_image.hpp"
namespace qpx {
// Quantitative blocks are independent of RGB display views and TIFF APIs.
// Immutable identities describe observed source versions, not a pixel-content
// fingerprint. Implementations check those versions at read trust boundaries.
class ChannelSource {
public:
    virtual ~ChannelSource()=default;
    virtual const RawImageMetadata& metadata() const=0;
    virtual const std::string& domain() const=0;
    virtual const std::string& sha256() const=0;
    virtual std::size_t payload_bytes() const=0;
    // Accounted native snapshot/buffer peak; excludes existing reader/OS caches.
    virtual std::size_t read_work_bytes(const RawRegionRequest&) const=0;
    virtual void require_current(const std::atomic<bool>* cancelled=nullptr) const=0;
    virtual RawPixelBuffer read(const RawRegionRequest&,std::size_t work_bytes=256u*1024*1024,
        const std::atomic<bool>* cancelled=nullptr) const=0;
};
// Adapts existing native raw_metadata/read_raw capabilities, without requiring a
// TIFF display view or changing any legacy reader/workflow identity.
std::shared_ptr<const ChannelSource> original_channel_source(std::shared_ptr<ImageServer>,
    const std::atomic<bool>* cancelled=nullptr);
struct DarkFlatOptions {
    // (signal-dark)*scale[channel]/(flat-dark). Explicit positive source-sample
    // normalization factors; no tile-local mean, implicit clamping or broadcast.
    std::vector<double> scales;
    double minimum_denominator=0;
    void validate() const;
    std::string sha256() const;
};
// Matching HWC/validity lattices; valid pixels with bad channel numerics produce
// NaN for that channel. Intersection validity denotes missing source pixels.
// Float64 output retains signed values; original integer values beyond +/-2^53
// refuse conversion instead of silently losing precision.
RawPixelBuffer correct_dark_flat(const RawPixelBuffer& signal,const RawPixelBuffer& dark,
    const RawPixelBuffer& flat,const DarkFlatOptions&,std::uint32_t first_channel=0,
    std::size_t work_bytes=256u*1024*1024,const std::atomic<bool>* cancelled=nullptr);
// Level-zero, identical C/Z/T/spatial axes and calibration; fields do not
// broadcast. Original samples remain immutable and are the default elsewhere.
std::shared_ptr<const ChannelSource> dark_flat_channel_source(
    std::shared_ptr<const ChannelSource> signal,std::shared_ptr<const ChannelSource> dark,
    std::shared_ptr<const ChannelSource> flat,DarkFlatOptions);
}

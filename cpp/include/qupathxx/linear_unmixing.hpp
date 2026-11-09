#pragma once
#include "qupathxx/channel_source.hpp"
#include "qupathxx/image_data_properties.hpp"
namespace qpx {
struct LinearMixingChannel {std::uint32_t channel=0;std::string name;};
struct LinearMixingMatrix {
    std::string input_domain="original-channel-samples";
    std::vector<LinearMixingChannel> inputs;
    std::vector<std::string> components;
    // Row-major measured-channel x component: measured = matrix * components.
    // Explicit dimensionless coefficients, no spectrum estimation or clamping.
    std::vector<double> coefficients;
    double maximum_condition=1e8;
    void validate() const;
    std::string sha256() const;
    std::size_t payload_bytes() const;
    std::vector<std::string> output_names() const;
    // Strict typed persistence, preserving coefficient bits (including -0).
    // Does not factorize or require the optional numerical backend.
    ImageDataValue to_value() const;
    static LinearMixingMatrix from_value(const ImageDataValue&);
};
bool has_linear_unmixing();
class LinearUnmixModel {
public:
    // Native thin SVD once per immutable matrix. Requires1..64 components,
    // components<=measured channels<=64 and a representable finite operator.
    explicit LinearUnmixModel(LinearMixingMatrix,const std::atomic<bool>* cancelled=nullptr);
    const LinearMixingMatrix& matrix() const;
    double condition() const;
    std::size_t payload_bytes() const;
private:
    struct Data;std::shared_ptr<const Data> data_;
    friend struct LinearUnmixKernel;
};
// Input HWC channels correspond to matrix rows, in exactly that order. Outputs
// are components followed by signed residuals (measured - reconstructed), named
// residual:<input label>. A zero count selects all outputs from first_output.
// Source padding remains invalid; any nonfinite input makes the coupled solve
// unknown (NaN). Per-component overflow is NaN; residuals then remain unknown.
RawPixelBuffer linear_unmix_block(const RawPixelBuffer&,const LinearUnmixModel&,
    std::uint32_t first_output=0,std::uint32_t output_channels=0,
    std::size_t work_bytes=256u*1024*1024,const std::atomic<bool>* cancelled=nullptr);
// Explicit source-index/name/domain binding. Sparse selected channel groups are
// gathered without reading/densifying gaps. Only requested output channels are
// allocated; residual reads reuse a single per-pixel component workspace.
std::shared_ptr<const ChannelSource> linear_unmix_channel_source(
    std::shared_ptr<const ChannelSource>,LinearMixingMatrix,
    const std::atomic<bool>* cancelled=nullptr);
std::shared_ptr<const ChannelSource> linear_unmix_channel_source(
    std::shared_ptr<const ChannelSource>,LinearUnmixModel,
    const std::atomic<bool>* cancelled=nullptr);
}

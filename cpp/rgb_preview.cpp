#include "qupathxx/rgb_preview.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/raw_image.hpp"
#include "utf8_text.hpp"
#include "ascii_case.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>
namespace qpx {
namespace {
void checkpoint(const std::atomic<bool>* cancelled) {
    if(cancelled && cancelled->load())throw std::runtime_error("RGB nominal preview cancelled");
}
}
std::shared_ptr<ImageServer> open_rgb_nominal_source(const std::string& path,const std::string& requested) {
    detail::bounded_utf8(path,32768);if(path.empty())throw std::invalid_argument("Source path must not be empty");
    auto reader=requested;
    if(reader=="auto") {
        auto suffix=std::filesystem::u8path(path).extension().u8string();detail::ascii_lower_in_place(suffix);
        reader=suffix==".ppm"?"ppm":suffix==".tif" || suffix==".tiff"?"tiff":"openslide";
    }
    if(reader=="ppm")return dense_image_server(Image::read_ppm(path));
    if(reader=="tiff")return open_tiff(path);
    if(reader=="openslide")return open_slide(path);
    throw std::invalid_argument("Source reader must be auto, ppm, tiff or openslide");
}
std::shared_ptr<Document> make_rgb_nominal_preview(const std::shared_ptr<ImageServer>& source,
    const RgbNominalRequest& request,std::optional<double> px,std::optional<double> py,const std::atomic<bool>* cancelled) {
    checkpoint(cancelled);if(!source)throw std::invalid_argument("RGB nominal preview requires a source");
    for(auto scale:{px,py})if(scale && (!std::isfinite(*scale) || *scale<=0))throw std::invalid_argument("Source calibration must be positive and finite");
    std::optional<double> x,y;
    if(px && py) {
        x=*px*request.downsample;y=*py*request.downsample;
        if(!std::isfinite(*x) || !std::isfinite(*y) || *x<=0 || *y<=0)throw std::invalid_argument("Nominal preview calibration is outside the finite positive range");
    }
    const auto pixels=read_rgb_nominal_region(*source,request,cancelled);
    std::vector<std::uint8_t> rgb(pixels.bytes().size()/4*3);
    for(std::size_t i=0;i<pixels.bytes().size()/4;++i) {
        if(i%65536==0)checkpoint(cancelled);
        std::copy_n(pixels.bytes().data()+4*i,3,rgb.data()+3*i);
    }
    checkpoint(cancelled);
    auto preview=std::make_shared<Document>(Image(static_cast<int>(pixels.width()),static_cast<int>(pixels.height()),std::move(rgb)));
    if(x && y)preview->set_calibration(*x,*y);
    return preview;
}
std::shared_ptr<Document> make_rgb_nominal_preview(const Document& source,const RgbNominalRequest& request,const std::atomic<bool>* cancelled) {
    return make_rgb_nominal_preview(source.server(),request,source.pixel_width_um(),source.pixel_height_um(),cancelled);
}
}

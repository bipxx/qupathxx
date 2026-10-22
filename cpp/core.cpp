#include "qupathxx/core.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <limits>
#include <stdexcept>

namespace qpx {
namespace {
void positive(double v) {
    if (!std::isfinite(v) || v <= 0) throw std::invalid_argument("value must be finite and positive");
}
double physical_area(double area,double sx,double sy) {
    positive(sx); positive(sy);
    auto result=area*sx*sy;
    if(!std::isfinite(result) || (result==0 && area!=0)) {
        // Preserve normal-range rounding, but avoid intermediate overflow or
        // underflow when reciprocal calibration axes yield a finite product.
        int a=0,x=0,y=0;
        const auto mantissa=std::frexp(area,&a)*std::frexp(sx,&x)*std::frexp(sy,&y);
        result=std::scalbn(mantissa,a+x+y);
    }
    if(!std::isfinite(result)) throw std::overflow_error("physical area exceeds numeric range");
    return result;
}
std::size_t pixel_count(int w, int h) {
    if (w <= 0 || h <= 0 || static_cast<std::size_t>(w) > std::numeric_limits<std::size_t>::max() / 3 / static_cast<std::size_t>(h))
        throw std::invalid_argument("invalid image dimensions");
    return static_cast<std::size_t>(w) * h * 3;
}
std::string token(std::istream& in) {
    std::string result;
    while (in >> std::ws && in.peek() == '#') in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    if (!(in >> result)) throw std::runtime_error("incomplete PPM header");
    return result;
}
int integer_token(std::istream& in) {
    auto s = token(in); std::size_t used = 0;
    int value = std::stoi(s, &used);
    if (used != s.size()) throw std::invalid_argument("invalid PPM integer");
    return value;
}
}
void Rect::validate() const {
    positive(width); positive(height);
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(x + width) || !std::isfinite(y + height) || !std::isfinite(width * height))
        throw std::invalid_argument("rectangle must have finite coordinates and area");
}
double Rect::area() const { validate(); return width * height; }
bool Rect::contains(Point p) const { return p.x >= x && p.y >= y && p.x < x + width && p.y < y + height; }
Image::Image(int w, int h, std::vector<std::uint8_t> rgb) : width_(w), height_(h), rgb_(std::make_shared<const std::vector<std::uint8_t>>(std::move(rgb))) {
    if (rgb_->size() != pixel_count(w, h)) throw std::invalid_argument("expected width * height * 3 RGB bytes");
}
Image Image::demo() {
    constexpr int w = 1024, h = 768;
    std::vector<std::uint8_t> data(pixel_count(w, h));
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        double dx = (x % 64) - 32., dy = (y % 64) - 32.;
        bool nucleus = dx * dx / 180 + dy * dy / 100 < 1;
        auto i = (static_cast<std::size_t>(y) * w + x) * 3;
        data[i] = nucleus ? 95 : 235; data[i+1] = nucleus ? 65 : 190; data[i+2] = nucleus ? 145 : 210;
    }
    return Image(w, h, std::move(data));
}
Image Image::read_ppm(const std::string& path) {
    std::ifstream in(std::filesystem::u8path(path), std::ios::binary);
    if (!in) throw std::runtime_error("cannot open image: " + path);
    if (token(in) != "P6") throw std::invalid_argument("only binary P6 PPM is supported");
    int w = integer_token(in), h = integer_token(in), max = integer_token(in);
    if (max != 255) throw std::invalid_argument("PPM maximum must be 255");
    char separator = 0; in.get(separator);
    if (separator == '\r' && in.peek() == '\n') in.get();
    else if (separator != ' ' && separator != '\n' && separator != '\t' && separator != '\r')
        throw std::invalid_argument("missing PPM raster separator");
    auto count = pixel_count(w, h);
    auto start = in.tellg(); in.seekg(0, std::ios::end); auto end = in.tellg();
    if (start < 0 || end < start || static_cast<std::uint64_t>(end - start) < count)
        throw std::runtime_error("truncated PPM raster");
    in.seekg(start);
    std::vector<std::uint8_t> data(count);
    if (!in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(count))) throw std::runtime_error("truncated PPM raster");
    return Image(w, h, std::move(data));
}
Document::Document(Image image) : Document(dense_image_server(std::move(image))) {}
Document::Document(std::shared_ptr<ImageServer> server) : server_(std::move(server)) {
    if (!server_ || server_->closed()) throw std::invalid_argument("document requires an open image server");
    pixel_width_um_ = metadata().pixel_width_um; pixel_height_um_ = metadata().pixel_height_um;
}
const Annotation& Document::find(std::uint64_t id) const {
    auto it=objects_.find(id); if (it!=objects_.end()) return it->second;
    throw std::out_of_range("annotation does not exist");
}
Rect Annotation::bounds() const {
    auto b=geometry.bounds(); if(!nucleus) return b;
    auto n=nucleus->bounds();
    auto right=std::max(b.x+b.width,n.x+n.width),bottom=std::max(b.y+b.height,n.y+n.height);
    auto left=std::min(b.x,n.x),top=std::min(b.y,n.y);
    return {left,top,right-left,bottom-top};
}
double Document::area_um2(std::uint64_t id) const {
    const auto area = find(id).geometry.area();
    if (!pixel_width_um_ || !pixel_height_um_) throw std::runtime_error("physical calibration is unknown");
    return physical_area(area,*pixel_width_um_,*pixel_height_um_);
}
double Document::mean_intensity(std::uint64_t id) const {
    return mean_rgb(*server_,find(id).geometry);
}
ShapeMeasurements Document::shape_measurements(std::uint64_t id) const {
    const auto& a=find(id); // A resident stale entry must never resurrect a deleted object.
    auto& cache=feature_cache_;
    auto found=cache.entries.find(id);
    if(found!=cache.entries.end() && found->second.object_revision==a.revision &&
       found->second.calibration_revision==calibration_revision_) {
        if(cache.hits!=std::numeric_limits<std::uint64_t>::max()) ++cache.hits;
        cache.lru.splice(cache.lru.begin(),cache.lru,found->second.position);
        return found->second.value;
    }
    if(cache.misses!=std::numeric_limits<std::uint64_t>::max()) ++cache.misses;
    auto measure=[&](const Roi& roi) {
        RoiMeasurements value{roi.area(),roi.perimeter(),{}, {}};
        if(pixel_width_um_ && pixel_height_um_) {
            value.area_um2=physical_area(value.area_px2,*pixel_width_um_,*pixel_height_um_);
            // Reuse the already measured pixel length for isotropic calibration.
            const auto length=*pixel_width_um_==*pixel_height_um_?
                value.length_px * *pixel_width_um_:roi.scaled_length(*pixel_width_um_,*pixel_height_um_);
            if(!std::isfinite(length)) throw std::overflow_error("scaled geometry length exceeds numeric range");
            value.length_um=length;
        }
        return value;
    };
    ShapeMeasurements result{measure(a.geometry),{}};
    if(a.nucleus) result.nucleus=measure(*a.nucleus);
    if(!cache.capacity) return result;
    if(found!=cache.entries.end()) {
        found->second.value=result;
        found->second.object_revision=a.revision;
        found->second.calibration_revision=calibration_revision_;
        cache.lru.splice(cache.lru.begin(),cache.lru,found->second.position);
    } else {
        cache.lru.push_front(id);
        try {cache.entries.emplace(id,FeatureCache::Entry{a.revision,calibration_revision_,result,cache.lru.begin()});}
        catch(...) {cache.lru.pop_front(); throw;}
        while(cache.entries.size()>cache.capacity) {
            cache.entries.erase(cache.lru.back()); cache.lru.pop_back();
        }
    }
    return result;
}
FeatureCacheStats Document::feature_cache_stats() const {
    return {feature_cache_.entries.size(),feature_cache_.capacity,feature_cache_.hits,feature_cache_.misses};
}
void Document::set_feature_cache_limit(std::size_t entries) {
    if(entries>65536) throw std::invalid_argument("feature cache limit must be at most 65536 entries");
    auto& cache=feature_cache_; cache.capacity=entries;
    while(cache.entries.size()>entries) {cache.entries.erase(cache.lru.back()); cache.lru.pop_back();}
}
void Document::clear_feature_cache() {
    auto& cache=feature_cache_; cache.entries.clear(); cache.lru.clear(); cache.hits=cache.misses=0;
}
double Document::nucleus_area_um2(std::uint64_t id) const {
    const auto& a=find(id);
    if(!a.nucleus) throw std::invalid_argument("object has no nucleus ROI");
    if(!pixel_width_um_ || !pixel_height_um_) throw std::runtime_error("physical calibration is unknown");
    return physical_area(a.nucleus->area(),*pixel_width_um_,*pixel_height_um_);
}
double Document::nucleus_mean_intensity(std::uint64_t id) const {
    const auto& a=find(id);
    if(!a.nucleus) throw std::invalid_argument("object has no nucleus ROI");
    return mean_rgb(*server_,*a.nucleus);
}
double mean_rgb(const ImageServer& server, const Rect& r, const std::atomic<bool>* cancelled) {
    return mean_rgb(server,Roi::rectangle(r),cancelled);
}
double mean_rgb(const ImageServer& server, const Roi& geometry, const std::atomic<bool>* cancelled) {
    if(!geometry.is_area()) throw std::invalid_argument("mean intensity requires an area ROI");
    const auto r=geometry.bounds();
    if (r.x<0 || r.y<0 || r.x+r.width>=static_cast<double>(std::numeric_limits<std::int64_t>::max()) ||
        r.y+r.height>=static_cast<double>(std::numeric_limits<std::int64_t>::max()) ||
        static_cast<long double>(r.x+r.width)>server.metadata().width || static_cast<long double>(r.y+r.height)>server.metadata().height)
        throw std::invalid_argument("ROI must lie inside the image");
    // Include pixels whose centers lie in the half-open ROI. Mean of RGB channels.
    auto x0 = static_cast<std::int64_t>(std::ceil(r.x - .5));
    auto y0 = static_cast<std::int64_t>(std::ceil(r.y - .5));
    auto x1 = static_cast<std::int64_t>(std::ceil(r.x + r.width - .5));
    auto y1 = static_cast<std::int64_t>(std::ceil(r.y + r.height - .5));
    if (x0 >= x1 || y0 >= y1) throw std::invalid_argument("ROI contains no pixel centers");
    double sum = 0;
    double samples = 0;
    for (auto y = y0; y < y1;) {
        auto h = std::min<std::int64_t>(256, y1-y);
        for (auto x = x0; x < x1;) {
            auto w = std::min<std::int64_t>(256, x1-x);
            if (cancelled && cancelled->load()) throw std::runtime_error("measurement cancelled");
            auto region = server.read_region({x,y,0,w,h}); const auto& data = region.bytes();
            for (std::size_t i = 0; i < data.size(); i += 4) if (data[i+3]) {
                if(geometry.kind()!=RoiKind::Rectangle) {
                    const auto pixel=i/4;
                    if(!geometry.contains({static_cast<double>(x+pixel%w)+.5,static_cast<double>(y+pixel/w)+.5})) continue;
                }
                sum += data[i]; sum += data[i+1]; sum += data[i+2]; samples += 3;
            }
            x += w;
        }
        y += h;
    }
    if (!samples) throw std::invalid_argument("ROI contains no nontransparent pixels");
    return sum / samples;
}
Point Viewport::image_to_screen(Point p) const { positive(zoom); return {origin.x + p.x * zoom, origin.y + p.y * zoom}; }
Point Viewport::screen_to_image(Point p) const { positive(zoom); return {(p.x - origin.x) / zoom, (p.y - origin.y) / zoom}; }
void Viewport::zoom_at(Point screen, double factor) {
    positive(factor); auto image = screen_to_image(screen);
    zoom = std::clamp(zoom * factor, 1e-6, 1e6);
    origin = {screen.x - image.x * zoom, screen.y - image.y * zoom};
}
void Viewport::fit(std::int64_t w, std::int64_t h, double cw, double ch) {
    if (w<=0 || h<=0) throw std::invalid_argument("image dimensions must be positive");
    positive(cw); positive(ch);
    zoom = std::min(cw / w, ch / h); origin = {(cw - w * zoom) / 2, (ch - h * zoom) / 2};
}
void Viewport::center_on(Point p, double cw, double ch) {
    positive(zoom); positive(cw); positive(ch);
    if (!std::isfinite(p.x) || !std::isfinite(p.y)) throw std::invalid_argument("center must be finite");
    origin={cw/2-p.x*zoom,ch/2-p.y*zoom};
}
}

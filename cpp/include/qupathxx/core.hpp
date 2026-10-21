#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "qupathxx/image_server.hpp"

namespace qpx {
struct Point {
    double x = 0, y = 0;
    Point() = default;
    Point(double x_, double y_) : x(x_), y(y_) {}
};
struct Rect {
    double x = 0, y = 0, width = 0, height = 0;
    Rect() = default;
    Rect(double x_, double y_, double w, double h) : x(x_), y(y_), width(w), height(h) {}
    void validate() const;
    double area() const;
    bool contains(Point p) const;
};
struct Annotation {
    std::uint64_t id;
    std::string name;
    Rect roi;
};
class Image {
public:
    Image(int width, int height, std::vector<std::uint8_t> rgb);
    static Image demo();
    static Image read_ppm(const std::string& path);
    int width() const { return width_; }
    int height() const { return height_; }
    const std::vector<std::uint8_t>& pixels() const { return rgb_; }
private:
    int width_, height_;
    std::vector<std::uint8_t> rgb_;
};
// Coordinates are level-zero image pixels; calibration is micrometers/pixel.
class Document {
public:
    explicit Document(Image image);
    explicit Document(std::shared_ptr<ImageServer> server);
    const Image& image() const { return dense_image(*server_); }
    std::shared_ptr<ImageServer> server() const { return server_; }
    const ImageMetadata& metadata() const { return server_->metadata(); }
    const std::vector<Annotation>& annotations() const { return annotations_; }
    std::uint64_t add_rectangle(Rect roi, std::string name = "Annotation");
    bool remove_annotation(std::uint64_t id);
    void set_calibration(double pixel_width_um, double pixel_height_um);
    std::optional<double> pixel_width_um() const { return pixel_width_um_; }
    std::optional<double> pixel_height_um() const { return pixel_height_um_; }
    void clear_calibration() { pixel_width_um_.reset(); pixel_height_um_.reset(); }
    double area_um2(std::uint64_t id) const;
    double mean_intensity(std::uint64_t id) const;
private:
    const Annotation& find(std::uint64_t id) const;
    std::shared_ptr<ImageServer> server_;
    std::vector<Annotation> annotations_;
    std::uint64_t next_id_ = 1;
    std::optional<double> pixel_width_um_, pixel_height_um_;
};
struct Viewport {
    Point origin;
    double zoom = 1;
    Point image_to_screen(Point p) const;
    Point screen_to_image(Point p) const;
    void zoom_at(Point screen, double factor);
    void fit(int image_width, int image_height, double canvas_width, double canvas_height);
};
// Runs on the calling/main thread. frame_limit=0 is interactive.
void run_gui(Document& document, int frame_limit = 0);
}

#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <atomic>
#include <map>
#include <set>
#include "qupathxx/image_server.hpp"
#include "qupathxx/display.hpp"

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
    std::uint64_t parent_id = 0;
    std::string class_name;
    std::uint64_t revision = 0;
};
enum class ObjectEditKind { Create, Replace, Remove };
struct ObjectEdit {
    ObjectEditKind kind;
    Annotation value;
    static ObjectEdit create(Rect roi, std::string name="Annotation", std::uint64_t parent_id=0, std::string class_name="");
    static ObjectEdit replace(std::uint64_t id, Rect roi, std::string name, std::uint64_t parent_id=0, std::string class_name="");
    static ObjectEdit remove(std::uint64_t id);
};
double mean_rgb(const ImageServer&, const Rect&, const std::atomic<bool>* cancelled = nullptr);
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
    std::vector<Annotation> annotations() const;
    const std::map<std::uint64_t,Annotation>& objects() const { return objects_; }
    Annotation annotation(std::uint64_t id) const { return find(id); }
    std::size_t object_count() const { return objects_.size(); }
    std::uint64_t revision() const { return revision_; }
    std::uint64_t add_rectangle(Rect roi, std::string name = "Annotation");
    bool remove_annotation(std::uint64_t id);
    void update_rectangle(std::uint64_t id, Rect roi);
    void rename_annotation(std::uint64_t id, std::string name);
    void classify_annotation(std::uint64_t id, std::string class_name);
    void reparent_annotation(std::uint64_t id, std::uint64_t parent_id);
    std::vector<std::uint64_t> execute(const std::vector<ObjectEdit>& edits, std::string label="Edit objects");
    std::vector<std::uint64_t> children(std::uint64_t parent_id) const;
    bool undo();
    bool redo();
    bool can_undo() const { return !undo_.empty(); }
    bool can_redo() const { return !redo_.empty(); }
    std::string undo_label() const { return can_undo()?undo_.back().label:""; }
    std::string redo_label() const { return can_redo()?redo_.back().label:""; }
    void clear_history();
    void set_history_limits(std::size_t steps, std::size_t payload_bytes);
    std::size_t history_bytes() const { return history_bytes_; }
    void set_calibration(double pixel_width_um, double pixel_height_um);
    std::optional<double> pixel_width_um() const { return pixel_width_um_; }
    std::optional<double> pixel_height_um() const { return pixel_height_um_; }
    void clear_calibration();
    double area_um2(std::uint64_t id) const;
    double mean_intensity(std::uint64_t id) const;
private:
    const Annotation& find(std::uint64_t id) const;
    using ObjectPatch = std::map<std::uint64_t,std::optional<Annotation>>;
    struct Calibration { std::optional<double> x,y; };
    struct EditRecord {
        ObjectPatch before,after;
        Calibration before_calibration,after_calibration;
        std::string label;
        std::size_t bytes = 0;
    };
    struct Staging;
    void apply_record(const EditRecord&, bool forward);
    void commit(Staging&, Calibration);
    void save_edit(Staging&, Calibration, std::string label);
    void trim_history();
    std::shared_ptr<ImageServer> server_;
    std::map<std::uint64_t,Annotation> objects_;
    std::map<std::uint64_t,std::set<std::uint64_t>> children_;
    std::vector<EditRecord> undo_,redo_;
    std::uint64_t next_id_ = 1;
    std::uint64_t revision_ = 0;
    std::size_t history_steps_ = 128, history_budget_ = 64*1024*1024, history_bytes_ = 0;
    std::optional<double> pixel_width_um_, pixel_height_um_;
};
struct Viewport {
    Point origin;
    double zoom = 1;
    Point image_to_screen(Point p) const;
    Point screen_to_image(Point p) const;
    void zoom_at(Point screen, double factor);
    void fit(std::int64_t image_width, std::int64_t image_height, double canvas_width, double canvas_height);
    void center_on(Point image_point, double canvas_width, double canvas_height);
};
// Runs on the calling/main thread. frame_limit=0 is interactive.
struct GuiReport {
    int frames = 0;
    std::uint64_t uploaded_tiles = 0;
    std::size_t peak_gpu_bytes = 0;
    std::uint64_t generations = 0;
    double cpu_frame_p95_ms = 0;
    std::size_t pending_tiles = 0;
    double final_zoom = 0;
    Point final_origin;
    Point final_center;
    int framebuffer_width = 0, framebuffer_height = 0;
    std::string gl_renderer, gl_version;
};
enum class GuiInputKind { Move, LeftDown, LeftUp, RightDown, RightUp, Wheel, RectangleKey, UndoKey, RedoKey, DeleteKey };
enum class GuiInputTarget { Canvas, Overview };
struct GuiInput {
    int frame = 0;
    GuiInputKind kind = GuiInputKind::Move;
    GuiInputTarget target = GuiInputTarget::Canvas;
    double x = .5, y = .5, wheel = 0;
};
struct GuiOptions {
    int width = 1440, height = 900;
    DisplaySettings display;
    std::vector<GuiInput> inputs;
};
GuiReport run_gui(Document& document, int frame_limit = 0, const std::string& capture_path = "", const GuiOptions& options = {});
}

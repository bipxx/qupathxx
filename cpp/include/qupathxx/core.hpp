#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <atomic>
#include <map>
#include <set>
#include "qupathxx/image_server.hpp"
#include "qupathxx/display.hpp"
#include "qupathxx/geometry.hpp"
#include "qupathxx/spatial_index.hpp"

namespace qpx {
class Project;
enum class ObjectKind { Annotation=0, Detection=1, Cell=2 };
struct ObjectClass {
    std::string name;
    std::uint32_t color = 0x41e1be; // RGB, no alpha.
};
struct Annotation {
    std::uint64_t id;
    std::string name;
    Roi geometry;
    std::uint64_t parent_id = 0;
    std::string class_name;
    std::uint64_t revision = 0;
    ObjectKind kind = ObjectKind::Annotation;
    std::optional<Roi> nucleus;
    Rect bounds() const;
    bool is_detection() const { return kind!=ObjectKind::Annotation; }
};
enum class ObjectEditKind { Create, Replace, Remove };
struct ObjectEdit {
    ObjectEditKind kind;
    Annotation value;
    static ObjectEdit create(Rect roi, std::string name="Annotation", std::uint64_t parent_id=0, std::string class_name="");
    static ObjectEdit replace(std::uint64_t id, Rect roi, std::string name, std::uint64_t parent_id=0, std::string class_name="");
    static ObjectEdit remove(std::uint64_t id);
    static ObjectEdit create_geometry(Roi geometry, std::string name="Annotation", std::uint64_t parent_id=0, std::string class_name="", ObjectKind kind=ObjectKind::Annotation, std::optional<Roi> nucleus=std::nullopt);
    static ObjectEdit replace_geometry(std::uint64_t id, Roi geometry, std::string name, std::uint64_t parent_id=0, std::string class_name="", ObjectKind kind=ObjectKind::Annotation, std::optional<Roi> nucleus=std::nullopt);
};
double mean_rgb(const ImageServer&, const Rect&, const std::atomic<bool>* cancelled = nullptr);
double mean_rgb(const ImageServer&, const Roi&, const std::atomic<bool>* cancelled = nullptr);
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
    std::uint64_t structure_revision() const { return structure_revision_; }
    const std::map<std::string,ObjectClass>& classes() const { return classes_; }
    ObjectClass object_class(const std::string& name) const;
    std::size_t class_count(const std::string& name) const;
    void set_class(std::string name, std::uint32_t color);
    void remove_class(const std::string& name);
    void rename_class(const std::string& old_name, std::string new_name);
    void classify_objects(const std::vector<std::uint64_t>& ids, std::string class_name);
    std::uint64_t add_rectangle(Rect roi, std::string name = "Annotation");
    std::uint64_t add_roi(Roi roi, std::string name="Annotation");
    std::uint64_t add_detection(Roi roi, std::string name="Detection", std::uint64_t parent_id=0, std::string class_name="");
    std::uint64_t add_cell(Roi roi, std::optional<Roi> nucleus=std::nullopt, std::string name="Cell", std::uint64_t parent_id=0, std::string class_name="");
    void update_nucleus(std::uint64_t id, std::optional<Roi> nucleus);
    bool remove_annotation(std::uint64_t id);
    void remove_objects(const std::vector<std::uint64_t>& ids);
    void update_rectangle(std::uint64_t id, Rect roi);
    void update_roi(std::uint64_t id, Roi roi);
    void edit_vertex(std::uint64_t id,std::size_t path,std::size_t vertex,Point point);
    void insert_vertex(std::uint64_t id,std::size_t path,std::size_t vertex,Point point);
    void remove_vertex(std::uint64_t id,std::size_t path,std::size_t vertex);
    void add_hole(std::uint64_t id,std::size_t part,Ring hole);
    void remove_hole(std::uint64_t id,std::size_t part,std::size_t hole);
    void apply_boolean(std::uint64_t target,const std::vector<std::uint64_t>& operands,BooleanOp operation);
    void rename_annotation(std::uint64_t id, std::string name);
    void classify_annotation(std::uint64_t id, std::string class_name);
    void reparent_annotation(std::uint64_t id, std::uint64_t parent_id);
    std::vector<std::uint64_t> execute(const std::vector<ObjectEdit>& edits, std::string label="Edit objects");
    std::vector<std::uint64_t> children(std::uint64_t parent_id) const;
    std::vector<std::uint64_t> query_region(Rect bounds) const;
    std::vector<std::uint64_t> pick(Point point,double tolerance=0) const;
    SpatialStats spatial_stats() const;
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
    double nucleus_area_um2(std::uint64_t id) const;
    double nucleus_mean_intensity(std::uint64_t id) const;
private:
    friend class Project;
    static std::shared_ptr<Document> restore(std::shared_ptr<ImageServer> server,
        const std::vector<Annotation>& objects,const std::vector<ObjectClass>& classes,
        std::optional<double> pixel_width,std::optional<double> pixel_height,
        std::uint64_t next_id,std::uint64_t revision);
    const Annotation& find(std::uint64_t id) const;
    void publish_roi(std::uint64_t id,Roi geometry,std::string label);
    using ObjectPatch = std::map<std::uint64_t,std::optional<Annotation>>;
    using ClassPatch = std::map<std::string,std::optional<ObjectClass>>;
    struct Calibration { std::optional<double> x,y; };
    struct EditRecord {
        ObjectPatch before,after;
        Calibration before_calibration,after_calibration;
        ClassPatch before_classes,after_classes;
        std::string label;
        std::size_t bytes = 0;
    };
    struct Staging;
    void apply_record(const EditRecord&, bool forward);
    void commit(Staging&, Calibration);
    void save_edit(Staging&, Calibration, std::string label);
    void trim_history();
    void refresh_spatial() const;
    std::shared_ptr<ImageServer> server_;
    std::map<std::uint64_t,Annotation> objects_;
    std::map<std::uint64_t,std::set<std::uint64_t>> children_;
    std::map<std::string,ObjectClass> classes_;
    std::map<std::string,std::size_t> class_counts_;
    std::vector<EditRecord> undo_,redo_;
    std::uint64_t next_id_ = 1;
    std::uint64_t revision_ = 0;
    std::uint64_t structure_revision_ = 0;
    std::size_t history_steps_ = 128, history_budget_ = 64*1024*1024, history_bytes_ = 0;
    std::optional<double> pixel_width_um_, pixel_height_um_;
    mutable std::shared_ptr<const SpatialIndex> spatial_;
    mutable std::map<std::uint64_t,std::optional<Rect>> spatial_dirty_;
    mutable bool spatial_rebuild_=true;
    mutable std::uint64_t spatial_builds_=0;
};
// Presentation state, bound to a document that must outlive it. Selection is
// not journaled; deletion/undo pruning never silently selects restored objects.
class ObjectSelection {
public:
    explicit ObjectSelection(const Document& document) : document_(document) {}
    std::vector<std::uint64_t> ids() const;
    std::uint64_t primary() const;
    std::size_t size() const;
    bool contains(std::uint64_t id) const;
    void replace(const std::vector<std::uint64_t>& ids,std::uint64_t primary=0);
    void toggle(std::uint64_t id);
    void clear();
    void select_all();
    void select_range(std::uint64_t anchor,std::uint64_t target,bool additive=false);
    void select_region(Rect bounds,bool additive=false);
private:
    void sync() const;
    void publish(std::set<std::uint64_t> ids,std::uint64_t primary);
    const Document& document_;
    mutable std::set<std::uint64_t> ids_;
    mutable std::uint64_t primary_=0;
    mutable std::uint64_t structure_revision_=0;
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
    std::size_t visible_objects = 0;
    std::vector<std::uint64_t> selected_ids;
    std::uint64_t primary_selection = 0;
};
enum class GuiInputKind { Move, LeftDown, LeftUp, RightDown, RightUp, Wheel, RectangleKey, UndoKey, RedoKey, DeleteKey, EllipseKey, PolygonKey, PolylineKey, PointsKey, FinishKey, SelectAllKey, EscapeKey, VertexKey, HoleKey, InsertKey, BooleanKey, SaveKey };
enum class GuiInputTarget { Canvas, Overview, ObjectList, Image };
struct GuiInput {
    int frame = 0;
    GuiInputKind kind = GuiInputKind::Move;
    GuiInputTarget target = GuiInputTarget::Canvas;
    double x = .5, y = .5, wheel = 0;
    bool ctrl = false, shift = false;
};
struct GuiOptions {
    int width = 1440, height = 900;
    DisplaySettings display;
    std::vector<GuiInput> inputs;
    std::optional<Viewport> start_view;
    std::shared_ptr<Project> project;
};
GuiReport run_gui(Document& document, int frame_limit = 0, const std::string& capture_path = "", const GuiOptions& options = {});
}

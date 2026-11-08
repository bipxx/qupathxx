#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <optional>

namespace qpx {
struct Point {
    double x=0,y=0;
    Point()=default;
    Point(double x_,double y_) : x(x_),y(y_) {}
};
struct Rect {
    double x=0,y=0,width=0,height=0;
    Rect()=default;
    Rect(double x_,double y_,double w,double h) : x(x_),y(y_),width(w),height(h) {}
    void validate() const;
    double area() const;
    bool contains(Point p) const;
};
using Ring=std::vector<Point>;
struct PolygonPart {
    Ring shell;
    std::vector<Ring> holes;
    PolygonPart(Ring shell_,std::vector<Ring> holes_={}) : shell(std::move(shell_)),holes(std::move(holes_)) {}
};
enum class RoiKind { Rectangle=0, Ellipse=1, Polygon=2, MultiPolygon=3, Polyline=4, Points=5 };
enum class BooleanOp { Union, Intersection, Difference, SymmetricDifference };
enum class RoiSamplingPolicy { GeometryCoverage=0, PolygonWinding=1 };
void validate_sampling_policy(RoiSamplingPolicy);
// Immutable shared geometry. Rectangle bounds remain available through the
// legacy Annotation.roi Python property, but this is the authoritative shape.
class Roi {
public:
    Roi()=default; // Empty value is only for a removal descriptor.
    static Roi rectangle(Rect);
    static Roi ellipse(Rect);
    static Roi polygon(Ring shell,std::vector<Ring> holes={});
    static Roi multipolygon(std::vector<PolygonPart> parts);
    static Roi polyline(Ring points);
    static Roi points(Ring points);
    RoiKind kind() const;
    Rect bounds() const;
    // Area/length/point-weighted centroid of the represented geometry.
    Point centroid() const;
    double area() const;
    double perimeter() const;
    double scaled_length(double x_scale,double y_scale) const;
    bool is_area() const;
    bool contains(Point) const;
    // Explicit sample-domain policy. Geometry hit-testing remains contains().
    // Winding changes only polygon edges; analytic/non-area shapes retain their
    // existing membership. Valid polygon shells/holes are interpreted by role.
    bool contains_sample(Point,RoiSamplingPolicy) const;
    bool covers(const Roi&) const;
    double distance(Point) const;
    Roi translated(double dx,double dy) const;
    Roi as_polygon() const;
    Roi with_vertex(std::size_t path,std::size_t vertex,Point point) const;
    Roi inserted_vertex(std::size_t path,std::size_t vertex,Point point) const;
    Roi without_vertex(std::size_t path,std::size_t vertex) const;
    Roi with_hole(std::size_t part,Ring hole) const;
    Roi without_hole(std::size_t part,std::size_t hole) const;
    std::optional<Roi> boolean_operation(const Roi&,BooleanOp) const;
    bool equals(const Roi&) const;
    std::vector<Ring> paths() const;
    std::vector<PolygonPart> polygon_parts() const;
    std::size_t payload_bytes() const;
    std::string wkt() const;
private:
    struct Data;
    std::shared_ptr<Data> data_;
    explicit Roi(std::shared_ptr<Data> data) : data_(std::move(data)) {}
    const Data& data() const;
    Roi rebuild_paths(std::vector<Ring>) const;
};
bool has_geos();
std::string geos_version();
}

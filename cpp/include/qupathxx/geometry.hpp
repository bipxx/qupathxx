#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <utility>

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
enum class RoiKind { Rectangle, Ellipse, Polygon, MultiPolygon, Polyline, Points };
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
    double area() const;
    double perimeter() const;
    bool is_area() const;
    bool contains(Point) const;
    bool covers(const Roi&) const;
    double distance(Point) const;
    Roi translated(double dx,double dy) const;
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
};
bool has_geos();
std::string geos_version();
}

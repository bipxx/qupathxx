#pragma once
#include "qupathxx/scientific_grid.hpp"
namespace qpx {
class ScientificRawWindow;
// Global geometry without whole-raster traversal/allocation admission.
class ScientificRawLattice {
    std::int64_t width_,height_;ScientificGridAxis x_,y_;
public:
    ScientificRawLattice(std::int64_t width,std::int64_t height,ScientificGridAxis x,ScientificGridAxis y):width_(width),height_(height),x_(x),y_(y) {
        if(width<1 || height<1 || width>ScientificGridAxis::coordinate_limit || height>ScientificGridAxis::coordinate_limit) throw std::invalid_argument("invalid raw lattice source dimensions");
    }
    explicit ScientificRawLattice(const ScientificGrid& g):ScientificRawLattice(g.image_width(),g.image_height(),g.x(),g.y()) {}
    auto image_width() const {return width_;}auto image_height() const {return height_;}
    const ScientificGridAxis& x() const {return x_;}const ScientificGridAxis& y() const {return y_;}
    std::uint64_t core_samples() const {return std::uint64_t(x_.samples())*std::uint64_t(y_.samples());}
    ScientificRawWindow window(std::int64_t first_x,std::int64_t first_y,std::int64_t width,std::int64_t height) const;
};
// A sample-index view of the original lattice. samples()/extent()/origin()
// retain their global meanings; no fractional origin is rounded or rebased.
class ScientificRawWindowAxis {
    ScientificGridAxis lattice_;
    std::int64_t first_,count_;
public:
    ScientificRawWindowAxis(ScientificGridAxis lattice,std::int64_t first,std::int64_t count):lattice_(lattice),first_(first),count_(count) {
        if(first<0 || count<1 || first>lattice.samples() || count>lattice.samples()-first) throw std::invalid_argument("raw sample window lies outside the global core lattice");
    }
    const ScientificGridAxis& lattice() const {return lattice_;}
    auto origin() const {return lattice_.origin();}auto extent() const {return lattice_.extent();}
    auto samples() const {return lattice_.samples();}auto context() const {return lattice_.context();}
    auto first_core_index() const {return first_;}auto core_samples() const {return count_;}
    auto first_index() const {return first_-context();}auto stop_index() const {return first_+count_+context();}
    GridFraction edge(std::int64_t index) const {
        if(index<first_index() || index>stop_index()) throw std::out_of_range("raw window edge outside captured context");return lattice_.edge(index);
    }
    GridFraction center(std::int64_t index) const {
        if(index<first_index() || index>=stop_index()) throw std::out_of_range("raw window center outside captured context");return lattice_.center(index);
    }
    GridFraction overlap(std::int64_t index,std::int64_t source_pixel) const {
        edge(index);edge(index+1);return lattice_.overlap(index,source_pixel);
    }
    std::uint64_t overlap_visits() const {
        const auto a=first_index(),b=stop_index(),period=samples()/std::gcd(extent(),samples());
        const auto integers=GridFraction(b-1,period).floor()-GridFraction(a,period).floor();
        return std::uint64_t(edge(b).ceil()-edge(a).floor()+b-a-1-integers);
    }
};
// Geometry only: global dimensions may exceed whole-raster admission limits.
// Numeric traversal and allocation are admitted for this window before IO.
class ScientificRawWindow {
    std::int64_t image_width_,image_height_;
    ScientificRawWindowAxis x_,y_;
    GridReadBounds read_,clipped_;
    std::uint64_t visits_;
public:
    ScientificRawWindow(std::int64_t image_width,std::int64_t image_height,ScientificGridAxis x,ScientificGridAxis y,
        std::int64_t first_x,std::int64_t first_y,std::int64_t width,std::int64_t height):
        image_width_(image_width),image_height_(image_height),x_(x,first_x,width),y_(y,first_y,height) {
        if(image_width<1 || image_height<1 || image_width>ScientificGridAxis::coordinate_limit || image_height>ScientificGridAxis::coordinate_limit) throw std::invalid_argument("invalid raw window source dimensions");
        const auto left=x_.edge(x_.first_index()).floor(),right=x_.edge(x_.stop_index()).ceil(),top=y_.edge(y_.first_index()).floor(),bottom=y_.edge(y_.stop_index()).ceil();
        read_={left,top,right-left,bottom-top};const auto l=std::clamp(left,std::int64_t(0),image_width),r=std::clamp(right,std::int64_t(0),image_width),t=std::clamp(top,std::int64_t(0),image_height),b=std::clamp(bottom,std::int64_t(0),image_height);clipped_={l,t,r-l,b-t};
        const auto vx=x_.overlap_visits(),vy=y_.overlap_visits();if(vx>(1ULL<<28)/vy) throw std::length_error("raw window exceeds overlap traversal limit");visits_=vx*vy;
        if(std::uint64_t(width+2*x_.context())>std::numeric_limits<std::uint32_t>::max()/std::uint64_t(height+2*y_.context())) throw std::length_error("raw window exceeds sample index range");
    }
    ScientificRawWindow(const ScientificGrid& grid,std::int64_t first_x,std::int64_t first_y,std::int64_t width,std::int64_t height):
        ScientificRawWindow(grid.image_width(),grid.image_height(),grid.x(),grid.y(),first_x,first_y,width,height) {}
    const auto& x() const {return x_;}const auto& y() const {return y_;}
    auto image_width() const {return image_width_;}auto image_height() const {return image_height_;}
    auto core_width() const {return x_.core_samples();}auto core_height() const {return y_.core_samples();}
    auto width() const {return core_width()+2*x_.context();}auto height() const {return core_height()+2*y_.context();}
    GridReadBounds read_bounds() const {return read_;}GridReadBounds clipped_read_bounds() const {return clipped_;}
    auto overlap_visits() const {return visits_;}
};
inline ScientificRawWindow ScientificRawLattice::window(std::int64_t first_x,std::int64_t first_y,std::int64_t width,std::int64_t height) const {return ScientificRawWindow(width_,height_,x_,y_,first_x,first_y,width,height);}
}

#include "nucls_geometry.hpp"
#include "nucls_csv.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace qpx::detail {
namespace {
class MomentSum {
    std::vector<double> parts_;
public:
    void add(double x) {
        if(!std::isfinite(x))throw std::overflow_error("Nonfinite centroid moment");
        std::size_t keep=0;
        for(double y:parts_){if(std::abs(x)<std::abs(y))std::swap(x,y);const auto hi=x+y,lo=y-(hi-x);if(!std::isfinite(hi))throw std::overflow_error("Centroid moment sum overflow");if(lo)parts_[keep++]=lo;x=hi;}
        parts_.resize(keep);if(x)parts_.push_back(x);
    }
    double value() const {
        if(parts_.empty())return 0;auto remaining=parts_.size();double hi=parts_[--remaining],lo=0;
        while(remaining){const auto x=hi,y=parts_[--remaining];hi=x+y;lo=y-(hi-x);if(lo)break;}
        // Sticky lower terms decide which side of a half-way rounding lies.
        if(remaining && ((lo<0 && parts_[remaining-1]<0) || (lo>0 && parts_[remaining-1]>0))){const auto correction=2*lo,candidate=hi+correction;if(candidate-hi==correction)hi=candidate;}
        return hi;
    }
};
std::vector<double> coordinates(const std::string& text) {
    std::vector<double> values;
    for(std::size_t at=0;;){const auto comma=text.find(',',at);values.push_back(nucls_number(text.substr(at,comma==std::string::npos?comma:comma-at)));if(values.size()>10000)throw std::invalid_argument("Polygon requires3..10000 paired coordinates");if(comma==std::string::npos)break;at=comma+1;}return values;
}
}
double nucls_accurate_sum(const std::vector<double>& values){MomentSum sum;for(auto value:values)sum.add(value);return sum.value();}
NuclsGeometry nucls_geometry(const std::map<std::string,std::string>& row) {
    const auto x0=nucls_number(row.at("xmin")),y0=nucls_number(row.at("ymin")),x1=nucls_number(row.at("xmax")),y1=nucls_number(row.at("ymax"));
    if(!(x1>x0) || !(y1>y0))throw std::invalid_argument("Bounds require positive width and height");
    if(row.at("type")=="rectangle")return {Roi::rectangle({x0,y0,x1-x0,y1-y0}),{x0/2+x1/2,y0/2+y1/2},4};
    if(row.at("type")!="polyline")throw std::invalid_argument("NuCLS type must be rectangle or polyline (closed polygon)");
    const auto xs=coordinates(row.at("coords_x")),ys=coordinates(row.at("coords_y"));
    if(xs.size()!=ys.size() || xs.size()<3)throw std::invalid_argument("Polygon requires3..10000 paired coordinates");
    if(*std::min_element(xs.begin(),xs.end())!=x0 || *std::max_element(xs.begin(),xs.end())!=x1 || *std::min_element(ys.begin(),ys.end())!=y0 || *std::max_element(ys.begin(),ys.end())!=y1)
        throw std::invalid_argument("Polygon bounds disagree with coordinates");
    Ring points;points.reserve(xs.size());for(std::size_t i=0;i<xs.size();++i)points.push_back({xs[i],ys[i]});auto outline=Roi::polygon(std::move(points));
    MomentSum area,xmoment,ymoment;
    for(std::size_t i=0;i<xs.size();++i){const auto next=(i+1)%xs.size();const auto ax=xs[i]-x0,ay=ys[i]-y0,bx=xs[next]-x0,by=ys[next]-y0;
        const auto cross=ax*by-bx*ay;area.add(cross);xmoment.add((ax+bx)*cross);ymoment.add((ay+by)*cross);}
    const auto area2=area.value();if(!area2 || !std::isfinite(area2))throw std::invalid_argument("Polygon has undefined centroid");
    Point center{x0+xmoment.value()/(3*area2),y0+ymoment.value()/(3*area2)};
    if(!std::isfinite(center.x) || !std::isfinite(center.y))throw std::invalid_argument("Polygon has nonfinite centroid");return {std::move(outline),center,xs.size()};
}
}

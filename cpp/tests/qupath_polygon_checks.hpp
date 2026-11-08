#include "../qupath_polygon.hpp"
#include <algorithm>
#include <limits>
void polygon_walk_checks() {
    const auto winding=[](const Ring& ring,Point p) {
        int result=0;
        for(std::size_t i=0;i<ring.size();++i) {
            const auto a=ring[i],b=ring[(i+1)%ring.size()];
            const auto cross=(b.x-a.x)*(p.y-a.y)-(p.x-a.x)*(b.y-a.y);
            if(a.y<=p.y && b.y>p.y && cross>0)++result;
            if(a.y>p.y && b.y<=p.y && cross<0)--result;
        }
        return result!=0;
    };
    const std::vector<Ring> walks={
        {{0,0},{2,0},{2,2},{0,2},{0,0},{-2,0},{-2,-2},{0,-2}},
        {{0,0},{2,0},{2,2},{0,2},{0,0},{0,-2},{-2,-2},{-2,0}},
        {{0,0},{10,0},{10,10},{0,10},{0,0},{2,4},{4,2}},
        {{0,0},{10,0},{10,10},{0,10},{0,0},{4,2},{2,4}},
        {{0,0},{2,0},{2,2},{0,2},{0,0},{2,0},{2,2},{0,2}}
    };
    std::size_t cases=0;
    for(auto walk:walks)for(int reverse=0;reverse<2;++reverse)for(double scale:{.5,1.,3.}) {
        if(reverse)std::reverse(walk.begin(),walk.end());
        Ring input;for(auto p:walk)input.push_back({100+p.x*scale,-50+p.y*scale});
        bool normalized=false;const auto roi=detail::qupath_saved_polygon(input,normalized);check(normalized);
        for(double y=-3.783;y<11;y+=.5)for(double x=-3.877;x<11;x+=.5) {
            const Point p{100+x*scale,-50+y*scale};check(roi.contains(p)==winding(input,p));
        }
        for(const auto& path:roi.paths())for(auto p:path) {
            check(std::any_of(input.begin(),input.end(),[&](auto q){return p.x==q.x && p.y==q.y;}));
        }
        ++cases;
    }
    bool normalized=true;
    const auto ordinary=detail::qupath_saved_polygon({{0,0},{2,0},{2,2},{0,2}},normalized);
    check(!normalized && ordinary.area()==4);
    for(const Ring& bad:std::vector<Ring>{
        {{0,0},{2,2},{0,2},{2,0}},
        {{0,0},{4,0},{4,4},{2,4},{2,6},{2,4},{0,4}},
        {{0,0},{4,0},{2,4},{0,0},{3,0},{3,3}},
        {{0,0},{2,0},{2,2},{0,2},{0,0},{0,2},{2,2},{2,0}},
        {{0,0},{0,0},{2,0},{2,2},{0,2}}
    })rejects([&]{detail::qupath_saved_polygon(bad,normalized);});
    Ring oversized(100002,Point{0,0});
    try {detail::qupath_saved_polygon(oversized,normalized);throw std::runtime_error("missing vertex bound");}
    catch(const std::invalid_argument& error) {check(std::string(error.what()).find("100000")!=std::string::npos);}
    Ring many;for(int i=0;i<1025;++i)for(auto p:Ring{{0,0},{2,0},{2,2},{0,2}})many.push_back(p);
    rejects([&]{detail::qupath_saved_polygon(many,normalized);});
    std::atomic<bool> cancelled{true};rejects([&]{detail::qupath_saved_polygon(walks[0],normalized,&cancelled);});
    check(!normalized);
    std::cout<<"Saved PolygonROI vertex-touch normalization: "<<cases<<" exact winding/coordinate cases; crossings, spikes, cancellation and limits refused\n";
}

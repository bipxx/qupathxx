#include "qupathxx/view_tiles.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace qpx;
void check(bool value) { if(!value) throw std::runtime_error("view tile check failed"); }
int main() {
    try {
        ImageMetadata m; m.width=1024; m.height=768; m.levels={{1024,768,1},{512,384,2},{256,192,4}};
        Viewport v; auto p=plan_view_tiles(m,v,512,384);
        check(p.detail_level==0 && p.tiles.size()==5 && p.tiles.front().fallback);
        for(const auto& t:p.tiles) { validate_region(m,t.request); check(t.bounds.width>0 && t.bounds.height>0); }
        v.zoom=.5; check(plan_view_tiles(m,v,512,384).detail_level==1);
        check(plan_view_tiles(m,v,512,384,2).detail_level==0);
        v.origin={2000,2000}; check(plan_view_tiles(m,v,512,384).tiles.empty());
        m.width=1000000000000; m.height=1000000000000; m.levels={{m.width,m.height,1}};
        v.fit(m.width,m.height,1000,1000);
        auto before=std::chrono::steady_clock::now();
        for(int i=0;i<100;++i) { auto bounded=plan_view_tiles(m,v,1000,1000); check(bounded.tiles.size()<=192 && bounded.truncated); }
        check(std::chrono::steady_clock::now()-before<std::chrono::seconds(2));
        m.width=1000; m.height=777; m.levels={{1000,777,1},{333,259,3.001}};
        v.fit(m.width,m.height,500,400);
        p=plan_view_tiles(m,v,500,400);
        for(const auto& tile:p.tiles) check(tile.bounds.x+tile.bounds.width<=1000 && tile.bounds.y+tile.bounds.height<=777);
        std::cout<<"Tile selection, DPI, bounds and huge-image request cap passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}

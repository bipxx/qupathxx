#include "qupathxx/core.hpp"
#include <iostream>
#include <stdexcept>
using namespace qpx;
void check(bool value) { if(!value) throw std::runtime_error("spatial check failed"); }
int main() {
    try {
        Document d(Image::demo());
        auto id=d.add_rectangle({0,0,10,10});
        check(d.query_region({2,2,0,0})==std::vector<std::uint64_t>{id});
        auto builds=d.spatial_stats().builds;
        d.rename_annotation(id,"Renamed"); check(d.pick({2,2})==std::vector<std::uint64_t>{id});
        check(d.spatial_stats().builds==builds && !d.spatial_stats().dirty_entries);
        d.update_rectangle(id,{100,100,10,10});
        check(d.pick({2,2}).empty() && d.pick({102,102})==std::vector<std::uint64_t>{id});
        check(d.undo() && d.pick({2,2})==std::vector<std::uint64_t>{id});
        d.remove_annotation(id); check(d.pick({2,2}).empty());
        check(d.undo() && d.pick({2,2})==std::vector<std::uint64_t>{id});
        std::cout<<"Spatial queries, delta updates, picking and undo passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}

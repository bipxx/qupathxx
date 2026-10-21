#include "../cell_numeric.hpp"
#include "qupathxx/cells.hpp"
#include <iostream>
using namespace qpx;
void check(bool v) {if(!v) throw std::runtime_error("cell check failed");}
class AlphaDomain final:public ImageServer {
    ImageMetadata info;
public:
    std::uint8_t alpha=0;
    AlphaDomain() {info.width=16;info.height=16;info.levels={{16,16,1}};}
    const ImageMetadata& metadata() const override {return info;}
    PixelBuffer read_region(const RegionRequest& r) const override {
        PixelBuffer out(r.width,r.height);
        for(std::int64_t y=0;y<r.height;++y) for(std::int64_t x=0;x<r.width;++x) {
            const auto i=static_cast<std::size_t>((y*r.width+x)*4);
            out.writable_bytes()[i+3]=(r.x+x==8 && r.y+y==8)?alpha:255;
        }
        return out;
    }
    void close() override {}
    bool closed() const override {return false;}
};
int main() {
 try {
    std::vector<double> tiny_sigma_values{1,2,3,4};
    cell_detail::gaussian(tiny_sigma_values,std::vector<std::uint8_t>(4,1),2,2,1e-300,nullptr);
    check(tiny_sigma_values==std::vector<double>({1,2,3,4}));
    for(unsigned bits=0;bits<512;++bits) {
        std::vector<double> values(9);for(int i=0;i<9;++i) values[i]=bits&(1u<<i)?cell_detail::infinity:0;
        cell_detail::distance(values,3,3,true,nullptr,nullptr);
        for(int y=0;y<3;++y) for(int x=0;x<3;++x) {
            double best=1e30;
            for(int yy=-1;yy<=3;++yy) for(int xx=-1;xx<=3;++xx) {
                if(xx>=0 && yy>=0 && xx<3 && yy<3 && (bits&(1u<<(yy*3+xx)))) continue;
                best=std::min(best,double((x-xx)*(x-xx)+(y-yy)*(y-yy)));
            }
            check(values[y*3+x]==best);
        }
        std::vector<std::uint32_t> nearest;for(int i=0;i<9;++i) values[i]=bits&(1u<<i)?0:cell_detail::infinity;
        cell_detail::distance(values,3,3,false,&nearest,nullptr);
        for(int y=0;y<3;++y) for(int x=0;x<3;++x) {
            double best=1e30;std::uint32_t index=std::numeric_limits<std::uint32_t>::max();
            for(int i=0;i<9;++i) if(bits&(1u<<i)) {const auto d=double((x-i%3)*(x-i%3)+(y-i/3)*(y-i/3));if(d<best) {best=d;index=i;}}
            check(values[y*3+x]==best && nearest[y*3+x]==index);
        }
    }
    for(auto alpha:{0,128}) {
        auto server=std::make_shared<AlphaDomain>();server->alpha=static_cast<std::uint8_t>(alpha);
        Document doc(server);const auto id=doc.add_rectangle({0,0,16,16});CellParameters p;
        p.sigma=1;p.min_nucleus_pixels=1;p.min_seed_distance=0;
        const auto result=detect_cells(CellRequest(doc,id,StainTransform({{1,0,0},{0,1,0}}),p));
        check(result->nuclei()->samples()==255 && result->cells()->samples()==255);
        check(!result->nuclei()->labels()[8*16+8] && !result->cells()->labels()[8*16+8]);
        check(result->nuclei()->foreground_pixels()==255 && result->cells()->foreground_pixels()==255);
        std::atomic<bool> cancelled{true};bool stopped=false;
        try {detect_cells(CellRequest(doc,id,StainTransform({{1,0,0},{0,1,0}}),p),&cancelled);} catch(const std::runtime_error&) {stopped=true;}
        check(stopped);
    }
    std::vector<std::uint8_t> rgb(64*64*3,255);
    for(int y=0;y<64;++y) for(int x=0;x<64;++x) if((x-25)*(x-25)+(y-32)*(y-32)<=100 || (x-39)*(x-39)+(y-32)*(y-32)<=100) for(int c=0;c<3;++c) rgb[(y*64+x)*3+c]=0;
    Document doc(Image(64,64,rgb));const auto obj=doc.add_rectangle({0,0,64,64});CellParameters p;p.sigma=0;p.min_seed_distance=4;
    CellJob job;check(job.request(CellRequest(doc,obj,StainTransform({{1,0,0},{0,1,0}}),p)));check(job.wait(doc,obj).state=="ready");
    const auto result=job.result();check(result->features().size()==2 && result->markers().size()==2);
    for(const auto& f:result->features()) check(f.nucleus_pixels>100 && f.cell_pixels>f.nucleus_pixels && f.nucleus_rgb[0]==0);
    if(has_geos()) {const auto ids=job.keep_result(doc,obj);check(ids.size()==2 && doc.object_count()==3 && doc.annotation(ids[0]).kind==ObjectKind::Cell && doc.annotation(ids[0]).nucleus.has_value());check(doc.undo() && doc.object_count()==1);check(doc.redo() && doc.object_count()==3);}
    else {bool failed=false;try {job.keep_result(doc,obj);} catch(const std::runtime_error&) {failed=true;}check(failed && doc.object_count()==1);}
    check(job.request(CellRequest(doc,obj,StainTransform({{1,0,0},{0,1,0}}),p)));job.cancel();check(job.wait(doc,obj).state=="cancelled" && !job.result());
    check(job.request(CellRequest(doc,obj,StainTransform({{1,0,0},{0,1,0}}),p)));doc.rename_annotation(obj,"Changed");check(job.wait(doc,obj).state=="stale" && !job.result());
    std::cout<<"Cell distance/nearest-site references, touching nuclei and lifecycle passed\n";
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}

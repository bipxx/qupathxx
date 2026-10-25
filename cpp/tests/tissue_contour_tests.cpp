#include "qupathxx/tissue.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
void check_scientific_raster();
void check(bool value) {if(!value) throw std::runtime_error("tissue contour check failed");}
template<class Exception,class Action> void rejects(Action action) {try {action();} catch(const Exception&) {return;}throw std::logic_error("missing contour rejection");}
int main() {
    try {
        check_scientific_raster();
        using namespace qpx;StainTransform transform({{1,0,0},{0,1,0}});
        if(!has_geos()) {
            Document doc(Image(3,3,std::vector<std::uint8_t>(27,0)));auto obj=doc.add_rectangle({0,0,3,3});
            auto mask=detect_tissue(TissueRequest(doc,obj,transform));
            rejects<std::runtime_error>([&]{trace_tissue(*mask);});
            TissueJob job;check(job.request(TissueRequest(doc,obj,transform)));check(job.wait(doc,obj).state=="ready");
            rejects<std::runtime_error>([&]{job.request_contours();});
            std::cout<<"contour backend-disabled checks passed\n";return 0;
        }
        for(unsigned bits=0;bits<512;++bits) for(auto connectivity:{4,8}) {
            std::vector<std::uint8_t> rgb;for(int i=0;i<9;++i) for(int c=0;c<3;++c) rgb.push_back(bits&(1u<<i)?0:255);
            Document doc(Image(3,3,rgb));const auto obj=doc.add_rectangle({0,0,3,3});TissueParameters p;p.connectivity=connectivity;
            const auto mask=detect_tissue(TissueRequest(doc,obj,transform,p));const auto contours=trace_tissue(*mask);
            check(contours->components().size()==mask->components().size());double area=0;
            for(const auto& contour:contours->components()) {
                check(contour.geometry.area()==contour.pixels);area+=contour.geometry.area();
                for(int y=0;y<3;++y) for(int x=0;x<3;++x) check(contour.geometry.contains({x+.5,y+.5})==(mask->labels()[y*3+x]==contour.label));
            }
            check(area==mask->foreground_pixels() && contours->retained_bytes()<=contours->estimated_work_bytes());
        }
        Document solid(Image(12,12,std::vector<std::uint8_t>(12*12*3,0)));const auto source=solid.add_rectangle({1,1,10,10});
        TissueJob job;check(job.request(TissueRequest(solid,source,transform)));check(job.wait(solid,source).state=="ready");
        check(job.request_contours());check(job.wait(solid,source).state=="ready");
        const auto result=job.contours();check(result->edge_count()==4 && result->vertex_count()==4 && result->components()[0].geometry.area()==100);
        const auto before=solid.revision();rejects<std::invalid_argument>([&]{job.keep_contours(solid,source,std::string(4096,'x'),"Tissue");});
        check(solid.revision()==before && solid.object_count()==1 && !job.contours_published() && solid.classes().empty());
        const auto ids=job.keep_contours(solid,source,"Tissue","Tissue");check(ids.size()==1 && job.contours_published());
        check(solid.annotation(ids[0]).parent_id==source && solid.children(source)==ids && solid.undo_label()=="Add tissue annotations");
        rejects<std::logic_error>([&]{job.keep_contours(solid,source);});
        check(solid.undo() && solid.object_count()==1 && solid.classes().empty());
        check(solid.redo() && solid.annotation(ids[0]).geometry.area()==100 && solid.class_count("Tissue")==1);
        TissueContourOptions small;small.max_edges=3;
        check(job.request_contours(small));check(job.wait(solid,source).state=="failed" && job.result() && !job.contours());
        small=TissueContourOptions{};small.max_vertices=3;rejects<std::length_error>([&]{trace_tissue(*job.result(),small);});
        small=TissueContourOptions{};small.work_bytes=1;rejects<std::length_error>([&]{trace_tissue(*job.result(),small);});
        check(job.request_contours());job.cancel();check(job.wait(solid,source).state=="cancelled" && !job.result() && !job.contours());
        check(job.request(TissueRequest(solid,source,transform)));check(job.wait(solid,source).state=="ready");
        check(job.request_contours());solid.rename_annotation(source,"Changed");check(job.wait(solid,source).state=="stale" && !job.contours());
        std::atomic<bool> stop{true};auto mask=detect_tissue(TissueRequest(solid,source,transform));
        rejects<std::runtime_error>([&]{trace_tissue(*mask,{},&stop);});
        const auto ellipse=solid.add_roi(Roi::ellipse({1.2,1.3,6.1,5.4}));
        check(solid.annotation(ellipse).geometry.covers(solid.annotation(ellipse).geometry.as_polygon()));
        check(job.request(TissueRequest(solid,ellipse,transform)));check(job.wait(solid,ellipse).state=="ready");
        check(job.request_contours());check(job.wait(solid,ellipse).state=="ready");check(!job.keep_contours(solid,ellipse).empty());
        std::cout<<"tissue contour topology, publication and lifecycle passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}

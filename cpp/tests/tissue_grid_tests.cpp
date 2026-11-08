#include "qupathxx/tissue.hpp"
#include "qupathxx/input_digest.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace qpx;
void check(bool v,const char* message) {if(!v) throw std::runtime_error(message);}
template<class F> void rejects(F f) {bool rejected=false;try {f();}catch(const std::exception&) {rejected=true;}check(rejected,"expected rejection");}
class Pattern final:public ImageServer {
public:
    ImageMetadata m;mutable unsigned reads=0;bool shut=false;
    Pattern() {m.width=9;m.height=7;m.levels={{9,7,1}};}
    static int gray(int x,int y) {return (x*47+y*31+17)%256;}
    static bool opaque(int x,int y) {return !(x==3 && y==2);}
    const ImageMetadata& metadata() const override {return m;}
    PixelBuffer read_region(const RegionRequest& r) const override {
        validate_region(m,r);++reads;PixelBuffer b(r.width,r.height);
        for(int y=0;y<r.height;++y) for(int x=0;x<r.width;++x) {
            const int gx=int(r.x)+x,gy=int(r.y)+y;const auto at=std::size_t(y*r.width+x)*4;
            for(int c=0;c<3;++c) b.writable_bytes()[at+c]=std::uint8_t(gray(gx,gy));
            b.writable_bytes()[at+3]=opaque(gx,gy)?255:128;
        }
        return b;
    }
    void close() override {shut=true;}
    bool closed() const override {return shut;}
};
// Independent direct footprint integration and full 2-D Gaussian/median oracle.
// It uses world interval intersections, not the production sampler or filters.
void compare(Document& doc,std::uint64_t id,const ScientificGrid& g,TissueParameters p,const StainTransform& t) {
    const auto request=TissueRequest(doc,id,t,g,p);const auto result=detect_tissue(request);
    const int w=int(g.width()),h=int(g.height());const auto n=w*h;
    std::vector<double> score(n);std::vector<bool> valid(n),core(n),mask(n);
    const auto& ax=g.x();const auto& ay=g.y();
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
        const auto i=y*w+x;const int gx=x-int(ax.context()),gy=y-int(ay.context());
        const long double l=ax.origin()+static_cast<long double>(gx)*ax.extent()/ax.samples(),r=ax.origin()+static_cast<long double>(gx+1)*ax.extent()/ax.samples();
        const long double top=ay.origin()+static_cast<long double>(gy)*ay.extent()/ay.samples(),bottom=ay.origin()+static_cast<long double>(gy+1)*ay.extent()/ay.samples();
        if(l<0 || top<0 || r>9 || bottom>7) continue;
        long double total=0,weighted=0;bool supported=true;
        for(int yy=int(std::floor(top));yy<int(std::ceil(bottom));++yy) for(int xx=int(std::floor(l));xx<int(std::ceil(r));++xx) {
            const auto area=std::max(0.L,std::min(r,(long double)xx+1)-std::max(l,(long double)xx))*
                std::max(0.L,std::min(bottom,(long double)yy+1)-std::max(top,(long double)yy));
            if(area==0) continue;
            supported=supported && Pattern::opaque(xx,yy);total+=area;weighted+=area*Pattern::gray(xx,yy);
        }
        if(!supported) continue;
        valid[i]=true;
        const auto rgb=double(weighted/total);
        score[i]=3*std::max(0.,-std::log10(std::max(1.,rgb)/255.));
        core[i]=gx>=0 && gy>=0 && gx<ax.samples() && gy<ay.samples() &&
            request.roi().contains_sample({double((l+r)/2),double((top+bottom)/2)},p.sampling_policy);
    }
    if(p.smoothing_sigma>0) {
        auto before=score;const int radius=int(std::ceil(3*p.smoothing_sigma));
        for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
            double sum=0,weight=0;
            for(int dy=-radius;dy<=radius;++dy) for(int dx=-radius;dx<=radius;++dx) {
                const auto xx=x+dx,yy=y+dy;if(xx<0 || yy<0 || xx>=w || yy>=h || !valid[yy*w+xx]) continue;
                const auto k=std::exp(-.5*(double(dx)*dx+double(dy)*dy)/(p.smoothing_sigma*p.smoothing_sigma));
                sum+=k*before[yy*w+xx];weight+=k;
            }
            score[y*w+x]=weight?sum/weight:0;
        }
    }
    std::uint64_t samples=0,threshold=0,cleaned=0;
    for(int i=0;i<n;++i) {samples+=core[i];mask[i]=valid[i] && score[i]>=p.threshold;threshold+=core[i] && mask[i];}
    if(p.median_cleanup) {
        auto before=mask;
        for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
            int votes=0;for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx)
                votes+=before[std::clamp(y+dy,0,h-1)*w+std::clamp(x+dx,0,w-1)];
            mask[y*w+x]=valid[y*w+x] && votes>=5;
        }
    }
    auto square=[&](int radius,bool dilate) {
        const auto before=mask;
        for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
            bool v=!dilate;
            for(int dy=-radius;dy<=radius;++dy) for(int dx=-radius;dx<=radius;++dx) {
                const int xx=x+dx,yy=y+dy;
                const bool bit=xx>=0 && yy>=0 && xx<w && yy<h && before[yy*w+xx];
                v=dilate?(v || bit):(v && bit);
            }
            mask[y*w+x]=valid[y*w+x] && v;
        }
    };
    if(p.open_radius) {square(p.open_radius,false);square(p.open_radius,true);}
    if(p.close_radius) {square(p.close_radius,true);square(p.close_radius,false);}
    std::uint64_t filled=0;
    if(p.fill_holes) {
        std::vector<bool> visited(n);
        for(int seed=0;seed<n;++seed) {
            if(visited[seed] || mask[seed] || !valid[seed]) continue;
            std::vector<int> region{seed};visited[seed]=true;bool exterior=false;
            for(std::size_t at=0;at<region.size();++at) {
                const int x=region[at]%w,y=region[at]/w;
                for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
                    if((!dx && !dy) || (p.connectivity==8 && dx && dy)) continue;
                    const int xx=x+dx,yy=y+dy;
                    if(xx<0 || yy<0 || xx>=w || yy>=h) {exterior=true;continue;}
                    const int j=yy*w+xx;if(!valid[j]) {exterior=true;continue;}
                    if(!mask[j] && !visited[j]) {visited[j]=true;region.push_back(j);}
                }
            }
            if(!exterior && (!p.max_hole_pixels || region.size()<=p.max_hole_pixels))
                for(auto i:region) {mask[i]=true;filled+=core[i];}
        }
    }
    check(result->filled_pixels()==filled,"core-only filled pixels");
    for(int i=0;i<n;++i) {
        const bool expected=core[i] && mask[i];cleaned+=expected;
        check(bool(result->labels()[i])==expected,"grid mask differs from independent integration/filter oracle");
    }
    check(result->samples()==samples && result->threshold_pixels()==threshold && result->cleaned_pixels()==cleaned && result->foreground_pixels()==cleaned,"core counters");
    check(result->scientific_grid() && std::string(result->input_format())=="qpx.grid-opaque-rgb-area/1","grid identity");
    check(result->downsample_x()==double(ax.extent())/ax.samples() && result->downsample_y()==double(ay.extent())/ay.samples(),"axis spacing");
    check(input_sha256(request)==result->input_sha256(),"grid digest-only route");
    rejects([&]{result->downsample();});rejects([&]{request.level_bounds();});
}
void check_tissue_grid_archives();
int main() {
 try {
    auto server=std::make_shared<Pattern>();Document doc(server);const auto id=doc.add_rectangle({1,1,7,5});
    StainTransform transform({{1,0,0},{0,1,0}});TissueParameters p;p.threshold=2;
    for(auto context:{0,1,2}) for(auto sigma:{0.,.8}) for(auto median:{false,true}) for(auto threshold:{.5,1.,1.5}) for(int cleanup=0;cleanup<4;++cleanup) {
        p.smoothing_sigma=sigma;p.median_cleanup=median;p.threshold=threshold;
        p.open_radius=cleanup==1?1:0;p.close_radius=cleanup==2?1:0;p.fill_holes=cleanup==3;
        compare(doc,id,ScientificGrid(9,7,{1,7,5,context},{1,5,3,context}),p,transform);
    }
    p.smoothing_sigma=0;p.median_cleanup=false;p.open_radius=0;p.close_radius=0;p.fill_holes=false;p.threshold=1;
    // Smaller ROI excludes supported context before output, not before filters.
    const auto unsupported=doc.add_rectangle({2.2,1.3,2.6,3.1});
    rejects([&]{detect_tissue(TissueRequest(doc,unsupported,transform,ScientificGrid(9,7,{1,7,5,2},{1,5,3,2}),p));});
    const auto small=doc.add_rectangle({1.2,1.3,5,3.1});
    p.smoothing_sigma=.8;compare(doc,small,ScientificGrid(9,7,{1,7,5,2},{1,5,3,2}),p,transform);
    p.smoothing_sigma=0;
    TissueGridSampling sampling;sampling.samples_x=5;sampling.samples_y=3;sampling.context_x=1;sampling.context_y=2;
    const auto factory=TissueRequest::for_grid_roi(doc,id,transform,sampling,p);
    check(factory.scientific_grid()->x().origin()==1 && factory.scientific_grid()->y().context()==2,"ROI factory descriptor");
    auto exact=p;exact.work_bytes=factory.estimated_work_bytes();
    check(TissueRequest(doc,id,transform,*factory.scientific_grid(),exact).estimated_work_bytes()==exact.work_bytes,"exact admitted grid budget");
    --exact.work_bytes;rejects([&]{TissueRequest(doc,id,transform,*factory.scientific_grid(),exact);});
    const auto before=server->reads;
    auto low=p;low.work_bytes=1;rejects([&]{TissueRequest(doc,id,transform,*factory.scientific_grid(),low);});
    auto level=p;level.level=1;rejects([&]{TissueRequest(doc,id,transform,*factory.scientific_grid(),level);});
    rejects([&]{TissueRequest(doc,id,transform,ScientificGrid(8,7,{1,6,5},{1,5,3}),p);});
    check(server->reads==before,"admission must precede IO");
    auto bad=sampling;bad.samples_x=0;rejects([&]{TissueRequest::for_grid_roi(doc,id,transform,bad,p);});
    rejects([&]{TissueRequest::for_grid_roi(doc,id,transform,sampling,p,true);});
    std::atomic<bool> cancelled{true};rejects([&]{detect_tissue(factory,&cancelled);});
    check(server->reads==before,"pre-cancel must precede IO");
    rejects([&]{TissueRequest::with_grid_component_area_limits(doc,id,transform,*factory.scientific_grid(),1.,{},p);});
    doc.set_calibration(.5,.25);const auto calibrated=TissueRequest::with_grid_component_area_limits(doc,id,transform,*factory.scientific_grid(),.6,.9,p);
    check(calibrated.parameters().min_pixels==3 && calibrated.parameters().max_pixels==3,"anisotropic physical count limits");
    check(calibrated.matches(doc,id),"captured calibration matches");doc.set_calibration(1,.25);
    check(!calibrated.matches(doc,id) && !factory.matches(doc,id),"grid calibration change is stale");
    TissueJob job;const auto current=TissueRequest(doc,id,transform,*factory.scientific_grid(),p);
    check(job.request(current),"start grid job");doc.set_calibration(1,.5);
    check(job.wait(doc,id).state=="stale" && !job.result(),"stale grid publication rejected");
    // Exact geometry for one connected core, excluding context from components.
    p.threshold=-1;const auto all=TissueRequest(doc,id,transform,ScientificGrid(9,7,{5,3,3,1},{1,5,2,0}),p);
    const auto result=detect_tissue(all);check(result->components().size()==1 && result->components()[0].pixels==6,"core component count");
    const auto& c=result->components()[0];check(c.bounds.x==5 && c.bounds.y==1 && c.bounds.width==3 && c.bounds.height==5 && c.centroid.x==6.5 && c.centroid.y==3.5,"exact component geometry");
    check_tissue_grid_archives();
    std::cout<<"tissue grid native checks passed\n";return 0;
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}

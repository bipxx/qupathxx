#include "qupathxx/channel_source.hpp"
#include "qupathxx/source_identity.hpp"
#include "qupathxx/workflow.hpp"
#include "quantitative_tiff_fixture.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace qpx;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class E=std::exception,class F>void rejects(F action){try{action();}catch(const E&){return;}throw std::runtime_error("Expected channel-source refusal");}
void expect_close(double a,double b){check(std::isfinite(a) && std::abs(a-b)<=1e-12*std::max(1.0,std::abs(b)),"Corrected value differs from analytical expectation");}
class Reader final:public ImageServer {
public:
    RawImageMetadata raw;ImageMetadata display;int role=0;mutable unsigned reads=0;
    std::shared_ptr<const ReaderSource> observed=make_reader_source("analytical-channels","","memory");
    bool is_closed=false,wrong_shape=false,io_error=false;mutable bool change_during_read=false;std::atomic<bool>* cancel_on_read=nullptr;
    Reader(int r):role(r){raw.width=7;raw.height=5;raw.channels=3;raw.z_planes=raw.timepoints=2;raw.sample_type=SampleType::Float64;raw.levels={{7,5,1}};raw.channel_names={"DNA","A","B"};raw.pixel_width_um=.5;raw.pixel_height_um=1;display.width=7;display.height=5;display.levels=raw.levels;}
    const ImageMetadata& metadata()const override{return display;}
    std::string reader_id()const override{return "analytical-channels";}
    std::shared_ptr<const ReaderSource> reader_source()const override{return observed;}
    std::shared_ptr<const RawImageMetadata> raw_metadata(const std::atomic<bool>*)const override{return std::make_shared<const RawImageMetadata>(raw);}
    PixelBuffer read_region(const RegionRequest&)const override{throw std::runtime_error("RGB preview must never supply quantitative correction");}
    RawPixelBuffer read_raw(const RawRegionRequest& r,const std::atomic<bool>* cancelled)const override{
        ++reads;workflow_checkpoint(cancelled);if(io_error)throw std::runtime_error("Independent decoder failure");
        RawPixelBuffer out(r.region.width+(wrong_shape?1:0),r.region.height,r.channels,SampleType::Float64);auto& values=std::get<std::vector<double>>(out.writable_storage());
        for(std::int64_t y=0;y<r.region.height;++y)for(std::int64_t x=0;x<r.region.width;++x){const auto px=x+r.region.x,py=y+r.region.y;const auto p=std::size_t(y*r.region.width+x);
            out.writable_valid()[p]=px>=0 && py>=0 && px<7 && py<5 && !(role==1 && px==0 && py==0);
            for(std::size_t c=0;c<r.channels;++c){const auto original=c+r.first_channel;const double dark=10+original+r.z+r.t,illumination=2+px+2*py+original+r.z+3*r.t;
                const double intensity=double((px-2)*20+py)+double(original)+3*r.z+5*r.t;const double scale=original==0?10:original==1?20:40;
                values[p*r.channels+c]=role==1?dark:role==2?dark+illumination:dark+intensity*illumination/scale;}
        }
        if(cancel_on_read)cancel_on_read->store(true);if(change_during_read){change_during_read=false;const_cast<Reader*>(this)->raw.channel_names[0]="Changed";}return out;
    }
    void close()override{is_closed=true;}bool closed()const override{return is_closed;}
};
RawPixelBuffer block(std::initializer_list<double> values){RawPixelBuffer result(std::int64_t(values.size()),1,1,SampleType::Float64);std::get<std::vector<double>>(result.writable_storage())=values;std::fill(result.writable_valid().begin(),result.writable_valid().end(),1);return result;}
void kernel(){
    auto s=block({10,0,-5,10,10,10,10,10}),d=block({0,0,0,2,2,2,NAN,2}),f=block({10,10,10,2,1,2.5,10,INFINITY});DarkFlatOptions options{{2},.5};
    auto out=correct_dark_flat(s,d,f,options);const auto& v=std::get<std::vector<double>>(out.storage());expect_close(v[0],2);expect_close(v[1],0);expect_close(v[2],-1);
    for(std::size_t i=3;i<v.size();++i)check(std::isnan(v[i]) && out.valid()[i],"Bad channel numerics stay NaN on a valid sample lattice");
    options.minimum_denominator=.49;out=correct_dark_flat(s,d,f,options);expect_close(std::get<std::vector<double>>(out.storage())[5],32);
    d.writable_valid()[0]=0;out=correct_dark_flat(s,d,f,options);check(!out.valid()[0],"Missing field pixel uses intersection validity");
    options.scales[0]=0;rejects([&]{correct_dark_flat(s,d,f,options);});options.scales[0]=2;options.minimum_denominator=NAN;rejects([&]{options.sha256();});options.minimum_denominator=0;
    auto short_field=block({1});rejects<WorkflowInputChanged>([&]{correct_dark_flat(s,short_field,f,options);});
    s.writable_storage()=std::vector<float>(8);rejects<WorkflowInputChanged>([&]{correct_dark_flat(s,d,f,options);});
    RawPixelBuffer large(1,1,1,SampleType::UInt64);large.writable_valid()[0]=1;auto& u=std::get<std::vector<std::uint64_t>>(large.writable_storage());u[0]=(std::uint64_t(1)<<53)+1;auto zero=block({0}),one=block({1});
    rejects<std::overflow_error>([&]{correct_dark_flat(large,zero,one,options);});u[0]=(std::uint64_t(1)<<53);out=correct_dark_flat(large,zero,one,options);expect_close(std::get<std::vector<double>>(out.storage())[0],18014398509481984.0);
    std::atomic<bool> cancelled{true};rejects<WorkflowCancelled>([&]{correct_dark_flat(one,zero,one,options,0,1024,&cancelled);});
}
void sources(){
    auto sr=std::make_shared<Reader>(0),dr=std::make_shared<Reader>(1),fr=std::make_shared<Reader>(2);
    auto s=original_channel_source(sr),d=original_channel_source(dr),f=original_channel_source(fr);DarkFlatOptions options{{10,20,40},0};auto source=dark_flat_channel_source(s,d,f,options);
    check(!sr->tiff_view() && s->domain()=="original-channel-samples" && source->domain()!=s->domain(),"Raw capability and derived domain do not require TIFF display APIs");
    check(source->sha256()==dark_flat_channel_source(original_channel_source(sr),d,f,options)->sha256(),"Re-capture same observed sources/recipe has exact identity");
    auto changed=options;changed.scales[0]=11;check(source->sha256()!=dark_flat_channel_source(s,d,f,changed)->sha256(),"Normalization changes derived identity");
    rejects([&]{dark_flat_channel_source(source,d,f,options);});
    RawRegionRequest r{{0,0,0,7,5},0,3,1,1};const auto work=source->read_work_bytes(r);auto count=sr->reads+dr->reads+fr->reads;
    rejects<WorkflowCaptureLimit>([&]{source->read(r,work-1);});check(sr->reads+dr->reads+fr->reads==count,"Work admission precedes every decoder read");
    auto full=source->read(r,work);const auto& values=std::get<std::vector<double>>(full.storage());
    for(int y=0;y<5;++y)for(int x=0;x<7;++x){const auto p=std::size_t(y*7+x);check(bool(full.valid()[p])==!(x==0 && y==0),"Analytical field validity");if(full.valid()[p])for(int c=0;c<3;++c)expect_close(values[p*3+c],(x-2)*20+y+c+8);}
    for(int y=0;y<5;y+=2)for(int x=0;x<7;x+=3){auto tile=r;tile.region={x,y,0,std::min(3,7-x),std::min(2,5-y)};const auto data=source->read(tile);const auto& v=std::get<std::vector<double>>(data.storage());for(int dy=0;dy<tile.region.height;++dy)for(int dx=0;dx<tile.region.width;++dx){const auto local=std::size_t(dy*tile.region.width+dx),global=std::size_t((y+dy)*7+x+dx);check(data.valid()[local]==full.valid()[global],"Tile validity seam equality");for(int c=0;c<3;++c)check(v[local*3+c]==values[global*3+c],"Tile values exactly equal whole-region correction");}}
    auto selected=r;selected.first_channel=1;selected.channels=1;auto part=source->read(selected);const auto& pv=std::get<std::vector<double>>(part.storage());for(std::size_t p=0;p<pv.size();++p)check(pv[p]==values[p*3+1],"Original channel selection uses global recipe scale index");
    auto invalid=r;invalid.region.level=1;rejects([&]{source->read(invalid);});
    std::atomic<bool> cancel{true};rejects<WorkflowCancelled>([&]{source->read(r,work,&cancel);});cancel.store(false);sr->cancel_on_read=&cancel;rejects<WorkflowCancelled>([&]{source->read(r,work,&cancel);});sr->cancel_on_read=nullptr;
    sr->wrong_shape=true;rejects<WorkflowInputChanged>([&]{source->read(r);});sr->wrong_shape=false;
    sr->io_error=true;rejects([&]{source->read(r);});sr->io_error=false;
    sr->change_during_read=true;rejects<WorkflowInputChanged>([&]{source->read(r);});sr->raw.channel_names[0]="DNA";
    dr->raw.pixel_width_um=2;rejects<WorkflowInputChanged>([&]{source->read(r);});auto mismatch=original_channel_source(dr);rejects([&]{dark_flat_channel_source(s,mismatch,f,options);});dr->raw.pixel_width_um=.5;
    fr->observed=make_reader_source("analytical-channels","","memory");rejects<WorkflowInputChanged>([&]{source->read(r);});
    sr->close();rejects<WorkflowInputChanged>([&]{s->read(r);});
}
void file_source(){if(!has_tiff())return;namespace fs=std::filesystem;const auto root=fs::current_path()/("channel-source-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root);auto path=root/"native.tif";qpx_test::raw_pixel_tiff_fixture(path,15,9);
    auto server=open_tiff(path.u8string());auto source=original_channel_source(server);auto reopened=original_channel_source(open_tiff(path.u8string()));check(source->sha256()==reopened->sha256(),"File source identity survives decoder reopen");
    RawRegionRequest r{{0,0,0,15,9},0,2};auto raw=source->read(r);auto reference=server->read_raw(r);check(raw.storage()==reference.storage() && raw.valid()==reference.valid(),"Adapter preserves existing TIFF bytes and validity");
    {std::ofstream out(path,std::ios::binary|std::ios::app);out.put('x');check(bool(out),"Change local synthetic fixture");}rejects([&]{source->read(r);});
}
int main(){try{kernel();sources();file_source();std::cout<<"Native channel capability, analytical correction, seams, channel/plane identity, budgets, precision, source guards and cancellation pass\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

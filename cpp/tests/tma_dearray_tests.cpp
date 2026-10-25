#include "qupathxx/tma_dearray.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace qpx;
namespace {
void checked(bool ok,int line) {if(!ok) throw std::runtime_error("TMA dearray check at line "+std::to_string(line));}
#define check(...) checked((__VA_ARGS__),__LINE__)
template<class F> void rejects(F f,const std::string& message) {
    try {f();} catch(const std::exception& e) {check(std::string(e.what()).find(message)!=std::string::npos);return;}
    throw std::runtime_error("expected rejection: "+message);
}
PixelBuffer mask(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);std::string magic;int w=0,h=0,maximum=0;
    input>>magic>>w>>h>>maximum;check(input && magic=="P5" && w==480 && h==352 && maximum==255);check(input.get()=='\n');
    PixelBuffer image(w,h);auto& bytes=image.writable_bytes();
    for(std::size_t i=0;i<bytes.size()/4;++i) {
        const auto value=input.get();check(value==0 || value==255);
        bytes[i*4]=bytes[i*4+1]=bytes[i*4+2]=static_cast<std::uint8_t>(255-value);bytes[i*4+3]=255;
    }
    check(input.get()==EOF);return image;
}
TmaDearrayOptions options() {TmaDearrayOptions o;o.core_width=o.core_height=32;return o;}
// Independent expected geometry from the preregistered tma-dearray-v1 contract.
struct Fixture {const char* name;Point column,row;std::vector<std::size_t> missing;double height;};
bool missing(const Fixture& f,std::size_t i) {for(auto m:f.missing) if(i==m) return true;return false;}
std::shared_ptr<const TmaDearrayResult> fixture_contract(const std::filesystem::path& root) {
    std::shared_ptr<const TmaDearrayResult> retained;
    for(const Fixture& f:std::vector<Fixture>{{"aligned",{60,0},{0,60},{},32},
        {"interior_missing",{60,0},{0,60},{5},32},{"rotated_missing",{60,8},{-8,60},{3,6},32},
        {"skewed_missing",{60,4},{8,60},{5},32},{"anisotropic",{60,0},{0,60},{6},48}}) {
        auto image=mask(root/(std::string(f.name)+".pgm"));auto o=options();o.core_height=f.height;
        auto result=detect_tma_grid(image,480,352,o);check(result->grid.rows==3 && result->grid.columns==4 && result->cores.size()==12);
        check(result->candidates==12-f.missing.size());check(!result->thumbnail_plan);double maximum=0;
        for(std::size_t i=0;i<12;++i) {
            const auto row=i/4,col=i%4;const Point expected{112+col*f.column.x+row*f.row.x,96+col*f.column.y+row*f.row.y};
            const auto& core=result->cores[i];maximum=std::max(maximum,std::hypot(core.center.x-expected.x,core.center.y-expected.y));
            check(result->grid.cores[i].missing==missing(f,i));check(core.observed!=missing(f,i));
            check(core.valid_fraction==1 && bool(core.fit_error)==core.observed);
            check(result->grid.cores[i].case_id.empty());check(result->grid.cores[i].bounds.width==32*o.bounds_scale);
        }
        check(maximum<=2.0);check(result->input_sha256.size()==64 && result->recipe_sha256.size()==64);
        std::cout<<"TMA_DEARRAY_FIXTURE "<<f.name<<" max_center_error_pixels="<<maximum<<" missing_flags=match\n";
        if(!retained) retained=result;
    }
    for(const auto* name:{"blank","unobserved_row"}) {
        auto image=mask(root/(std::string(name)+".pgm"));
        rejects([&]{detect_tma_grid(image,480,352,options());},std::string(name)=="blank"?"contrast":"ambiguous");
    }
    return retained;
}
struct Server final:ImageServer {
    mutable ImageMetadata meta;PixelBuffer image;mutable int reads=0;int expected_level=0;bool bad_shape=false,bad_bytes=false,change=false,close_on_read=false;
    mutable bool is_closed=false;std::atomic<bool>* cancel_on_read=nullptr;
    explicit Server(PixelBuffer pixels):image(std::move(pixels)) {meta.width=image.width();meta.height=image.height();meta.levels={{meta.width,meta.height,1}};}
    const ImageMetadata& metadata() const override {return meta;}
    PixelBuffer read_region(const RegionRequest& r) const override {
        ++reads;check(r.x==0 && r.y==0 && r.level==expected_level && r.width==480 && r.height==352);
        if(change) meta.vendor="changed";
        if(close_on_read) is_closed=true;
        if(cancel_on_read) cancel_on_read->store(true);
        if(bad_shape) return PixelBuffer(1,1);
        auto out=image;if(bad_bytes) out.writable_bytes().pop_back();return out;
    }
    void close() override {is_closed=true;}bool closed() const override {return is_closed;}
};
void admission_and_io(const std::filesystem::path& root) {
    auto image=mask(root/"aligned.pgm");auto o=options();const auto bytes=tma_dearray_work_bytes(480,352,o);
    o.work_bytes=bytes;auto exact=detect_tma_grid(image,480,352,o);check(exact->estimated_work_bytes==bytes);
    --o.work_bytes;rejects([&]{detect_tma_grid(image,480,352,o);},"budget");o=options();
    std::atomic<bool> stop{true};rejects([&]{detect_tma_grid(image,480,352,o,&stop);},"cancelled");stop=false;
    auto malformed=image;malformed.writable_bytes().pop_back();rejects([&]{detect_tma_grid(malformed,480,352,o);},"byte count");
    rejects([&]{detect_tma_grid(image,479,352,o);},"non-upscaled");
    auto small=o;small.core_width=5;rejects([&]{detect_tma_grid(image,480,352,small);},"six preview pixels");
    auto capped=o;capped.max_candidates=11;rejects([&]{detect_tma_grid(image,480,352,capped);},"candidate count");
    capped=o;capped.pair_checks=65;rejects([&]{detect_tma_grid(image,480,352,capped);},"pair-work");
    auto invalid=o;invalid.core_width=std::numeric_limits<double>::quiet_NaN();rejects([&]{invalid.validate();},"width");
    invalid=o;invalid.rows=513;rejects([&]{invalid.validate();},"dimensions");
    // Explicit polarity yields identical geometry; raw-input/recipe identities differ.
    auto inverted=image;for(std::size_t i=0;i<inverted.bytes().size();++i) if(i%4!=3) inverted.writable_bytes()[i]=255-inverted.bytes()[i];
    auto light=o;light.polarity=TmaPolarity::LightOnDark;auto opposite=detect_tma_grid(inverted,480,352,light);
    check(opposite->cores[0].center.x==exact->cores[0].center.x && opposite->input_sha256!=exact->input_sha256);
    auto transparent=image;for(std::size_t i=3;i<transparent.bytes().size();i+=4) transparent.writable_bytes()[i]=0;
    rejects([&]{detect_tma_grid(transparent,480,352,o);},"contrast");
    Server server(image);o.work_bytes=bytes+480*352*4;auto read=detect_tma_grid(server,o);
    check(server.reads==1 && read->thumbnail_plan && read->thumbnail_plan->level==0 && read->estimated_work_bytes==o.work_bytes);
    check(read->input_sha256==exact->input_sha256 && read->recipe_sha256!=exact->recipe_sha256);
    --o.work_bytes;server.reads=0;rejects([&]{detect_tma_grid(server,o);},"budget");check(server.reads==0);o=options();
    o.thumbnail.source_pixel_limit=10;rejects([&]{detect_tma_grid(server,o);},"natural pyramid");check(server.reads==0);o=options();
    o.thumbnail.max_width=60;o.thumbnail.max_height=44;rejects([&]{detect_tma_grid(server,o);},"six preview pixels");check(server.reads==0);o=options();
    server.close();rejects([&]{detect_tma_grid(server,o);},"closed");check(server.reads==0);server.is_closed=false;
    server.bad_shape=true;rejects([&]{detect_tma_grid(server,o);},"unexpected dimensions");server.bad_shape=false;
    server.bad_bytes=true;rejects([&]{detect_tma_grid(server,o);},"unexpected dimensions");server.bad_bytes=false;
    server.change=true;rejects([&]{detect_tma_grid(server,o);},"metadata changed");server.change=false;
    server.close_on_read=true;rejects([&]{detect_tma_grid(server,o);},"metadata changed");server.close_on_read=false;server.is_closed=false;
    server.cancel_on_read=&stop;rejects([&]{detect_tma_grid(server,o,&stop);},"cancelled");
    std::cout<<"TMA_DEARRAY_ADMISSION exact/minus-one budgets, pre-IO geometry, malformed reads, epoch, alpha, polarity, cancellation passed\n";
}
void one_dimensional(const std::filesystem::path& root) {
    const auto original=mask(root/"aligned.pgm");
    for(const auto shape:std::vector<std::pair<std::size_t,std::size_t>>{{1,4},{3,1},{1,1}}) {
        auto image=original;auto& rgba=image.writable_bytes();
        for(std::int64_t y=0;y<352;++y) for(std::int64_t x=0;x<480;++x) {
            if((shape.first==1 && y>=120) || (shape.second==1 && x>=136)) {
                const auto i=static_cast<std::size_t>(y*480+x)*4;rgba[i]=rgba[i+1]=rgba[i+2]=255;
            }
        }
        auto o=options();o.rows=shape.first;o.columns=shape.second;const auto result=detect_tma_grid(image,480,352,o);
        check(result->cores.size()==o.rows*o.columns);
        for(std::size_t i=0;i<result->cores.size();++i) {
            check(result->cores[i].center.x==112+(i%o.columns)*60 && result->cores[i].center.y==96+(i/o.columns)*60);
            check(result->cores[i].observed && !result->grid.cores[i].missing);
        }
    }
    // Fully transparent support at an absent slot cannot be treated as a missing tissue core.
    auto image=mask(root/"interior_missing.pgm");
    for(int y=138;y<174;++y) for(int x=154;x<190;++x) image.writable_bytes()[static_cast<std::size_t>(y*480+x)*4+3]=0;
    rejects([&]{detect_tma_grid(image,480,352,options());},"no valid support");
    Server natural(original);natural.meta.width=960;natural.meta.height=704;
    natural.meta.levels={{960,704,1},{480,352,2}};natural.expected_level=1;
    auto o=options();o.core_width=o.core_height=64;o.thumbnail.max_width=480;o.thumbnail.max_height=352;
    auto scaled=detect_tma_grid(natural,o);
    check(natural.reads==1 && scaled->cores[0].center.x==224 && scaled->cores[0].center.y==192);
    check(scaled->thumbnail_plan->downsample_x==2 && scaled->thumbnail_plan->downsample_y==2);
    std::cout<<"TMA_DEARRAY_AXES singleton/row/column, invalid missing-slot support and natural-level coordinates passed\n";
}
void publication(const TmaDearrayResult& result) {
    Document doc(Image(480,352,std::vector<std::uint8_t>(480*352*3,255)));const auto before=doc.revision();
    auto ids=import_qptma(doc,result.grid,"Detected TMA");check(ids.size()==12 && doc.revision()==before+1 && doc.tma_core_count()==12);
    check(doc.undo() && doc.tma_core_count()==0);check(doc.redo() && doc.tma_core_count()==12);
    auto invalid=result.grid;invalid.cores.back().bounds.x=5000;Document other(Image(480,352,std::vector<std::uint8_t>(480*352*3,255)));
    const auto revision=other.revision();rejects([&]{import_qptma(other,invalid);},"");check(other.revision()==revision && other.tma_core_count()==0);
    const auto round=parse_qptma(qptma_text(result.grid));check(round.cores.size()==12 && round.cores[0].bounds.x==result.grid.cores[0].bounds.x);
    std::cout<<"TMA_DEARRAY_PUBLICATION retained proposal, atomic import/undo/redo, failed import rollback and QPTMA round trip passed\n";
}
}
int main(int argc,char** argv) {try {
    check(argc==2);const auto root=std::filesystem::u8path(argv[1]);auto retained=fixture_contract(root);admission_and_io(root);one_dimensional(root);publication(*retained);
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}

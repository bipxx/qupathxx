#include "../rgb_nominal.hpp"
#include "../sha256.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/resampling.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace {
void expect(bool value) {if(!value) throw std::runtime_error("RGB nominal reference check failed");}
template<class Exception,class Function> void refuses(Function action) {
    try {action();} catch(const Exception&) {return;}
    throw std::runtime_error("RGB nominal request was not rejected");
}
qpx::PixelBuffer render(const qpx::detail::RgbNominalPlan& plan,const std::vector<unsigned char>& input,std::int64_t sw) {
    std::optional<qpx::PixelBuffer> window;
    if(!plan.empty()) {
        window.emplace(plan.window.width,plan.window.height);
        auto& bytes=window->writable_bytes();
        for(std::int64_t row=0;row<plan.window.height;++row) for(std::int64_t col=0;col<plan.window.width;++col) {
            const auto source=std::size_t(((row+plan.window.y)*sw+col+plan.window.x)*3);
            const auto target=std::size_t((row*plan.window.width+col)*4);
            std::copy_n(input.data()+source,3,bytes.data()+target);bytes[target+3]=255;
        }
    }
    return qpx::detail::sample_rgb_nominal(plan,window?&*window:nullptr);
}
struct Reader final:qpx::ImageServer {
    qpx::ImageMetadata info;
    mutable int reads=0;
    mutable qpx::RegionRequest last{0,0,0,1,1};
    bool stopped=false;int fault=0;std::atomic<bool>* cancel_after_read=nullptr;
    Reader() {info.width=31;info.height=29;info.levels={{31,29,1}};}
    const qpx::ImageMetadata& metadata() const override {return info;}
    qpx::PixelBuffer read_region(const qpx::RegionRequest& request) const override {
        ++reads;last=request;
        if(fault==4) throw std::runtime_error("injected RGB reader failure");
        qpx::PixelBuffer pixels(request.width+(fault==1?1:0),request.height);
        std::fill(pixels.writable_bytes().begin(),pixels.writable_bytes().end(),255);
        if(fault==2) pixels.writable_bytes()[3]=128;
        if(fault==3) pixels.writable_bytes().clear();
        if(cancel_after_read) cancel_after_read->store(true);
        return pixels;
    }
    void close() override {stopped=true;}
    bool closed() const override {return stopped;}
};
}
void check_rgb_nominal_sampling() {
    using namespace qpx;
    using namespace qpx::detail;
    const auto root=std::filesystem::path(QPX_RGB_NOMINAL_FIXTURE);
    std::ifstream index(root/"index.txt");index.imbue(std::locale::classic());expect(bool(index));
    std::string line;std::getline(index,line);expect(line=="qpx.rgb-nominal-golden/1");unsigned count=0;
    while(std::getline(index,line)) {
        std::istringstream record(line);record.imbue(std::locale::classic());
        std::string name,file,expected;
        std::int64_t sw,sh,x,y,w,h,ow,oh;double step;
        expect(bool(record>>name>>file>>sw>>sh>>x>>y>>w>>h>>step>>ow>>oh>>expected));
        std::string extra;expect(!(record>>extra));
        std::ifstream stream(root/file,std::ios::binary);expect(bool(stream));
        const std::vector<unsigned char> input{std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
        expect(input.size()==static_cast<std::size_t>(sw*sh*3));
        const auto plan=plan_rgb_nominal(sw,sh,x,y,w,h,step);
        expect(plan.output_width==ow && plan.output_height==oh);
        std::optional<PixelBuffer> window;
        if(!plan.empty()) {
            window.emplace(plan.window.width,plan.window.height);
            auto& bytes=window->writable_bytes();
            for(std::int64_t row=0;row<plan.window.height;++row) for(std::int64_t col=0;col<plan.window.width;++col) {
                const auto source=static_cast<std::size_t>(((row+plan.window.y)*sw+col+plan.window.x)*3);
                const auto target=static_cast<std::size_t>((row*plan.window.width+col)*4);
                std::copy_n(input.data()+source,3,bytes.data()+target);bytes[target+3]=255;
            }
        }
        const auto original=window?window->bytes():std::vector<std::uint8_t>{};
        const auto output=sample_rgb_nominal(plan,window?&*window:nullptr);
        Sha256 hash;hash.update(output.bytes().data(),output.bytes().size());
        if(hash.finish()!=expected) throw std::runtime_error("RGB nominal captured pixels differ: "+name);
        const auto affine=render(plan_rgb_nominal(sw,sh,x,y,w,h,step,nullptr,RgbNominalCoordinates::AffineInverse),input,sw);
        expect(affine.bytes()==output.bytes());
        auto server=dense_image_server(Image(static_cast<int>(sw),static_cast<int>(sh),input));
        const RgbNominalRequest request{x,y,w,h,step};
        expect(std::string(request.algorithm())=="qpx.rgb-nominal/1");
        const auto read=read_rgb_nominal_region(*server,request);server->close();
        Sha256 reader_hash;reader_hash.update(read.bytes().data(),read.bytes().size());
        if(reader_hash.finish()!=expected) throw std::runtime_error("RGB nominal public reader pixels differ: "+name);
        // Each assembly is compared directly to the captured hash above. A new
        // integer source origin for each tile would change nondyadic phases.
        for(const std::int64_t tile:{1,7,32}) {
            PixelBuffer assembled(ow,oh);
            for(std::int64_t ty=0;ty<oh;ty+=tile) for(std::int64_t tx=0;tx<ow;tx+=tile) {
                const auto tw=std::min(tile,ow-tx),th=std::min(tile,oh-ty);
                const auto part=render(plan_rgb_nominal_window(sw,sh,x,y,w,h,step,tx,ty,tw,th),input,sw);
                for(std::int64_t row=0;row<th;++row)
                    std::copy_n(part.bytes().data()+row*tw*4,std::size_t(tw*4),assembled.writable_bytes().data()+((ty+row)*ow+tx)*4);
            }
            expect(assembled.bytes()==output.bytes());
        }
        const auto context=render(plan_rgb_nominal_window(sw,sh,x,y,w,h,step,-2,-3,ow+4,oh+6),input,sw);
        for(std::int64_t row=0;row<oh;++row)
            expect(std::equal(output.bytes().begin()+row*ow*4,output.bytes().begin()+(row+1)*ow*4,
                context.bytes().begin()+((row+3)*(ow+4)+2)*4));
        expect(output.width()==ow && output.height()==oh);
        if(window) expect(window->bytes()==original);
        ++count;
    }
    expect(count==99);
    std::ifstream affine_index(std::filesystem::path(QPX_RGB_AFFINE_FIXTURE)/"index.txt");
    affine_index.imbue(std::locale::classic());expect(bool(affine_index));
    std::getline(affine_index,line);expect(line=="qpx.rgb-affine-golden/1");
    unsigned affine_count=0;
    while(std::getline(affine_index,line)) {
        std::istringstream record(line);record.imbue(std::locale::classic());
        std::string name,file,expected;std::int64_t sw,sh,x,y,w,h,ow,oh;double step;
        expect(bool(record>>name>>file>>sw>>sh>>x>>y>>w>>h>>step>>ow>>oh>>expected));
        std::string extra;expect(!(record>>extra));
        std::ifstream stream(std::filesystem::path(QPX_RGB_AFFINE_FIXTURE)/file,std::ios::binary);expect(bool(stream));
        const std::vector<unsigned char> input{std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
        expect(input.size()==std::size_t(sw*sh*3));
        const auto mode=RgbNominalCoordinates::AffineInverse;
        const auto plan=plan_rgb_nominal(sw,sh,x,y,w,h,step,nullptr,mode);
        expect(plan.output_width==ow && plan.output_height==oh);
        const auto output=render(plan,input,sw);Sha256 hash;hash.update(output.bytes().data(),output.bytes().size());
        if(hash.finish()!=expected) throw std::runtime_error("RGB affine captured pixels differ: "+name);
        PixelBuffer assembled(ow,oh);
        for(std::int64_t ty=0;ty<oh;ty+=17) for(std::int64_t tx=0;tx<ow;tx+=17) {
            const auto tw=std::min(std::int64_t(17),ow-tx),th=std::min(std::int64_t(17),oh-ty);
            const auto part=render(plan_rgb_nominal_window(sw,sh,x,y,w,h,step,tx,ty,tw,th,nullptr,mode),input,sw);
            for(std::int64_t row=0;row<th;++row)
                std::copy_n(part.bytes().data()+row*tw*4,std::size_t(tw*4),assembled.writable_bytes().data()+((ty+row)*ow+tx)*4);
        }
        expect(assembled.bytes()==output.bytes());
        const auto context=render(plan_rgb_nominal_window(sw,sh,x,y,w,h,step,-3,-5,ow+6,oh+10,nullptr,mode),input,sw);
        for(std::int64_t row=0;row<oh;++row)
            expect(std::equal(output.bytes().begin()+row*ow*4,output.bytes().begin()+(row+1)*ow*4,
                context.bytes().begin()+((row+5)*(ow+6)+3)*4));
        ++affine_count;
    }
    expect(affine_count==13);
    const auto mode=RgbNominalCoordinates::AffineInverse;
    const auto negative_affine=plan_rgb_nominal(8,8,-50,-50,2,2,1,nullptr,mode);
    expect(negative_affine.empty());
    const auto large_affine=plan_rgb_nominal(3000000000000LL,3000000000000LL,2000000000000LL,2000000000000LL,2,2,1,nullptr,mode);
    expect(large_affine.window.x==2000000000000LL && large_affine.window.y==2000000000000LL);
    refuses<std::invalid_argument>([&] {plan_rgb_nominal(8,8,(std::int64_t(1)<<53)+1,0,2,2,1,nullptr,mode);});
    refuses<std::invalid_argument>([&] {plan_rgb_nominal(8,8,-(std::int64_t(1)<<53)-1,0,2,2,1,nullptr,mode);});
    refuses<std::invalid_argument>([] {plan_rgb_nominal(8,8,0,0,2,2,1,nullptr,RgbNominalCoordinates(2));});
    std::cout<<"112 captured RGB affine reads exact; odd crop, tiled/context cores, signed/large origins and precision admission passed\n";
    const auto halo=plan_rgb_nominal(31,29,3,5,24,20,1.1);
    expect(halo.window.x+halo.window.width>27);
    const auto huge=plan_rgb_nominal(3000000000000LL,3000000000000LL,2000000000000LL,2000000000000LL,2,2,1);
    expect(huge.window.x==2000000000000LL && huge.window.y==2000000000000LL);
    PixelBuffer small(2,2,std::vector<std::uint8_t>(16,255));
    expect(sample_rgb_nominal(huge,&small).bytes()==small.bytes());
    const auto outside=plan_rgb_nominal(8,8,std::numeric_limits<std::int64_t>::min(),0,2,2,1);
    expect(outside.empty());const auto black=sample_rgb_nominal(outside,nullptr);
    for(std::size_t i=0;i<black.bytes().size();++i) expect(black.bytes()[i]==(i%4==3?255:0));
    for(const auto step:{0.,-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN(),1e-320})
        refuses<std::invalid_argument>([&] {plan_rgb_nominal(8,8,0,0,2,2,step);});
    refuses<std::invalid_argument>([] {plan_rgb_nominal(8,8,0,0,0,2,1);});
    refuses<std::invalid_argument>([] {plan_rgb_nominal(8,8,0,0,2,2,100);});
    refuses<std::invalid_argument>([] {plan_rgb_nominal(8,8,0,0,67108864,2,1);});
    refuses<std::invalid_argument>([] {plan_rgb_nominal(8,8,std::numeric_limits<std::int64_t>::max(),0,2,2,1);});
    refuses<std::runtime_error>([&] {sample_rgb_nominal(huge,nullptr);});
    PixelBuffer wrong(1,1,{0,0,0,255});
    refuses<std::runtime_error>([&] {sample_rgb_nominal(huge,&wrong);});
    small.writable_bytes()[3]=128;
    refuses<std::invalid_argument>([&] {sample_rgb_nominal(huge,&small);});
    small.writable_bytes().clear();
    refuses<std::runtime_error>([&] {sample_rgb_nominal(huge,&small);});
    std::atomic<bool> cancel{true};
    refuses<std::runtime_error>([&] {plan_rgb_nominal(8,8,0,0,2,2,1,&cancel);});
    refuses<std::runtime_error>([&] {sample_rgb_nominal(outside,nullptr,&cancel);});
    Reader reader;
    const RgbNominalRequest crop{3,5,24,20,1.1};
    const auto owned=read_rgb_nominal_region(reader,crop);
    expect(reader.reads==1 && reader.last.level==0 && reader.last.x+reader.last.width>27);
    reader.close();expect(owned.width()==22 && owned.bytes().front()==255);
    refuses<std::runtime_error>([&] {read_rgb_nominal_region(reader,crop);});
    expect(reader.reads==1);
    Reader no_io;
    refuses<std::invalid_argument>([&] {RgbNominalRequest request{0,0,2,2,0};read_rgb_nominal_region(no_io,request);});
    refuses<std::runtime_error>([&] {read_rgb_nominal_region(no_io,crop,&cancel);});
    expect(no_io.reads==0);
    const auto empty=read_rgb_nominal_region(no_io,RgbNominalRequest{-50,-50,2,2,1});
    expect(no_io.reads==0 && empty.bytes()==black.bytes());
    no_io.info.levels.front().downsample=2;
    refuses<std::invalid_argument>([&] {read_rgb_nominal_region(no_io,crop);});
    expect(no_io.reads==0);
    for(int fault:{1,2,3,4}) {
        Reader bad;bad.fault=fault;
        if(fault==2) refuses<std::invalid_argument>([&] {read_rgb_nominal_region(bad,crop);});
        else refuses<std::runtime_error>([&] {read_rgb_nominal_region(bad,crop);});
        expect(bad.reads==1);
    }
    std::atomic<bool> late_cancel{false};Reader interrupted;interrupted.cancel_after_read=&late_cancel;
    refuses<std::runtime_error>([&] {read_rgb_nominal_region(interrupted,crop,&late_cancel);});
    expect(interrupted.reads==1);
    // Independent negative-context reference: exact binary nominal factors
    // permit an integer source translation, retaining the same sampled centers.
    std::vector<unsigned char> ramp(61*53*3);
    for(std::size_t i=0;i<ramp.size();++i) ramp[i]=std::uint8_t((i*29+i/17)%256);
    for(const auto step:{.75,1.,1.25,1.5,2.,3.75}) {
        const auto base=plan_rgb_nominal(61,53,19,17,24,18,step);
        const auto shifted=std::int64_t(4*step);
        const auto extended=plan_rgb_nominal_window(61,53,19,17,24,18,step,-4,-4,base.output_width+8,base.output_height+8);
        const auto translated=plan_rgb_nominal(61,53,19-shifted,17-shifted,24+2*shifted,18+2*shifted,step);
        expect(render(extended,ramp,61).bytes()==render(translated,ramp,61).bytes());
    }
    const auto far=plan_rgb_nominal_window(61,53,0,0,8192,8192,16384,-4096,-4096,1,1);
    expect(far.empty());expect(render(far,ramp,61).bytes()==std::vector<std::uint8_t>({0,0,0,255}));
    for(const auto x:{std::numeric_limits<std::int64_t>::min(),std::numeric_limits<std::int64_t>::max(),std::int64_t(-4097),std::int64_t(4100)})
        refuses<std::invalid_argument>([&] {plan_rgb_nominal_window(8,8,0,0,2,2,1,x,0,1,1);});
    refuses<std::invalid_argument>([] {plan_rgb_nominal_window(8,8,0,0,2,2,1,0,0,0,1);});
    refuses<std::invalid_argument>([] {plan_rgb_nominal_window(8,8,0,0,2,2,1,-4096,0,8195,1);});
    refuses<std::runtime_error>([&] {plan_rgb_nominal_window(8,8,0,0,2,2,1,0,0,1,1,&cancel);});
    std::cout<<"99 pinned RGB nominal reads, fixed-point bytes, global crop context, admission and cancellation passed\n";
    std::cout<<"99 public RGB nominal reader hashes, exact halo IO, closed-reader ownership and decoder/cancellation rejection passed\n";
    std::cout<<"99 RGB nominal captured outputs stitch at three tile sizes with unchanged context cores; six negative-context translations and admission passed\n";
}

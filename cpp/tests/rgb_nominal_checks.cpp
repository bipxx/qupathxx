#include "../rgb_nominal.hpp"
#include "../sha256.hpp"
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
        expect(output.width()==ow && output.height()==oh);
        if(window) expect(window->bytes()==original);
        ++count;
    }
    expect(count==99);
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
    std::cout<<"99 pinned RGB nominal reads, fixed-point bytes, global crop context, admission and cancellation passed\n";
}

#include "qupathxx/rgb_preview.hpp"
#include "qupathxx/core.hpp"
#include <cmath>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace qpx;
void checked(bool value,int line){if(!value)throw std::runtime_error("RGB preview check failed at line "+std::to_string(line));}
#define check(...) checked((__VA_ARGS__),__LINE__)
template<class F>void rejects(F fn){bool caught=false;try{fn();}catch(const std::exception&){caught=true;}check(caught);}
struct Counted final:ImageServer {
    std::shared_ptr<ImageServer> source=dense_image_server(Image(17,13,std::vector<std::uint8_t>(17*13*3,80)));
    mutable std::size_t reads=0;std::atomic<bool>* stop=nullptr;
    const ImageMetadata& metadata()const override{return source->metadata();}
    PixelBuffer read_region(const RegionRequest& request)const override{++reads;auto result=source->read_region(request);if(stop)stop->store(true);return result;}
    void close()override{source->close();}bool closed()const override{return source->closed();}
};
int main(){try {
    std::vector<std::uint8_t> rgb(17*13*3);for(std::size_t i=0;i<rgb.size();++i)rgb[i]=std::uint8_t(i%256);
    Document document(Image(17,13,rgb));document.set_calibration(.5,1.2);
    const RgbNominalRequest request(0,0,17,13,1.5);
    const auto expected=read_rgb_nominal_region(*document.server(),request);
    const auto preview=make_rgb_nominal_preview(document,request);
    check(preview->server()!=document.server() && preview->pixel_width_um()==.75 && preview->pixel_height_um()==1.2*1.5);
    document.server()->close();
    check(preview->server()->read_region({0,0,0,expected.width(),expected.height()}).bytes()==expected.bytes());
    auto counted=std::make_shared<Counted>();
    const auto unknown=make_rgb_nominal_preview(counted,request);
    const auto partial=make_rgb_nominal_preview(counted,request,.5,{});
    check(!unknown->pixel_width_um() && !unknown->pixel_height_um() && !partial->pixel_width_um() && !partial->pixel_height_um());
    const auto reads=counted->reads;
    for(auto scale:{0.,-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::max()})
        rejects([&]{make_rgb_nominal_preview(counted,request,scale,scale);});
    check(counted->reads==reads);
    std::atomic<bool> cancel{true};rejects([&]{make_rgb_nominal_preview(counted,request,{},{},&cancel);});check(counted->reads==reads);
    cancel.store(false);counted->stop=&cancel;rejects([&]{make_rgb_nominal_preview(counted,request,{},{},&cancel);});check(counted->reads==reads+1);
    counted->stop=nullptr;counted->close();rejects([&]{make_rgb_nominal_preview(counted,request);});
    rejects([&]{make_rgb_nominal_preview(std::shared_ptr<ImageServer>{},request);});
    const auto root=std::filesystem::temp_directory_path()/std::filesystem::u8path(std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-rgb-preview");
    std::filesystem::create_directory(root);const auto path=root/"source.PPM";
    {std::ofstream file(path,std::ios::binary);file<<"P6\n17 13\n255\n";file.write(reinterpret_cast<const char*>(rgb.data()),std::streamsize(rgb.size()));}
    auto source=open_rgb_nominal_source(path.u8string());
    check(source->read_region({0,0,0,17,13}).bytes()==dense_image_server(Image(17,13,rgb))->read_region({0,0,0,17,13}).bytes());
    source->close();rejects([&]{open_rgb_nominal_source(path.u8string(),"unknown");});rejects([&]{open_rgb_nominal_source("");});
    std::cout<<"Native nominal preview: owned exact RGB, calibration/unknown/overflow, pre/post-read cancel, closed/null source and native reader selection passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}

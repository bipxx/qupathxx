#include "qupathxx/pixel_classifier.hpp"
#include "object_forest_fixture.hpp"
#include "raw_pixel_feature_checks.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace qpx;
void check(bool v) {if(!v) throw std::runtime_error("pixel feature check failed");}
template<class F> void rejects(F f) {try {f();}catch(const std::exception&) {return;}throw std::runtime_error("missing pixel feature rejection");}
int main() {try {
    const StainTransform transform({{1,0,0},{0,1,0}});
    PixelFeatureOptions options;options.sigmas={0,1};options.gradient_magnitude=options.laplacian=true;
    PixelBuffer constant(4,3);for(std::size_t i=0;i<12;++i) {auto& b=constant.writable_bytes();b[4*i]=10;b[4*i+1]=20;b[4*i+2]=30;b[4*i+3]=255;}constant.writable_bytes()[3]=128;
    const auto result=extract_pixel_features(constant,transform,options);check(result->features().size()==18 && result->valid_pixels()==11 && !result->valid()[0]);
    for(std::size_t i=0;i<12;++i) for(std::size_t col=0;col<18;++col) {const auto v=result->values()[i*18+col];if(!i) check(std::isnan(v));else check(std::abs(v-(col%3==0?10*(col/6+1):0))<1e-5);}
    PixelBuffer pixels(40,35);for(std::size_t i=0;i<40*35;++i) {auto& b=pixels.writable_bytes();b[4*i]=static_cast<std::uint8_t>(i*11);b[4*i+1]=static_cast<std::uint8_t>(i*17);b[4*i+2]=static_cast<std::uint8_t>(i*23);b[4*i+3]=i%13?255:0;}
    const auto full=extract_pixel_features(pixels,transform,options);
    const PixelFeatureWindow region{40,35,12,10,5,4};const auto cropped=extract_pixel_features(pixels,transform,options,region);
    check(cropped->context_x()==8 && cropped->context_y()==6 && cropped->context_width()==13 && cropped->context_height()==12);
    PixelBuffer tile(13,12);for(int y=0;y<12;++y) std::copy_n(pixels.bytes().data()+((6+y)*40+8)*4,13*4,tile.writable_bytes().data()+y*13*4);
    const auto tiled=extract_pixel_features(tile,transform,options,PixelFeatureWindow{40,35,12,10,5,4,8,6});
    check(tiled->sha256()==cropped->sha256() && tiled->input_sha256()==cropped->input_sha256());
    for(std::size_t y=0;y<4;++y) for(std::size_t x=0;x<5;++x) for(std::size_t c=0;c<18;++c) {const auto a=full->values()[((y+10)*40+x+12)*18+c],b=tiled->values()[(y*5+x)*18+c];check((std::isnan(a)&&std::isnan(b)) || a==b);}
    rejects([&]{extract_pixel_features(tile,transform,options,PixelFeatureWindow{40,35,12,10,5,4,9,6});});
    auto budget=options;budget.work_bytes=1024*1024;rejects([&]{pixel_feature_work_bytes(1000,1000,budget);});
    PixelFeatureOptions raw;raw.sigmas={0};check(pixel_feature_work_bytes(1000000000,1,raw,PixelFeatureWindow{1000000000,1,500000000,0,1,1})<1024*1024);
    auto invalid=raw;invalid.sigmas={0,0};rejects([&]{invalid.validate();});invalid.sigmas={-0.0};rejects([&]{invalid.validate();});
    invalid=raw;invalid.rgb=invalid.optical_density=invalid.stains=true;invalid.sigmas={0,1,2,3,4,5,6,7};invalid.gradient_magnitude=invalid.laplacian=true;rejects([&]{invalid.validate();});
    const auto raw_result=extract_pixel_features(pixels,transform,raw);const StainTransform other({{1,0,0},{0,1,0}},{200,210,220});check(extract_pixel_features(pixels,other,raw)->sha256()==raw_result->sha256());
    raw.rgb=false;raw.optical_density=true;const auto od=extract_pixel_features(pixels,transform,raw);const auto expected=transform.optical_densities(pixels);
    for(std::size_t i=0;i<od->values().size();++i) check(od->values()[i]==expected.values()[i] || (std::isnan(od->values()[i])&&std::isnan(expected.values()[i])));
    check(pixel_feature_schema(raw,transform)[0].name!=pixel_feature_schema(raw,other)[0].name);
    std::atomic<bool> cancelled{true};rejects([&]{extract_pixel_features(pixels,transform,options,{},&cancelled);});
    PixelBuffer broken(1,1);broken.writable_bytes().clear();rejects([&]{extract_pixel_features(broken,transform);});
    PixelFeatureOptions recipe_options;recipe_options.sigmas={0};
    std::vector<std::pair<std::string,std::string>> schema;for(const auto& f:pixel_feature_schema(recipe_options,transform)) schema.emplace_back(f.name,f.unit);
    const auto forest=ForestModel::from_bytes(qpx_test::object_forest_fixture(schema));const PixelClassifier recipe(forest,recipe_options,transform);
    check(PixelClassifier::from_bytes(recipe.bytes()).bytes()==recipe.bytes());
    auto damaged=recipe.bytes();damaged.back()^=1;rejects([&]{PixelClassifier::from_bytes(damaged);});damaged.resize(8);rejects([&]{PixelClassifier::from_bytes(damaged);});
    auto mismatch=recipe_options;mismatch.sigmas={1};rejects([&]{PixelClassifier wrong(forest,mismatch,transform);});
    PixelBuffer sample(3,1);sample.writable_bytes()={0,0,0,255,10,0,0,255,10,0,0,128};
    const auto labels=classify_pixels(sample,recipe);check(labels->labels()==std::vector<std::uint32_t>{1,2,0});
    check(labels->vote_fractions()[0]==.5 && labels->vote_fractions()[1]==1 && std::isnan(labels->vote_fractions()[2]));
    check(labels->valid_pixels()==2 && labels->classified_pixels()==2 && labels->class_counts()==std::vector<std::uint64_t>{1,1});
    const auto threshold=classify_pixels(sample,recipe,.75);check(threshold->labels()==std::vector<std::uint32_t>{0,2,0} && threshold->valid()[0]);
    check(classify_pixels(sample,recipe,-0.)->sha256()==labels->sha256());
    rejects([&]{classify_pixels(sample,recipe,1.1);});rejects([&]{classify_pixels(sample,recipe,0,{},1);});
    rejects([&]{classify_pixels(sample,recipe,0,{},0,&cancelled);});
    const auto path=(std::filesystem::temp_directory_path()/("qpx-pixel-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".qpc")).u8string();
    save_pixel_classifier(recipe,path);check(load_pixel_classifier(path).sha256()==recipe.sha256());rejects([&]{save_pixel_classifier(recipe,path);});
    save_pixel_classifier(recipe,path,true);rejects([&]{save_pixel_classifier(recipe,path,true,&cancelled);});check(load_pixel_classifier(path).sha256()==recipe.sha256());
    {std::ofstream bad(path,std::ios::binary|std::ios::trunc);bad<<"preserve this image";}rejects([&]{save_pixel_classifier(recipe,path,true);});
    {std::ifstream bad(path,std::ios::binary);std::string text((std::istreambuf_iterator<char>(bad)),{});check(text=="preserve this image");}std::filesystem::remove(std::filesystem::u8path(path));
    raw_feature_checks::run();
    std::cout<<"Pixel feature channels, context/tiles, alpha, schema, bounds and cancellation passed\n";return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}

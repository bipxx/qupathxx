#pragma once
#include "qupathxx/raw_pixel_classifier.hpp"
#include "qupathxx/pixel_classifier.hpp"
#include "raw_pixel_feature_checks.hpp"
#include "object_forest_fixture.hpp"
#include "../sha256.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>

namespace raw_classifier_checks {
using raw_feature_checks::check;
using raw_feature_checks::rejects;
inline qpx::ForestModel forest(const qpx::RawPixelRecipe& recipe) {
    std::vector<std::pair<std::string,std::string>> schema;for(const auto& f:recipe.feature_schema()) schema.emplace_back(f.name,f.unit);
    return qpx::ForestModel::from_bytes(qpx_test::object_forest_fixture(schema));
}
inline std::vector<std::uint8_t> checksum(std::vector<std::uint8_t> bytes) {qpx::Sha256 hash;hash.update(bytes.data(),bytes.size());const auto value=hash.finish();bytes.insert(bytes.end(),value.begin(),value.end());return bytes;}
// Independently authored QPR1 framing for the simple two-channel recipe below.
inline std::vector<std::uint8_t> encoded(const qpx::ForestModel& forest) {
    std::vector<std::uint8_t> bytes{'Q','P','R',1};
    auto integer=[&](std::uint64_t value) {for(int i=0;i<8;++i) bytes.push_back(std::uint8_t(value>>(i*8)));};
    auto real=[&](double value) {std::uint64_t bits;std::memcpy(&bits,&value,8);integer(bits);};
    for(auto value:{0ULL,0ULL,0ULL,2ULL,0ULL,0ULL,256ULL*1024*1024,1ULL,1ULL,1ULL,1ULL,0ULL}) integer(value);
    integer(1);bytes.push_back('a');integer(1);bytes.push_back('b');
    real(0);real(1);real(0);real(1);integer(1);real(0);real(0);integer(0);integer(256ULL*1024*1024);
    const auto payload=forest.bytes();integer(payload.size());bytes.insert(bytes.end(),payload.begin(),payload.end());return checksum(std::move(bytes));
}
inline void run() {
    using namespace qpx;
    raw_feature_checks::Reader reader(0);ScientificRawOptions selection;selection.channels=2;
    RawPixelFeatureOptions options;options.offsets={0,0};options.scales={1,1};
    const ScientificGrid grid(7,5,{0,7,7},{0,5,5});const auto input=read_scientific_raw(reader,grid,selection);
    const RawPixelRecipe recipe(input.metadata(),grid,selection,options);const auto trees=forest(recipe);const RawPixelClassifier model(trees,recipe);
    check(model.bytes()==encoded(trees));check(RawPixelClassifier::from_bytes(model.bytes()).bytes()==model.bytes());
    const auto result=classify_raw_pixels(input,model);check(result->grid().x().extent()==7 && result->grid().y().extent()==5 && result->labels().size()==35);
    std::vector<std::uint64_t> counts(2);
    for(std::size_t i=0;i<35;++i) {
        const bool valid=i!=17;check(bool(result->valid()[i])==valid);
        if(!valid) {check(result->labels()[i]==0 && std::isnan(result->vote_fractions()[i]));continue;}
        const auto label=i<=1?1u:2u;check(result->labels()[i]==label && result->vote_fractions()[i]==(label==1?.5f:1.f));++counts[label-1];
    }
    check(result->class_counts()==counts);check(classify_raw_pixels(input,model,-0.)->sha256()==result->sha256());
    const auto threshold=classify_raw_pixels(input,model,.75);check(threshold->labels()[0]==0 && threshold->valid()[0] && threshold->vote_fractions()[0]==.5f && threshold->class_counts()[0]==0);
    const ScientificGrid tile(7,5,{2,3,3},{1,2,2});const auto part=classify_raw_pixels(read_scientific_raw(reader,tile,selection),model);
    for(std::size_t y=0;y<2;++y) for(std::size_t x=0;x<3;++x) {const auto a=y*3+x,b=(y+1)*7+x+2;check(part->labels()[a]==result->labels()[b] && part->valid()[a]==result->valid()[b]);}
    const ScientificGrid large(7,5,{0,7,7,60},{0,5,5,60});const auto large_input=read_scientific_raw(reader,large,selection);const auto large_result=classify_raw_pixels(large_input,model);
    const auto exact=large_result->estimated_work_bytes();check(exact>=1024*1024);check(classify_raw_pixels(large_input,model,0,exact)->sha256()==large_result->sha256());rejects<std::length_error>([&]{classify_raw_pixels(large_input,model,0,exact-1);});
    std::atomic<bool> cancelled{true};rejects<std::runtime_error>([&]{classify_raw_pixels(input,model,0,0,&cancelled);});rejects<std::invalid_argument>([&]{classify_raw_pixels(input,model,1.1);});
    auto changed=*reader.raw;changed.channel_names[1]="other";check(!recipe.matches(changed,grid,selection));changed=*reader.raw;changed.sample_type=SampleType::Int8;check(!recipe.matches(changed,grid,selection));
    changed=*reader.raw;changed.series=1;changed.series_count=2;check(!recipe.matches(changed,grid,selection));
    ScientificRawOptions other=selection;other.z=1;changed=*reader.raw;changed.z_planes=2;check(!recipe.matches(changed,grid,other));
    const ScientificGrid coarse(7,5,{0,6,3},{0,4,2});check(!recipe.matches(*reader.raw,coarse,selection));
    // Calibration semantics are per axis, with exactly equal binary64 sampled
    // physical spacing. Different source step ratios may match physically.
    changed=*reader.raw;changed.pixel_width_um=.5;changed.pixel_height_um=.25;
    auto physical_options=options;physical_options.sigmas={{.5,.5}};
    const RawPixelRecipe physical(changed,grid,selection,physical_options,true);check(physical.features().sigmas[0].x==1 && physical.features().sigmas[0].y==2);
    const RawPixelClassifier calibrated(forest(physical),physical);check(RawPixelClassifier::from_bytes(calibrated.bytes()).bytes()==calibrated.bytes());
    auto other_calibration=changed;other_calibration.pixel_width_um=.25;other_calibration.pixel_height_um=.125;check(physical.matches(other_calibration,coarse,selection));
    std::swap(other_calibration.pixel_width_um,other_calibration.pixel_height_um);check(!physical.matches(other_calibration,coarse,selection));
    other_calibration.pixel_width_um.reset();rejects<std::invalid_argument>([&]{physical.matches(other_calibration,coarse,selection);});rejects<std::invalid_argument>([&]{RawPixelRecipe unknown(*reader.raw,grid,selection,physical_options,true);});
    // Physical sigma is not capped in micrometres by a grid-pixel limit.
    changed.pixel_width_um=changed.pixel_height_um=100.;physical_options.sigmas={{100,200}};const RawPixelRecipe wide(changed,grid,selection,physical_options,true);check(wide.features().sigmas[0].x==1 && wide.features().sigmas[0].y==2);
    auto wrong_options=options;wrong_options.offsets[0]=1;const RawPixelRecipe wrong(*reader.raw,grid,selection,wrong_options);rejects<std::invalid_argument>([&]{RawPixelClassifier mismatch(trees,wrong);});
    auto damaged=model.bytes();damaged.back()^=1;rejects<std::invalid_argument>([&]{RawPixelClassifier::from_bytes(damaged);});
    damaged=model.bytes();damaged[3]=2;rejects<std::invalid_argument>([&]{RawPixelClassifier::from_bytes(damaged);});
    damaged=model.bytes();damaged.resize(24);rejects<std::invalid_argument>([&]{RawPixelClassifier::from_bytes(damaged);});
    damaged=model.bytes();damaged.resize(damaged.size()-64);damaged[4]=255;damaged=checksum(std::move(damaged));rejects<std::invalid_argument>([&]{RawPixelClassifier::from_bytes(damaged);});
    rejects<std::invalid_argument>([&]{PixelClassifier::from_bytes(model.bytes());});
    const auto path=(std::filesystem::temp_directory_path()/("qpx-raw-model-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".qpr")).u8string();
    save_raw_pixel_classifier(model,path);check(load_raw_pixel_classifier(path).sha256()==model.sha256());rejects<std::exception>([&]{save_raw_pixel_classifier(model,path);});
    save_raw_pixel_classifier(model,path,true);rejects<std::runtime_error>([&]{save_raw_pixel_classifier(model,path,true,&cancelled);});check(load_raw_pixel_classifier(path).sha256()==model.sha256());
    {std::ofstream output(path,std::ios::binary|std::ios::trunc);output<<"preserve this image";}rejects<std::invalid_argument>([&]{save_raw_pixel_classifier(model,path,true);});
    {std::ifstream stream(path,std::ios::binary);const std::string content((std::istreambuf_iterator<char>(stream)),{});check(content=="preserve this image");}std::filesystem::remove(std::filesystem::u8path(path));
    check(result->labels()[34]==2); // Prior owning result survives model file changes.
    std::cout<<"Raw classifier QPR1 independent framing, channel/grid/physical contracts, forest votes, streams, exact budgets and atomic model files passed\n";
}
}

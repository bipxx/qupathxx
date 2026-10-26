#pragma once
#include "../raw_pixel_features.hpp"
#include "raw_pixel_feature_reference.hpp"
#include <cstring>
#include <limits>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace raw_feature_checks {
inline void check(bool ok) {if(!ok) throw std::runtime_error("raw pixel feature reference check failed");}
template<class E,class F> void rejects(F f) {try {f();}catch(const E&) {return;}throw std::runtime_error("raw pixel feature rejection missing");}
class Reader final:public qpx::ImageServer {
    unsigned which_;
    qpx::ImageMetadata display_;
public:
    std::shared_ptr<qpx::RawImageMetadata> raw=std::make_shared<qpx::RawImageMetadata>();
    mutable unsigned reads=0;
    explicit Reader(unsigned which):which_(which) {
        raw->width=7;raw->height=5;raw->channels=2;raw->sample_type=qpx::SampleType(raw_feature_reference::cases[which].type);raw->levels={{7,5,1}};raw->channel_names={"a","b"};
        display_.width=7;display_.height=5;display_.levels={{7,5,1}};
    }
    const qpx::ImageMetadata& metadata() const override {return display_;}
    std::string reader_id() const override {return "raw-feature-reference";}
    std::shared_ptr<const qpx::RawImageMetadata> raw_metadata(const std::atomic<bool>*) const override {return raw;}
    qpx::PixelBuffer read_region(const qpx::RegionRequest&) const override {throw std::runtime_error("raw features must not use display IO");}
    qpx::RawPixelBuffer read_raw(const qpx::RawRegionRequest& r,const std::atomic<bool>*) const override {
        ++reads;qpx::RawPixelBuffer out(r.region.width,r.region.height,r.channels,raw->sample_type);
        std::visit([&](auto& values) {
            using T=typename std::decay_t<decltype(values)>::value_type;
            for(std::int64_t y=0;y<out.height();++y) for(std::int64_t x=0;x<out.width();++x) {
                const auto sx=r.region.x+x,sy=r.region.y+y;
                const auto pixel=std::size_t(y*out.width()+x);out.writable_valid()[pixel]=!(sx==3 && sy==2);
                for(std::uint32_t c=0;c<r.channels;++c) {
                    const auto bits=raw_feature_reference::cases[which_].raw[(sy*7+sx)*2+r.first_channel+c];T value;
                    if constexpr(sizeof(T)==8) std::memcpy(&value,&bits,8);
                    else if constexpr(sizeof(T)==4) {const auto b=std::uint32_t(bits);std::memcpy(&value,&b,4);}
                    else if constexpr(sizeof(T)==2) {const auto b=std::uint16_t(bits);std::memcpy(&value,&b,2);}
                    else {const auto b=std::uint8_t(bits);std::memcpy(&value,&b,1);}
                    values[pixel*r.channels+c]=value;
                }
            }
        },out.writable_storage());return out;
    }
    void close() override {}
    bool closed() const override {return false;}
};
inline std::uint32_t bits(float value) {std::uint32_t out;std::memcpy(&out,&value,4);return out;}
inline void run() {
    using namespace qpx;
    for(unsigned which=0;which<12;++which) {
        Reader reader(which);const auto& reference=raw_feature_reference::cases[which];
        detail::RawFeatureOptions o;o.offsets={0,0};o.scales={reference.scale,reference.scale};o.sigmas={{0,0},{0,1},{.5,1},{1,1}};o.gradient_magnitude=o.laplacian=true;
        const ScientificGrid grid(7,5,{0,7,7,4},{0,5,5,4});ScientificRawOptions raw;raw.channels=2;
        const auto work=detail::raw_feature_work_bytes(*reader.raw,grid,raw,o);
        const auto sampled=read_scientific_raw(reader,grid,raw);const auto image=detail::extract_raw_pixel_features(sampled,o);
        check(image->width==7 && image->height==5 && image->features.size()==24 && image->estimated_work_bytes==work);
        for(std::size_t i=0;i<image->values.size();++i) {
            const auto expected_bits=reference.expected[i];float expected;std::memcpy(&expected,&expected_bits,4);
            if(std::isnan(expected)) {check(!image->valid[i] && std::isnan(image->values[i]));continue;}
            check(image->valid[i]);
            const auto ulp=std::abs(double(std::nextafter(expected,std::numeric_limits<float>::infinity()))-expected);
            check(std::abs(double(image->values[i])-expected)<=4*ulp+1e-12*std::max(1.,reference.max_normalized));
            if(i%12==0) check(bits(image->values[i])==expected_bits);
        }
        for(std::size_t row=0;row<35;++row) {bool valid=true;for(std::size_t c=0;c<24;++c) valid=valid && image->valid[row*24+c];check(bool(image->rows_valid[row])==valid);}
        const ScientificGrid tile(7,5,{2,3,3,4},{1,2,2,4});const auto sampled_tile=read_scientific_raw(reader,tile,raw);const auto part=detail::extract_raw_pixel_features(sampled_tile,o);
        for(std::size_t y=0;y<2;++y) for(std::size_t x=0;x<3;++x) for(std::size_t c=0;c<24;++c) check(bits(part->values[(y*3+x)*24+c])==bits(image->values[((y+1)*7+x+2)*24+c]));
        check(detail::extract_raw_pixel_features(sampled,o)->sha256==image->sha256);
        auto altered=o;altered.offsets[0]=.5;check(detail::extract_raw_pixel_features(sampled,altered)->input_sha256!=image->input_sha256);
        std::atomic<bool> cancelled{true};rejects<std::runtime_error>([&]{detail::extract_raw_pixel_features(sampled,o,&cancelled);});
        // For this small grid, add unused context until admission reaches the
        // minimum supported budget, then verify its exact byte boundary.
        const ScientificGrid large(7,5,{0,7,7,60},{0,5,5,60});
        const auto exact=detail::raw_feature_work_bytes(*reader.raw,large,raw,o);check(exact>=1024*1024);
        auto limited=o;limited.work_bytes=exact;check(detail::raw_feature_work_bytes(*reader.raw,large,raw,limited)==exact);
        limited.work_bytes=exact-1;rejects<std::length_error>([&]{detail::raw_feature_work_bytes(*reader.raw,large,raw,limited);});
        const auto before=reader.reads;rejects<std::invalid_argument>([&]{const ScientificGrid short_halo(7,5,{0,7,7,3},{0,5,5,4});detail::raw_feature_work_bytes(*reader.raw,short_halo,raw,o);});check(reader.reads==before);
    }
    detail::RawFeatureOptions invalid;invalid.offsets={0};invalid.scales={1};invalid.sigmas={{0,0},{0,1}};invalid.validate(1);
    invalid.sigmas={{0,0},{1,0}};invalid.validate(1);
    invalid.sigmas={{0,0},{0,0}};rejects<std::invalid_argument>([&]{invalid.validate(1);});
    invalid.sigmas={{-0.,0}};rejects<std::invalid_argument>([&]{invalid.validate(1);});
    invalid.sigmas={{0,0}};invalid.scales={0};rejects<std::invalid_argument>([&]{invalid.validate(1);});
    Reader extreme(10);ScientificRawOptions raw;raw.channels=2;const ScientificGrid grid(7,5,{0,7,7},{0,5,5});const auto sampled=read_scientific_raw(extreme,grid,raw);
    invalid.offsets={0,0};invalid.scales={1,1};rejects<std::invalid_argument>([&]{detail::extract_raw_pixel_features(sampled,invalid);});
    std::cout<<"Typed raw pixel features: 12 independent dtype/extreme references, per-channel support, anisotropic scales, stream seams, admission and cancellation passed\n";
}
}

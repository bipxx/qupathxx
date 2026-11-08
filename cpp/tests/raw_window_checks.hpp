#pragma once
#include "raw_pixel_feature_checks.hpp"
#include "raw_window_weight_reference.hpp"
#include "../exact_weighted_mean.hpp"
#include <type_traits>
namespace raw_window_checks {
inline void require(bool condition,const char* text) {if(!condition) throw std::runtime_error(std::string("Raw lattice window: ")+text);}
inline std::uint64_t bits(double value) {std::uint64_t raw;std::memcpy(&raw,&value,8);return raw;}
class LargeReader final:public qpx::ImageServer {
    qpx::ImageMetadata display_;
    bool closed_=false;
public:
    std::shared_ptr<qpx::RawImageMetadata> raw=std::make_shared<qpx::RawImageMetadata>();
    mutable unsigned reads=0;mutable std::size_t largest_read=0;
    unsigned mutate_after=std::numeric_limits<unsigned>::max();
    explicit LargeReader(std::int64_t size=1000000) {
        raw->width=raw->height=size;raw->levels={{size,size,1}};raw->channels=1;raw->channel_names={"quantity"};raw->sample_type=qpx::SampleType::UInt64;
        display_.width=display_.height=size;display_.levels={{size,size,1}};
    }
    static std::uint64_t value(std::int64_t x,std::int64_t y) {return std::numeric_limits<std::uint64_t>::max()-(std::uint64_t(x)*65537+std::uint64_t(y)*16777259);}
    const qpx::ImageMetadata& metadata() const override {return display_;}
    std::shared_ptr<const qpx::RawImageMetadata> raw_metadata(const std::atomic<bool>*) const override {return raw;}
    std::string reader_id() const override {return "independent-large-uint64-window";}
    qpx::PixelBuffer read_region(const qpx::RegionRequest&) const override {throw std::runtime_error("window used display IO");}
    qpx::RawPixelBuffer read_raw(const qpx::RawRegionRequest& r,const std::atomic<bool>*) const override {
        r.validate(*raw);++reads;qpx::RawPixelBuffer out(r.region.width,r.region.height,1,qpx::SampleType::UInt64);auto& values=std::get<std::vector<std::uint64_t>>(out.writable_storage());
        for(std::int64_t y=0;y<r.region.height;++y) for(std::int64_t x=0;x<r.region.width;++x) {const auto i=std::size_t(y*r.region.width+x);values[i]=value(r.region.x+x,r.region.y+y);out.writable_valid()[i]=1;}
        if(reads>mutate_after && !values.empty()) values.front()^=1;
        largest_read=std::max(largest_read,values.size()*9);return out;
    }
    void close() override {closed_=true;}bool closed() const override {return closed_;}
};
inline void run() {
    using namespace qpx;using raw_feature_checks::rejects;
    static_assert(!std::is_convertible_v<const ScientificRawWindowData&,const ScientificRawGrid&>);
    for(const auto& reference:raw_window_reference::means) {
        detail::ExactRawWindowMean mean;
        auto add=[&](std::uint64_t value,std::uint64_t weight) {if(reference.kind==0) {std::int64_t v;std::memcpy(&v,&value,8);mean.add(v,weight);}else if(reference.kind==1) mean.add(value,weight);else {double v;std::memcpy(&v,&value,8);mean.add(v,weight);}};
        add(reference.a,reference.weight_a);add(reference.b,reference.weight_b);require(bits(mean.mean())==reference.expected,"independent wide-denominator rounding");
    }
    detail::ExactRawWindowMean limit;for(unsigned i=0;i<256;++i) limit.add(std::numeric_limits<double>::max(),std::numeric_limits<std::uint64_t>::max());require(limit.mean()==std::numeric_limits<double>::max(),"72-bit numerator capacity");rejects<std::length_error>([&]{limit.add(1.,std::numeric_limits<std::uint64_t>::max());});require(limit.mean()==std::numeric_limits<double>::max(),"weight refusal changed owned mean");
    rejects<std::length_error>([]{detail::ExactRawWindowMean::validate_footprint(1ULL<<36,1ULL<<36);});detail::ExactRawWindowMean::validate_footprint((1ULL<<36)-1,(1ULL<<36)-1);
    rejects<std::invalid_argument>([]{detail::ExactRawWindowMean empty;empty.mean();});
    LargeReader large;const ScientificRawWindow window(1000000,1000000,{-1,999983,1000000,1},{2,999979,1000000,1},543210,123456,3,2);
    require(window.x().edge(543210).numerator()==54319976543 && window.x().edge(543210).denominator()==100000,"fractional global origin was rebased");
    const auto sampled=read_scientific_raw_window(large,window);require(sampled.width()==5 && sampled.height()==4 && sampled.values().size()==20,"window capture dimensions");
    for(std::size_t i=0;i<20;++i) require(sampled.valid()[i] && bits(sampled.values()[i])==raw_window_reference::large_window[i],"independent irreducible-lattice uint64 sample");
    require(digest_scientific_raw_window(large,window).input_sha256==sampled.input_sha256(),"hash-only/window capture identity");
    auto too_small=sampled.options();too_small.work_bytes=scientific_raw_window_work_bytes(*large.raw,window,too_small)-1;const auto before=large.reads;rejects<std::length_error>([&]{read_scientific_raw_window(large,window,too_small);});require(large.reads==before,"budget rejection performed pixel IO");
    std::atomic<bool> cancelled{true};rejects<std::runtime_error>([&]{read_scientific_raw_window(large,window,{},&cancelled);});rejects<std::runtime_error>([&]{digest_scientific_raw_window(large,window,{},&cancelled);});require(large.reads==before,"pre-cancel performed pixel IO");
    rejects<std::out_of_range>([&]{window.x().edge(0);});rejects<std::invalid_argument>([]{ScientificRawWindow bad(7,5,{0,6,7},{0,5,6},-1,0,1,1);});rejects<std::invalid_argument>([]{ScientificRawWindow bad(7,5,{0,6,7},{0,5,6},6,0,2,1);});
    rejects<std::length_error>([]{ScientificGrid full(1000000,1000000,{0,1000000,1000000},{0,1000000,1000000});});
    const ScientificRawWindow dense(1000000,1000000,{0,1000000,1000000},{0,1000000,1000000},500000,500000,4,3);const auto exact=read_scientific_raw_window(large,dense);require(exact.values().size()==12,"dense huge-lattice window");for(std::int64_t y=0;y<3;++y) for(std::int64_t x=0;x<4;++x) require(exact.values()[std::size_t(y*4+x)]==static_cast<double>(LargeReader::value(500000+x,500000+y)),"reduced dense integer weights");
    const ScientificRawWindow outside(1000000,1000000,{-100,4,4},{-100,4,4},0,0,2,2);const auto no_reads=large.reads;const auto unsupported=read_scientific_raw_window(large,outside);require(large.reads==no_reads,"outside window decoded source pixels");for(std::size_t i=0;i<4;++i) require(!unsupported.valid()[i] && unsupported.reasons()[i]==std::uint8_t(ScientificRawReason::OutsideSource) && bits(unsupported.values()[i])==0,"outside support precedence");
    // Exercise the existing bounded raw chunk commitments, including mutation
    // after capture but before reread for area-weighted materialization.
    LargeReader coarse(1024);const ScientificRawWindow block(1024,1024,{0,1024,16},{0,1024,16},3,5,4,4);const auto bounded=read_scientific_raw_window(coarse,block);require(coarse.largest_read<=65536 && coarse.reads>10 && bounded.values().size()==16,"coarse chunk reuse/admission");require(digest_scientific_raw_window(coarse,block).input_sha256==bounded.input_sha256(),"coarse digest grammar");
    LargeReader changing(1024);changing.mutate_after=10;rejects<std::runtime_error>([&]{read_scientific_raw_window(changing,block);});
    // Compare core values and feature bits at every tile seam with the already
    // independently qualified full-grid path, across all twelve dtype cases.
    std::size_t slots=0;
    for(unsigned which=0;which<12;++which) {
        raw_feature_checks::Reader reader(which);const ScientificGrid global(7,5,{0,6,7,4},{0,5,6,4});ScientificRawOptions selection;selection.channels=2;
        const auto full=read_scientific_raw(reader,global,selection);RawPixelFeatureOptions features;features.offsets={raw_feature_reference::cases[which].offsets[0],raw_feature_reference::cases[which].offsets[1]};features.scales={raw_feature_reference::cases[which].scales[0],raw_feature_reference::cases[which].scales[1]};features.sigmas={{0,0},{0,.5},{1,.5}};features.gradient_magnitude=features.laplacian=true;
        const auto expected=detail::extract_raw_pixel_features(full,features);const auto columns=expected->features.size();
        for(std::int64_t y=0;y<6;y+=2) for(std::int64_t x=0;x<7;x+=3) {
            const ScientificRawWindow tile(global,x,y,std::min<std::int64_t>(3,7-x),2);const auto captured=read_scientific_raw_window(reader,tile,selection);const auto actual=detail::extract_raw_pixel_features(captured,features);
            require(actual->width==tile.core_width() && actual->height==2 && actual->features.size()==columns,"feature core/schema shape");
            for(std::size_t c=0;c<columns;++c) require(actual->features[c].name==expected->features[c].name && actual->features[c].unit==expected->features[c].unit,"feature schema/order seam");
            for(std::int64_t yy=0;yy<captured.height();++yy) for(std::int64_t xx=0;xx<captured.width();++xx) for(std::size_t c=0;c<2;++c) {const auto a=(std::size_t(yy)*std::size_t(captured.width())+std::size_t(xx))*2+c,b=(std::size_t(y+yy)*std::size_t(full.width())+std::size_t(x+xx))*2+c;require(captured.valid()[a]==full.valid()[b] && captured.reasons()[a]==full.reasons()[b] && bits(captured.values()[a])==bits(full.values()[b]),"raw context support/value seam");}
            for(std::int64_t yy=0;yy<2;++yy) for(std::int64_t xx=0;xx<tile.core_width();++xx) {const auto a=std::size_t(yy*tile.core_width()+xx),b=std::size_t((y+yy)*7+x+xx);require(actual->rows_valid[a]==expected->rows_valid[b],"feature row support seam");for(std::size_t c=0;c<columns;++c) {require(actual->valid[a*columns+c]==expected->valid[b*columns+c] && raw_feature_checks::bits(actual->values[a*columns+c])==raw_feature_checks::bits(expected->values[b*columns+c]),"feature float32 bit seam");++slots;}}
        }
    }
    require(slots==9072,"feature slot census");
    std::cout<<"Raw lattice windows: 13 Fraction wide means, 20 large irreducible uint64 samples, huge dense lattice, bounded chunk mutation, admission/cancel and 9072 exact tiled feature slots passed\n";
}
}

#pragma once
#include "../workflow_input.hpp"
#include "qupathxx/fluorescence_cells.hpp"
#include "../vendor/lodepng/lodepng.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <map>
#include <numeric>
#include <optional>
namespace fluorescence_evaluation {
struct Labels {unsigned width=0,height=0;std::vector<std::uint32_t> pixels;};
inline Labels equal_value_components(unsigned width,unsigned height,const std::vector<unsigned char>& rgba){
    const auto n=std::size_t(width)*height;if(!width || !height || n>2000000 || rgba.size()!=4*n)throw std::invalid_argument("Evaluation RGBA shape/limit");
    Labels result{width,height,std::vector<std::uint32_t>(n)};std::vector<std::size_t> pending;std::uint32_t label=0;
    for(std::size_t at=0;at<n;++at){if(!rgba[4*at] || result.pixels[at])continue;if(label==10000)throw std::length_error("Evaluation instance limit");
        ++label;result.pixels[at]=label;pending={at};
        while(!pending.empty()){const auto i=pending.back();pending.pop_back();const auto x=int(i%width),y=int(i/width);
            for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){const auto xx=x+dx,yy=y+dy;if(xx<0 || yy<0 || xx>=int(width) || yy>=int(height))continue;
                const auto j=std::size_t(yy)*width+unsigned(xx);if(!result.pixels[j] && rgba[4*j]==rgba[4*at]){result.pixels[j]=label;pending.push_back(j);}}
        }
    }return result;
}
inline Labels annotation(const qpx::detail::WorkflowInput& input){
    const auto bytes=qpx::detail::read_workflow_input(input);LodePNGState state;lodepng_state_init(&state);
    struct StateCleanup {LodePNGState* state;~StateCleanup(){lodepng_state_cleanup(state);}} cleanup{&state};state.decoder.color_convert=0;
    unsigned width=0,height=0;const auto* data=reinterpret_cast<const unsigned char*>(bytes.data());auto code=lodepng_inspect(&width,&height,&state,data,bytes.size());
    if(code || state.info_png.color.colortype!=LCT_RGBA || state.info_png.color.bitdepth!=8)throw std::invalid_argument("Evaluation requires strict RGBA8 PNG, without conversion");
    const auto n=std::uint64_t(width)*height;if(!n || n>2000000)throw std::length_error("Evaluation PNG pixel limit");state.decoder.zlibsettings.max_output_size=std::size_t(5*n);
    unsigned char* raw=nullptr;unsigned w=0,h=0;code=lodepng_decode(&raw,&w,&h,&state,data,bytes.size());std::unique_ptr<unsigned char,decltype(&std::free)> owned(raw,&std::free);
    if(code)throw std::invalid_argument(std::string("Evaluation PNG: ")+lodepng_error_text(code));
    if(w!=width || h!=height || state.info_raw.colortype!=LCT_RGBA || state.info_raw.bitdepth!=8)throw std::runtime_error("Evaluation PNG layout changed");
    input.verify();return equal_value_components(w,h,{raw,raw+std::size_t(4*n)});
}
inline std::vector<std::uint64_t> areas(const Labels& labels){
    if(!labels.width || !labels.height || std::uint64_t(labels.width)*labels.height>2000000 || labels.pixels.size()!=std::size_t(labels.width)*labels.height)throw std::invalid_argument("Evaluation labels shape/limit");
    const auto maximum=*std::max_element(labels.pixels.begin(),labels.pixels.end());if(maximum>10000)throw std::length_error("Evaluation label limit");
    std::vector<std::uint64_t> counts(std::size_t(maximum)+1);for(const auto id:labels.pixels)++counts[id];return counts;
}
struct Match {std::uint32_t reference=0,prediction=0;double iou=0;};
struct Metrics {
    std::size_t references=0,predictions=0,true_positive=0,false_positive=0,false_negative=0,splits=0,merges=0,reference_border=0,prediction_border=0;
    double dice=0,precision=0,recall=0,f1=0,split_rate=0,merge_rate=0;
    std::optional<double> mean_iou;std::vector<Match> matches;
};
inline Metrics compare(const Labels& reference,const Labels& prediction){
    const auto a=areas(reference),b=areas(prediction);if(reference.width!=prediction.width || reference.height!=prediction.height)throw std::invalid_argument("Evaluation axes differ");
    Metrics out;out.references=std::count_if(a.begin()+1,a.end(),[](auto n){return n!=0;});out.predictions=std::count_if(b.begin()+1,b.end(),[](auto n){return n!=0;});
    std::map<std::pair<std::uint32_t,std::uint32_t>,std::uint64_t> overlap;std::uint64_t common=0,total=0;
    std::vector<bool> border_a(a.size()),border_b(b.size());
    for(std::size_t i=0;i<reference.pixels.size();++i){const auto x=reference.pixels[i],y=prediction.pixels[i];total+=bool(x)+bool(y);if(x && y){++common;++overlap[{x,y}];}
        const auto xx=i%reference.width,yy=i/reference.width;if(!xx || !yy || xx+1==reference.width || yy+1==reference.height){if(x)border_a[x]=true;if(y)border_b[y]=true;}}
    out.reference_border=std::count(border_a.begin(),border_a.end(),true);out.prediction_border=std::count(border_b.begin(),border_b.end(),true);out.dice=total?2.*common/total:1;
    std::vector<std::vector<std::uint32_t>> edges(a.size());std::vector<unsigned> fragments(a.size()),merged(b.size());
    for(const auto& [pair,n]:overlap){const auto [x,y]=pair;if(3*n>=a[x]+b[y])edges[x].push_back(y);if(10*n>=a[x])++fragments[x];if(10*n>=b[y])++merged[y];}
    out.splits=std::count_if(fragments.begin()+1,fragments.end(),[](auto n){return n>=2;});out.merges=std::count_if(merged.begin()+1,merged.end(),[](auto n){return n>=2;});
    std::vector<std::uint32_t> owner(b.size());std::vector<bool> visited(b.size());
    std::function<bool(std::uint32_t)> augment=[&](std::uint32_t x){for(const auto y:edges[x])if(!visited[y]){visited[y]=true;if(!owner[y] || augment(owner[y])){owner[y]=x;return true;}}return false;};
    for(std::uint32_t x=1;x<a.size();++x){std::fill(visited.begin(),visited.end(),false);augment(x);}
    double iou_sum=0;for(std::uint32_t y=1;y<owner.size();++y)if(const auto x=owner[y]){const auto intersection=overlap.at({x,y});const auto iou=double(intersection)/(a[x]+b[y]-intersection);out.matches.push_back({x,y,iou});iou_sum+=iou;}
    out.true_positive=out.matches.size();out.false_positive=out.predictions-out.true_positive;out.false_negative=out.references-out.true_positive;
    out.precision=out.predictions?double(out.true_positive)/out.predictions:(out.references?0:1);out.recall=out.references?double(out.true_positive)/out.references:(out.predictions?0:1);
    out.f1=out.references+out.predictions?2.*out.true_positive/(out.references+out.predictions):1;if(out.true_positive)out.mean_iou=iou_sum/out.true_positive;
    out.split_rate=out.references?double(out.splits)/out.references:0;out.merge_rate=out.predictions?double(out.merges)/out.predictions:0;return out;
}
inline std::optional<double> median(std::vector<double> values){if(values.empty())return {};std::sort(values.begin(),values.end());const auto n=values.size();return n%2?values[n/2]:(values[n/2-1]+values[n/2])/2;}
struct Samples {
    std::uint64_t count=0,sum=0;std::vector<std::uint16_t> values;
    void add(std::uint16_t value){++count;sum+=value;values.push_back(value);}
    double mean()const{return double(sum)/count;}
    double sd()const{long double centered=0;const auto mu=(long double)sum/count;for(const auto value:values){const auto delta=value-mu;centered+=delta*delta;}return std::sqrt(double(centered/(count-1)));}
};
inline void verify_samples(const Samples& expected,const qpx::RawCellCompartment& actual,double tolerance){
    const auto close=[&](double x,double y){if(!std::isfinite(x) || !std::isfinite(y) || std::abs(x-y)>tolerance*std::max(1.,std::abs(y)))throw std::runtime_error("Native raw measurement differs from independent original-sample oracle");};
    if(actual.pixels!=expected.count || actual.valid_pixels!=expected.count || actual.channels.size()!=1)throw std::runtime_error("Native raw compartment count differs from oracle");
    const auto& c=actual.channels[0];if(c.finite_samples!=expected.count || c.nonfinite_samples)throw std::runtime_error("Native raw validity differs from oracle");
    if(!expected.count){if(c.mean || c.sum)throw std::runtime_error("Empty raw compartment has an invented measurement");return;}
    if(!c.sum || !c.mean)throw std::runtime_error("Native raw moment is absent");close(*c.sum,double(expected.sum));close(*c.mean,expected.mean());
    if(expected.count>1){if(!c.sample_standard_deviation)throw std::runtime_error("Native sample SD absent");close(*c.sample_standard_deviation,expected.sd());}
    else if(c.sample_standard_deviation)throw std::runtime_error("Singleton has an invented sample SD");
}
}

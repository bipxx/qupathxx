#include "qupathxx/fluorescence_cells.hpp"
#include "fluorescence_cell_internal.hpp"
#include "qupathxx/workflow.hpp"
#include "intensity_moments.hpp"
#include "scientific_cell_recipe.hpp"
#include "sha256.hpp"
#include <cstring>
#include <type_traits>

namespace qpx {
namespace {
void budget(std::size_t work) {if(!work || work>1024ULL*1024*1024)throw std::invalid_argument("Invalid fluorescence cell work budget");}
std::size_t lattice(const RawPixelBuffer& pixels) {
    const auto n=std::size_t(pixels.width())*std::size_t(pixels.height());
    if(pixels.valid().size()!=n || pixels.storage().index()!=std::size_t(pixels.sample_type()) ||
        std::visit([](const auto& values){return values.size();},pixels.storage())!=n*pixels.channels())throw std::invalid_argument("Malformed raw cell sample lattice/type");
    return n;
}
template<class T> double value(T x) {
    if constexpr(std::is_integral_v<T> && sizeof(T)==8) {
        if constexpr(std::is_signed_v<T>) {if(x<-(std::int64_t(1)<<53) || x>(std::int64_t(1)<<53))throw std::invalid_argument("Raw cell intensity exceeds exact binary64 integer admission");}
        else if(x>(std::uint64_t(1)<<53))throw std::invalid_argument("Raw cell intensity exceeds exact binary64 integer admission");
    }
    return double(x);
}
struct Accumulator {
    detail::IntensityMoment moments;
    std::uint64_t finite=0,nonfinite=0;
    void add(double v) {if(std::isfinite(v))moments.add(v,++finite,"raw cell intensity");else ++nonfinite;}
    RawChannelIntensity finish() const {
        RawChannelIntensity result;result.finite_samples=finite;result.nonfinite_samples=nonfinite;
        if(finite) {result.sum=moments.sum;result.mean=moments.mean;result.minimum=moments.minimum;result.maximum=moments.maximum;
            if(finite>1)result.sample_standard_deviation=std::sqrt(std::max(0.,moments.m2)/double(finite-1));}
        return result;
    }
};
std::size_t measurement_bytes(std::size_t cells,std::uint32_t channels) {
    constexpr auto base=sizeof(RawCellMeasurements)+3*sizeof(RawCellCompartment);
    const auto each=base+3ULL*channels*(sizeof(Accumulator)+sizeof(RawChannelIntensity));
    if(cells>1024ULL*1024*1024/each)throw WorkflowCaptureLimit("Raw cell measurements exceed maximum work budget");
    return std::size_t(cells*each);
}
void text(Sha256& sha,const std::string& s) {sha.integer(s.size());sha.update(s.data(),s.size());}
void real(Sha256& sha,double x) {std::uint64_t bits;std::memcpy(&bits,&x,8);sha.integer(bits);}
std::string input_hash(const RawPixelBuffer& pixels,const std::vector<std::uint8_t>& domain,const std::atomic<bool>* cancelled) {
    Sha256 sha;text(sha,"qpx.fluorescence-cell-raster/1");sha.integer(pixels.width());sha.integer(pixels.height());sha.integer(pixels.channels());sha.integer(std::uint64_t(pixels.sample_type()));
    sha.update(pixels.valid().data(),pixels.valid().size());sha.update(domain.data(),domain.size());
    std::visit([&](const auto& values) {using T=typename std::decay_t<decltype(values)>::value_type;
        for(std::size_t i=0;i<values.size();++i) {if((i&4095)==0)workflow_checkpoint(cancelled);
            if constexpr(std::is_integral_v<T>)sha.integer(std::uint64_t(values[i]));
            else if constexpr(sizeof(T)==4) {std::uint32_t bits;std::memcpy(&bits,&values[i],4);sha.integer(bits);}
            else real(sha,values[i]);}
    },pixels.storage());return sha.finish();
}
std::string recipe_hash(const FluorescenceCellOptions& p,const std::array<double,2>& metric) {
    Sha256 sha;text(sha,"qpx.fluorescence-threshold-distance-cells/1");sha.integer(p.segmentation_channel);sha.integer(std::uint64_t(p.threshold_method));
    for(auto x:{p.threshold,p.sigma,p.peak_prominence,p.min_nucleus_area,p.cell_expansion})real(sha,x);
    sha.integer(bool(p.max_nucleus_area));if(p.max_nucleus_area)real(sha,*p.max_nucleus_area);sha.integer(std::uint64_t(p.units));
    for(auto x:metric)real(sha,x);sha.integer(p.max_cells);sha.integer(p.work_bytes);return sha.finish();
}
double otsu(const std::vector<double>& values,const std::vector<std::uint8_t>& domain,const std::atomic<bool>* cancelled) {
    double lo=0,hi=0;std::uint64_t count=0;
    for(std::size_t i=0;i<values.size();++i) {if((i&4095)==0)workflow_checkpoint(cancelled);if(domain[i]) {if(!count)lo=hi=values[i];else {lo=std::min(lo,values[i]);hi=std::max(hi,values[i]);}++count;}}
    if(!count)throw std::invalid_argument("No finite fluorescence segmentation pixels in ROI");if(lo==hi)return lo;
    const auto scale=std::max(std::abs(lo),std::abs(hi)),lower=lo/scale,range=hi/scale-lower;
    std::array<std::uint64_t,256> bins{};
    for(std::size_t i=0;i<values.size();++i) {if((i&4095)==0)workflow_checkpoint(cancelled);if(domain[i])++bins[std::size_t(std::clamp((values[i]/scale-lower)/range*256,0.,255.))];}
    return detail::fluorescence_otsu_threshold(lo,hi,count,bins);
}
}
double detail::fluorescence_otsu_threshold(double lo,double hi,std::uint64_t count,const std::array<std::uint64_t,256>& bins) {
    if(!count)throw std::invalid_argument("No finite fluorescence segmentation pixels in ROI");
    if(lo==hi)return lo;
    double total=0;for(unsigned i=0;i<256;++i)total+=i*double(bins[i]);
    double sum=0,best=-1;std::uint64_t before=0;unsigned split=0;
    for(unsigned i=0;i<255;++i) {before+=bins[i];sum+=i*double(bins[i]);if(!before || before==count)continue;
        const auto delta=sum/double(before)-(total-sum)/double(count-before);const auto variance=(double(before)/count)*(double(count-before)/count)*delta*delta;
        if(variance>best){best=variance;split=i;}}
    const double fraction=double(split+1)/256;return lo*(1-fraction)+hi*fraction;
}
namespace {
void cut_components(const std::vector<std::uint8_t>& foreground,const std::vector<std::uint8_t>& eligible,
    std::size_t width,std::size_t height,std::size_t rx,std::size_t ry,std::vector<std::uint8_t>& cut,const std::atomic<bool>* cancelled) {
    cut.assign(foreground.size(),0);std::vector<std::size_t> queue;queue.reserve(foreground.size());
    for(std::size_t start=0;start<foreground.size();++start) {
        if(!(start&4095))workflow_checkpoint(cancelled);
        if(!foreground[start] || cut[start])continue;
        queue.clear();queue.push_back(start);cut[start]=1;bool open=false;
        for(std::size_t at=0;at<queue.size();++at) {
            if(!(at&4095))workflow_checkpoint(cancelled);
            const auto i=queue[at],x=i%width,y=i/width;
            open=open || x<rx || y<ry || width-1-x<rx || height-1-y<ry || !x || !y || x+1==width || y+1==height;
            for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
                if(!dx && !dy)continue;
                if((dx<0 && !x)||(dy<0 && !y)||(dx>0 && x+1==width)||(dy>0 && y+1==height)){open=true;continue;}
                const auto j=std::size_t(std::int64_t(y)+dy)*width+std::size_t(std::int64_t(x)+dx);
                if(!eligible[j])open=true;
                if(foreground[j] && !cut[j]){cut[j]=1;queue.push_back(j);}
            }
        }
        if(open)for(auto i:queue)cut[i]=2;
    }
    for(auto& value:cut)value=value==2?1:0;
}
}
std::vector<RawCellMeasurements> measure_raw_cell_compartments(const RawPixelBuffer& pixels,const std::vector<std::uint32_t>& nuclei,
    const std::vector<std::uint32_t>& bodies,std::size_t cells,std::size_t work,const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);budget(work);const auto n=lattice(pixels);const auto channels=pixels.channels();
    if(nuclei.size()!=n || bodies.size()!=n || cells>1000000)throw std::invalid_argument("Invalid raw cell label lattice/count");
    std::size_t bytes=4096;workflow_capture_charge(bytes,pixels.payload_bytes(),work);
    workflow_capture_charge(bytes,nuclei.capacity()*sizeof(std::uint32_t),work);workflow_capture_charge(bytes,bodies.capacity()*sizeof(std::uint32_t),work);
    workflow_capture_charge(bytes,measurement_bytes(cells,channels),work);
    std::vector<RawCellMeasurements> result(cells);std::vector<Accumulator> accumulators(cells*3*channels);
    for(std::size_t c=0;c<cells;++c) {result[c].label=std::uint32_t(c+1);result[c].nucleus.channels.resize(channels);result[c].cell.channels.resize(channels);}
    std::visit([&](const auto& samples) {
        for(std::size_t i=0;i<n;++i) {if((i&4095)==0)workflow_checkpoint(cancelled);
            const auto a=nuclei[i],b=bodies[i];if(a>cells || b>cells || (a && a!=b) || pixels.valid()[i]>1)throw std::invalid_argument("Inconsistent raw cell labels/validity");
            if(!b)continue;auto& cell=result[b-1];++cell.cell.pixels;
            if(!a && !cell.cytoplasm)cell.cytoplasm.emplace();
            auto& compartment=a?cell.nucleus:*cell.cytoplasm;
            ++compartment.pixels;if(!pixels.valid()[i])continue;++cell.cell.valid_pixels;++compartment.valid_pixels;
            const auto base=std::size_t(b-1)*3*channels;
            for(std::size_t c=0;c<channels;++c) {const auto v=value(samples[i*channels+c]);accumulators[base+channels+c].add(v);accumulators[base+(a?0:2*channels)+c].add(v);}
        }
    },pixels.storage());
    for(std::size_t i=0;i<cells;++i) {workflow_checkpoint(cancelled);auto& cell=result[i];if(!cell.nucleus.pixels)throw std::invalid_argument("Raw cell label lacks a nucleus");
        if(cell.cytoplasm)cell.cytoplasm->channels.resize(channels);
        for(std::size_t c=0;c<channels;++c) {cell.nucleus.channels[c]=accumulators[i*3*channels+c].finish();cell.cell.channels[c]=accumulators[i*3*channels+channels+c].finish();
            if(cell.cytoplasm)cell.cytoplasm->channels[c]=accumulators[i*3*channels+2*channels+c].finish();}}
    workflow_checkpoint(cancelled);return result;
}
void FluorescenceCellOptions::validate() const {
    budget(work_bytes);if((threshold_method!=FluorescenceThreshold::Explicit && threshold_method!=FluorescenceThreshold::Otsu256) ||
        (units!=FluorescenceCellUnits::Pixels && units!=FluorescenceCellUnits::Micrometres))throw std::invalid_argument("Unknown fluorescence cell policy");
    if(!std::isfinite(threshold) || !std::isfinite(sigma) || sigma<0 || sigma>1000000 || !std::isfinite(peak_prominence) || peak_prominence<=0 || peak_prominence>1000000 ||
        !std::isfinite(cell_expansion) || cell_expansion<0 || cell_expansion>1000000 || !std::isfinite(min_nucleus_area) || min_nucleus_area<=0 ||
        (max_nucleus_area && (!std::isfinite(*max_nucleus_area) || *max_nucleus_area<min_nucleus_area)) || !max_cells || max_cells>1000000)
        throw std::invalid_argument("Invalid fluorescence cell parameters");
}
std::size_t detail::fluorescence_cell_work_bytes(std::int64_t width,std::int64_t height,std::uint32_t channels,SampleType type,
    const FluorescenceCellOptions& p,std::optional<std::array<double,2>> calibration,std::size_t raw_bytes,std::size_t domain_bytes,std::size_t fixed) {
    p.validate();if(width<=0 || height<=0 || !channels || channels>65536)throw std::invalid_argument("Invalid fluorescence raster dimensions");
    const auto unit=std::uint64_t(channels)*sample_bytes(type)+1;
    if(std::uint64_t(width)>256ULL*1024*1024/unit || std::uint64_t(height)>256ULL*1024*1024/(std::uint64_t(width)*unit))throw WorkflowCaptureLimit("Fluorescence raw raster exceeds buffer admission");
    const auto n=std::size_t(width)*std::size_t(height);
    if(p.units==FluorescenceCellUnits::Micrometres && !calibration)throw std::invalid_argument("Physical fluorescence cells require both calibration axes");
    const auto spacing=p.units==FluorescenceCellUnits::Pixels?std::array<double,2>{1,1}:*calibration;
    const cell_detail::PhysicalGridMetric metric(spacing[0],spacing[1]);const auto sigmas=metric.sigma_pixels(p.sigma);
    const auto rx=cell_detail::scientific_gaussian_radius(sigmas.first),ry=cell_detail::scientific_gaussian_radius(sigmas.second);
    std::size_t bytes=65536;workflow_capture_charge(bytes,fixed,p.work_bytes);
    workflow_capture_charge(bytes,raw_bytes?raw_bytes:std::size_t(n*unit),p.work_bytes);
    workflow_capture_charge(bytes,domain_bytes?domain_bytes:n,p.work_bytes);
    workflow_capture_charge(bytes,256ULL*n+40ULL*std::max(width,height)+8ULL*(2*rx+2*ry+2),p.work_bytes);
    workflow_capture_charge(bytes,measurement_bytes(std::min(n,p.max_cells),channels),p.work_bytes);return bytes;
}
std::string detail::fluorescence_input_sha256(const RawPixelBuffer& pixels,const std::vector<std::uint8_t>& domain,const std::atomic<bool>* cancelled){return input_hash(pixels,domain,cancelled);}
detail::FluorescenceScores detail::fluorescence_scores(const RawPixelBuffer& pixels,const std::vector<std::uint8_t>& domain,FluorescenceCellOptions p,
    std::optional<std::array<double,2>> calibration,std::size_t fixed,const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);p.validate();const auto n=lattice(pixels);const auto width=std::size_t(pixels.width()),height=std::size_t(pixels.height());
    if(domain.size()!=n || p.segmentation_channel>=pixels.channels())throw std::invalid_argument("Invalid fluorescence segmentation domain/channel");
    auto spacing=std::array<double,2>{1,1};
    if(p.units==FluorescenceCellUnits::Micrometres) {if(!calibration)throw std::invalid_argument("Physical fluorescence cells require both calibration axes");spacing=*calibration;}
    const cell_detail::PhysicalGridMetric metric(spacing[0],spacing[1]);
    fluorescence_cell_work_bytes(pixels.width(),pixels.height(),pixels.channels(),pixels.sample_type(),p,calibration,pixels.payload_bytes(),domain.capacity(),fixed);
    FluorescenceScores result;result.values.resize(n);result.eligible.resize(n);std::vector<std::uint8_t> valid(n);
    std::visit([&](const auto& samples) {for(std::size_t i=0;i<n;++i) {if((i&4095)==0)workflow_checkpoint(cancelled);
        if(domain[i]>1 || pixels.valid()[i]>1)throw std::invalid_argument("Fluorescence domain/validity must be binary");
        if(!pixels.valid()[i]){result.invalid+=domain[i];continue;}const auto v=value(samples[i*pixels.channels()+p.segmentation_channel]);
        valid[i]=std::isfinite(v);result.values[i]=valid[i]?v:0;result.eligible[i]=domain[i] && valid[i];
        result.finite+=result.eligible[i];result.nonfinite+=domain[i] && !valid[i];}
    },pixels.storage());
    if(result.finite)cell_detail::physical_gaussian(result.values,valid,width,height,metric,p.sigma,p.work_bytes,cancelled);
    return result;
}
FluorescenceCellResult detail::segment_fluorescence_cells_reserved(const RawPixelBuffer& pixels,const std::vector<std::uint8_t>& domain,FluorescenceCellOptions p,
    std::optional<std::array<double,2>> calibration,std::size_t fixed,const std::atomic<bool>* cancelled,std::vector<std::uint8_t>* cut_foreground) {
    auto prepared=fluorescence_scores(pixels,domain,p,calibration,fixed,cancelled);
    const auto n=prepared.values.size(),width=std::size_t(pixels.width()),height=std::size_t(pixels.height());
    auto& score=prepared.values;auto& eligible=prepared.eligible;std::vector<std::uint8_t> foreground(n);
    FluorescenceCellResult result;result.width=pixels.width();result.height=pixels.height();result.options=p;
    if(p.units==FluorescenceCellUnits::Micrometres)result.metric=*calibration;
    const cell_detail::PhysicalGridMetric metric(result.metric[0],result.metric[1]);
    result.segmentation_finite_pixels=prepared.finite;result.segmentation_nonfinite_pixels=prepared.nonfinite;result.invalid_pixels=prepared.invalid;
    if(!result.segmentation_finite_pixels) {
        if(!cut_foreground || p.threshold_method!=FluorescenceThreshold::Explicit)throw std::invalid_argument("No finite fluorescence segmentation pixels in ROI");
        result.nuclei.assign(n,0);result.bodies.assign(n,0);cut_foreground->assign(n,0);result.resolved_threshold=p.threshold;
        result.input_sha256=input_hash(pixels,domain,cancelled);result.recipe_sha256=recipe_hash(p,result.metric);return result;
    }
    try {
        result.resolved_threshold=p.threshold_method==FluorescenceThreshold::Explicit?p.threshold:otsu(score,eligible,cancelled);
        std::vector<double> distance(n);
        for(std::size_t i=0;i<n;++i) {if((i&4095)==0)workflow_checkpoint(cancelled);foreground[i]=eligible[i] && score[i]>result.resolved_threshold;distance[i]=foreground[i]?std::numeric_limits<double>::infinity():0;}
        if(cut_foreground) {
            const auto sigmas=metric.sigma_pixels(p.sigma);
            cut_components(foreground,eligible,width,height,cell_detail::scientific_gaussian_radius(sigmas.first),cell_detail::scientific_gaussian_radius(sigmas.second),*cut_foreground,cancelled);
        }
        cell_detail::physical_distance(distance,width,height,metric,true,nullptr,p.work_bytes,cancelled);
        for(auto& v:distance)v=std::sqrt(v);
        const auto first=[&](auto predicate) {std::uint64_t lo=1,hi=n+1;while(lo<hi) {const auto mid=lo+(hi-lo)/2;const auto area=detail::calibrated_cell_product({double(mid),result.metric[0],result.metric[1]},true);if(predicate(area))hi=mid;else lo=mid+1;}return lo;};
        const auto minimum=first([&](double a){return a>=p.min_nucleus_area;}),maximum=p.max_nucleus_area?first([&](double a){return a>*p.max_nucleus_area;})-1:n;
        cell_detail::LogParameters split;split.sigma=0;split.threshold=0;split.prominence=p.peak_prominence;split.minimum_radius=split.minimum_spacing=0;
        split.minimum_area=minimum;split.maximum_area=maximum;split.maximum_cells=p.max_cells;split.work_bytes=p.work_bytes;
        if(minimum>maximum)result.nuclei.resize(n);
        // Foreground was already thresholded. The shared splitter's regional
        // mean gate must not threshold smoothed intensities a second time.
        else result.nuclei=cell_detail::log_split_regions(foreground,eligible,foreground,distance,width,height,split,cancelled,nullptr,result.metric[0],result.metric[1]).labels;
        result.bodies=cell_detail::expand_grid_cells(result.nuclei,eligible,width,height,metric,p.cell_expansion,p.work_bytes,cancelled).labels;
    }catch(...) {workflow_checkpoint(cancelled);throw;}
    const auto count=*std::max_element(result.nuclei.begin(),result.nuclei.end());
    result.measurements=measure_raw_cell_compartments(pixels,result.nuclei,result.bodies,count,p.work_bytes,cancelled);result.boundaries.resize(count);
    for(std::size_t i=0;i<n;++i) {if((i&4095)==0)workflow_checkpoint(cancelled);if(!result.bodies[i])continue;
        const auto x=i%width,y=i/width;bool edge=!x || !y || x+1==width || y+1==height;
        cell_detail::expansion_neighbors(i,width,height,[&](std::size_t j){edge=edge || !eligible[j];});
        if(edge) {result.boundaries[result.bodies[i]-1].cell=true;if(result.nuclei[i])result.boundaries[result.nuclei[i]-1].nucleus=true;}}
    result.input_sha256=input_hash(pixels,domain,cancelled);result.recipe_sha256=recipe_hash(p,result.metric);workflow_checkpoint(cancelled);return result;
}
FluorescenceCellResult segment_fluorescence_cells(const RawPixelBuffer& pixels,const std::vector<std::uint8_t>& domain,FluorescenceCellOptions p,
    std::optional<std::array<double,2>> calibration,const std::atomic<bool>* cancelled) {
    return detail::segment_fluorescence_cells_reserved(pixels,domain,p,calibration,0,cancelled);
}
}

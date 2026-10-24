#include "cell_archive.hpp"
#include "cell_contours.hpp"
#include "cell_numeric.hpp"
#include "cell_recipe.hpp"
#include "cell_expansion.hpp"
#include "sha256.hpp"
#include <cstring>
#include <limits>
#include <stdexcept>
namespace qpx {
namespace {
using cell_detail::checkpoint;
struct Writer {
    std::vector<std::uint8_t> bytes={'Q','C','L',1};
    void u32(std::uint32_t v) {for(int i=0;i<4;++i) bytes.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
    void u64(std::uint64_t v) {for(int i=0;i<8;++i) bytes.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
    void real(double v) {std::uint64_t bits;std::memcpy(&bits,&v,8);u64(bits);}
    void labels(const std::vector<std::uint32_t>& values,const std::atomic<bool>* cancelled) {
        std::size_t runs=0;std::uint32_t previous=0;
        for(std::size_t i=0;i<values.size();++i) {if((i&4095)==0) checkpoint(cancelled);if(!i || values[i]!=previous) ++runs;previous=values[i];}
        const bool rle=runs*8<values.size()*4;u64(rle);u64(rle?runs:values.size());
        if(!rle) {for(std::size_t i=0;i<values.size();++i) {if((i&4095)==0) checkpoint(cancelled);u32(values[i]);}return;}
        for(std::size_t at=0;at<values.size();) {
            checkpoint(cancelled);const auto value=values[at];auto end=at+1;
            while(end<values.size() && values[end]==value) {if((end&4095)==0) checkpoint(cancelled);++end;}
            u32(static_cast<std::uint32_t>(end-at));u32(value);at=end;
        }
    }
};
struct Reader {
    const std::uint8_t* p;std::size_t left;
    std::uint8_t byte() {if(!left) throw std::invalid_argument("truncated cell archive");--left;return *p++;}
    std::uint32_t u32() {std::uint32_t v=0;for(int i=0;i<4;++i) v|=std::uint32_t(byte())<<(8*i);return v;}
    std::uint64_t u64() {std::uint64_t v=0;for(int i=0;i<8;++i) v|=std::uint64_t(byte())<<(8*i);return v;}
    std::uint64_t limited(std::uint64_t limit) {auto v=u64();if(v>limit) throw std::invalid_argument("cell archive integer exceeds limit");return v;}
    double real() {auto bits=u64();double v;std::memcpy(&v,&bits,8);if(!std::isfinite(v)) throw std::invalid_argument("nonfinite cell archive number");return v;}
    std::vector<std::uint32_t> labels(std::size_t count,std::size_t components,const std::atomic<bool>* cancelled) {
        const bool rle=limited(1)!=0;const auto entries=limited(count);
        if((!rle && entries!=count) || (rle && !entries) || entries*(rle?8:4)>left) throw std::invalid_argument("invalid cell label payload length");
        std::vector<std::uint32_t> values(count);std::size_t at=0;std::uint32_t prior=std::numeric_limits<std::uint32_t>::max();
        for(std::uint64_t i=0;i<entries;++i) {
            if((i&4095)==0) checkpoint(cancelled);const auto n=rle?u32():1,value=u32();
            if(!n || n>count-at || value>components || (rle && value==prior)) throw std::invalid_argument("invalid cell label/run");
            const auto end=at+n;while(at<end) {checkpoint(cancelled);const auto chunk=std::min<std::size_t>(65536,end-at);std::fill_n(values.begin()+at,chunk,value);at+=chunk;}prior=value;
        }
        if(at!=count) throw std::invalid_argument("cell label runs do not fill grid");return values;
    }
};
std::string checksum(const std::uint8_t* data,std::size_t size,const Roi& roi,const std::atomic<bool>* cancelled) {
    Sha256 hash;const auto geometry=roi.wkt();hash.integer(geometry.size());hash.update(geometry.data(),geometry.size());
    for(std::size_t at=0;at<size;) {checkpoint(cancelled);const auto n=std::min<std::size_t>(65536,size-at);hash.update(data+at,n);at+=n;}return hash.finish();
}
bool hex(const std::string& s) {return s.size()==64 && std::all_of(s.begin(),s.end(),[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');});}
std::uint64_t edge(double coordinate,double scale,std::int64_t limit) {
    return static_cast<std::uint64_t>(std::max<long double>(0,std::min<long double>(static_cast<long double>(limit),std::ceil(static_cast<long double>(coordinate)/scale-.5L))));
}
}
std::vector<std::uint8_t> CellArchiveCodec::encode(const CellResult& r,const std::atomic<bool>* cancelled) {
    checkpoint(cancelled);r.parameters().validate();if(std::string(r.algorithm())!=cell_detail::algorithm_id(r.parameters().algorithm)) throw std::invalid_argument("unsupported cell archive algorithm");
    Writer w;const auto& n=*r.nuclei();const auto& p=r.parameters();
    if(p.algorithm!=CellAlgorithm::DistanceWatershed) {w.bytes[3]=p.algorithm==CellAlgorithm::SampledLogWatershedBodies?4:(p.algorithm==CellAlgorithm::LogWatershedBodies?3:2);w.u64(static_cast<std::uint64_t>(p.algorithm));w.real(p.log_prominence);w.u64(p.context_padding);}
    for(auto v:{n.object_id(),n.object_revision(),n.document_revision(),std::uint64_t(p.feature)}) w.u64(v);
    for(auto v:{p.threshold,p.sigma,p.min_peak_radius,p.min_seed_distance,p.cell_expansion}) w.real(v);
    for(auto v:{p.min_nucleus_pixels,p.max_nucleus_pixels,std::uint64_t(p.connectivity),std::uint64_t(std::int64_t(p.level)+1),std::uint64_t(p.tile_size),std::uint64_t(p.max_cells),std::uint64_t(p.work_bytes),
        std::uint64_t(p.contours.clip_to_roi),std::uint64_t(p.contours.max_components),std::uint64_t(p.contours.max_edges),std::uint64_t(p.contours.max_vertices),std::uint64_t(p.contours.work_bytes)}) w.u64(v);
    w.u64(n.level());w.real(n.downsample());const auto grid=n.level_bounds();for(auto v:{grid.x,grid.y,grid.width,grid.height}) w.u64(static_cast<std::uint64_t>(v));
    for(auto v:n.white()) w.real(v);for(const auto& vector:n.stains()) for(auto v:vector) w.real(v);
    for(auto v:{n.samples(),n.threshold_pixels(),std::uint64_t(r.raw_peaks()),std::uint64_t(r.markers().size()),std::uint64_t(r.features().size())}) w.u64(v);
    w.real(r.elapsed_ms());w.u64(n.estimated_work_bytes());w.bytes.insert(w.bytes.end(),n.input_sha256().begin(),n.input_sha256().end());
    for(const auto& m:r.markers()) {checkpoint(cancelled);w.u32(m.label);w.u32(m.result_label);w.u32(m.pixel_index);w.real(m.radius);}
    for(const auto& f:r.features()) {
        checkpoint(cancelled);w.u64(f.nucleus_pixels);w.u64(f.cell_pixels);
        for(const auto& vector:{f.nucleus_rgb,f.cell_rgb,f.nucleus_channels,f.cell_channels}) for(auto v:vector) w.real(v);
        if(f.cytoplasm_rgb) {for(auto v:*f.cytoplasm_rgb) w.real(v);for(auto v:*f.cytoplasm_channels) w.real(v);}
    }
    w.labels(n.labels(),cancelled);w.labels(r.cells()->labels(),cancelled);
    if(p.algorithm==CellAlgorithm::SampledLogWatershedBodies) {
        if(r.domain_bits_.size()!=(n.labels().size()+7)/8) throw std::invalid_argument("missing sampled cell domain");
        w.u64(r.domain_bits_.size());w.bytes.insert(w.bytes.end(),r.domain_bits_.begin(),r.domain_bits_.end());
    }
    const auto digest=checksum(w.bytes.data(),w.bytes.size(),n.roi(),cancelled);w.bytes.insert(w.bytes.end(),digest.begin(),digest.end());
    if(w.bytes.size()>256u*1024u*1024u) throw std::length_error("encoded cell archive exceeds 256 MiB");return w.bytes;
}
std::shared_ptr<const CellResult> CellArchiveCodec::decode(const std::uint8_t* data,std::size_t size,Roi roi,const ImageMetadata& metadata,const std::atomic<bool>* cancelled) {
    checkpoint(cancelled);if(!data || size<128 || size>256u*1024u*1024u) throw std::invalid_argument("invalid cell archive size");
    const std::string digest(reinterpret_cast<const char*>(data+size-64),64);
    if(!hex(digest) || checksum(data,size-64,roi,cancelled)!=digest) throw std::invalid_argument("cell archive checksum mismatch");
    Reader d{data,size-64};if(d.byte()!='Q' || d.byte()!='C' || d.byte()!='L') throw std::invalid_argument("unsupported cell archive encoding");
    const auto version=d.byte();if(version<1 || version>4) throw std::invalid_argument("unsupported cell archive encoding");
    auto result=std::shared_ptr<CellResult>(new CellResult());auto& p=result->parameters_;
    if(version>=2) {
        const auto algorithm=d.u64();if(algorithm!=std::uint64_t(version-1)) throw std::invalid_argument("unsupported cell archive algorithm/version");
        p.algorithm=static_cast<CellAlgorithm>(algorithm);p.log_prominence=d.real();p.context_padding=static_cast<std::uint32_t>(d.limited(4096));
    }
    result->algorithm_=cell_detail::algorithm_id(p.algorithm);
    const auto object=d.u64(),object_revision=d.u64(),document_revision=d.u64();
    if(!object || object_revision>document_revision) throw std::invalid_argument("invalid cell archive identity");
    p.feature=static_cast<TissueFeature>(d.limited(3));p.threshold=d.real();p.sigma=d.real();p.min_peak_radius=d.real();p.min_seed_distance=d.real();p.cell_expansion=d.real();
    p.min_nucleus_pixels=d.u64();p.max_nucleus_pixels=d.u64();p.connectivity=static_cast<int>(d.limited(8));
    p.level=static_cast<std::int32_t>(std::int64_t(d.limited(std::uint64_t(std::numeric_limits<std::int32_t>::max())+1))-1);
    p.tile_size=static_cast<std::size_t>(d.limited(1024));p.max_cells=static_cast<std::size_t>(d.limited(1000000));p.work_bytes=static_cast<std::size_t>(d.limited(512u*1024u*1024u));
    p.contours.clip_to_roi=d.limited(1)!=0;p.contours.max_components=static_cast<std::size_t>(d.limited(1000000));p.contours.max_edges=static_cast<std::size_t>(d.limited(10000000));
    p.contours.max_vertices=static_cast<std::size_t>(d.limited(10000000));p.contours.work_bytes=static_cast<std::size_t>(d.limited(512u*1024u*1024u));p.validate();
    const auto level=static_cast<std::int32_t>(d.limited(std::numeric_limits<std::int32_t>::max()));const auto scale=d.real();
    if(std::size_t(level)>=metadata.levels.size() || scale!=metadata.levels[level].downsample || (p.level>=0 && p.level!=level) || !roi.is_area()) throw std::invalid_argument("cell archive pyramid or ROI mismatch");
    const auto& source=metadata.levels[level];const auto b=roi.bounds();
    if(b.x<0 || b.y<0 || static_cast<long double>(b.x+b.width)>metadata.width || static_cast<long double>(b.y+b.height)>metadata.height) throw std::invalid_argument("cell archive ROI exceeds source");
    const auto x=d.limited(source.width),y=d.limited(source.height),width=d.limited(source.width),height=d.limited(source.height);
    const auto left=edge(b.x,scale,source.width),top=edge(b.y,scale,source.height),right=edge(b.x+b.width,scale,source.width),bottom=edge(b.y+b.height,scale,source.height);
    const Rect roi_grid{double(left),double(top),double(right-left),double(bottom-top)};
    const auto expected_grid=cell_detail::processing_grid(roi_grid,source,p);
    if(!width || !height || x+width>std::uint64_t(source.width) || y+height>std::uint64_t(source.height) || width>std::numeric_limits<std::uint32_t>::max()/height ||
       x!=expected_grid.x || y!=expected_grid.y || width!=expected_grid.width || height!=expected_grid.height) throw std::invalid_argument("invalid cell archive sample grid");
    const auto count=static_cast<std::size_t>(width*height);ColorVector white;std::array<ColorVector,3> stains;
    for(auto& v:white) v=d.real();for(auto& vector:stains) for(auto& v:vector) v=d.real();StainTransform::validate_normalized(stains,white);
    const auto samples=d.limited(count),threshold=d.limited(samples);result->raw_peaks_=static_cast<std::size_t>(d.limited(p.max_cells));
    const auto markers=static_cast<std::size_t>(d.limited(result->raw_peaks_)),components=static_cast<std::size_t>(d.limited(version==4?p.max_cells:markers));
    result->elapsed_ms_=d.real();const auto work=static_cast<std::size_t>(d.limited(p.work_bytes));
    const auto base=cell_detail::work_base(expected_grid,p);
    const auto decoded=(version==4?56ull:20ull)*count+components*(sizeof(CellFeature)+2*sizeof(TissueComponent)+64ull)+markers*sizeof(CellMarker)+40ull*std::max(width,height)+2*roi.payload_bytes()+4096;
    if(!samples || result->elapsed_ms_<0 || (work!=base && work!=base+2ull*p.contours.work_bytes) || decoded>256u*1024u*1024u)
        throw std::invalid_argument("cell archive exceeds decoded budget or has invalid statistics");
    std::string input;for(int i=0;i<64;++i) input.push_back(static_cast<char>(d.byte()));if(!hex(input)) throw std::invalid_argument("invalid cell input digest");
    result->markers_.reserve(markers);std::uint32_t dense=0,previous=0;
    for(std::size_t i=0;i<markers;++i) {
        checkpoint(cancelled);CellMarker m;m.label=d.u32();m.result_label=d.u32();m.pixel_index=d.u32();m.radius=d.real();
        if(m.label!=i+1 || m.result_label>components || (version<4 && m.result_label && m.result_label!=++dense) || m.pixel_index>=count || (i && m.pixel_index<=previous) ||
           m.radius<p.min_peak_radius || m.radius>std::hypot(double(width),double(height))) throw std::invalid_argument("invalid cell archive marker");
        m.center={(double(x)+m.pixel_index%width+.5)*scale,(double(y)+m.pixel_index/width+.5)*scale};previous=m.pixel_index;result->markers_.push_back(m);
    }
    if(version<4 && dense!=components) throw std::invalid_argument("cell marker mapping is not dense");
    result->features_.resize(components);
    for(std::size_t i=0;i<components;++i) {
        checkpoint(cancelled);auto& f=result->features_[i];f.label=static_cast<std::uint32_t>(i+1);f.nucleus_pixels=d.limited(samples);f.cell_pixels=d.limited(samples);
        if(f.nucleus_pixels<p.min_nucleus_pixels || (p.max_nucleus_pixels && f.nucleus_pixels>p.max_nucleus_pixels) || f.cell_pixels<f.nucleus_pixels) throw std::invalid_argument("invalid cell compartment counts");
        for(auto* vector:{&f.nucleus_rgb,&f.cell_rgb,&f.nucleus_channels,&f.cell_channels}) for(auto& v:*vector) v=d.real();
        if(f.cell_pixels>f.nucleus_pixels) {f.cytoplasm_rgb=ColorVector{};f.cytoplasm_channels=ColorVector{};for(auto& v:*f.cytoplasm_rgb) v=d.real();for(auto& v:*f.cytoplasm_channels) v=d.real();}
        for(int c=0;c<3;++c) {
            if(f.nucleus_rgb[c]<0 || f.nucleus_rgb[c]>255 || f.cell_rgb[c]<0 || f.cell_rgb[c]>255 || (f.cytoplasm_rgb && ((*f.cytoplasm_rgb)[c]<0 || (*f.cytoplasm_rgb)[c]>255))) throw std::invalid_argument("invalid cell RGB mean");
            if(!f.cytoplasm_rgb && (f.nucleus_rgb[c]!=f.cell_rgb[c] || f.nucleus_channels[c]!=f.cell_channels[c])) throw std::invalid_argument("empty cytoplasm has different means");
            if(f.cytoplasm_rgb) {
                for(bool channels:{false,true}) {
                    const auto a=(channels?f.nucleus_channels[c]:f.nucleus_rgb[c])*double(f.nucleus_pixels);
                    const auto bsum=(channels?f.cell_channels[c]:f.cell_rgb[c])*double(f.cell_pixels);
                    const auto cyto=(channels?(*f.cytoplasm_channels)[c]:(*f.cytoplasm_rgb)[c])*double(f.cell_pixels-f.nucleus_pixels);
                    if(std::abs(bsum-a-cyto)>1e-6*(1+std::abs(a)+std::abs(bsum)+std::abs(cyto))) throw std::invalid_argument("inconsistent cell compartment means");
                }
            }
        }
    }
    auto nucleus_labels=d.labels(count,components,cancelled),body_labels=d.labels(count,components,cancelled);
    std::vector<std::uint8_t> historical_domain;
    if(version==4) {
        const auto bytes=(count+7)/8;if(d.limited(bytes)!=bytes) throw std::invalid_argument("sampled cell domain length differs");
        result->domain_bits_.resize(bytes);for(std::size_t i=0;i<bytes;++i) {if((i&4095)==0) checkpoint(cancelled);result->domain_bits_[i]=d.byte();}
        if(count%8 && (result->domain_bits_.back()>>(count%8))) throw std::invalid_argument("noncanonical sampled cell domain padding");
        historical_domain.resize(count);std::uint64_t opaque=0;
        for(std::size_t i=0;i<count;++i) {
            if((i&4095)==0) checkpoint(cancelled);const bool inside=(result->domain_bits_[i/8]>>(i%8))&1u;historical_domain[i]=inside;opaque+=inside;
            if(inside && (x+i%width<left || y+i/width<top || x+i%width>=right || y+i/width>=bottom ||
                          !roi.contains({(double(x)+i%width+.5)*scale,(double(y)+i/width+.5)*scale}))) throw std::invalid_argument("sampled cell domain exceeds its ROI sample grid");
            if(!inside && (nucleus_labels[i] || body_labels[i])) throw std::invalid_argument("sampled cell label exceeds its domain");
        }
        if(opaque!=samples) throw std::invalid_argument("sampled cell domain sample count differs");
    }
    if(d.left) throw std::invalid_argument("trailing cell archive payload");
    auto raster=[&](std::vector<std::uint32_t> labels,bool body) {
        auto r=std::shared_ptr<TissueResult>(new TissueResult());r->algorithm_=body?"qpx.cell-bodies/1":"qpx.cell-nuclei/1";r->input_sha256_=input;r->roi_=roi;
        if(p.algorithm!=CellAlgorithm::DistanceWatershed) {r->algorithm_=cell_detail::mask_algorithm(p.algorithm,body);r->input_format_override_=cell_detail::input_format(p.algorithm,level!=0);}
        r->object_id_=object;r->object_revision_=object_revision;r->document_revision_=document_revision;r->level_=level;r->downsample_=scale;r->white_=white;r->stains_=stains;
        r->image_bounds_={0,0,double(metadata.width),double(metadata.height)};r->level_bounds_={double(x),double(y),double(width),double(height)};r->bounds_={double(x)*scale,double(y)*scale,double(width)*scale,double(height)*scale};
        r->parameters_.feature=p.feature;r->parameters_.threshold=p.threshold;r->parameters_.level=level;r->parameters_.tile_size=p.tile_size;r->parameters_.max_components=p.max_cells;r->parameters_.work_bytes=p.work_bytes;
        r->samples_=samples;r->raw_components_=components;r->estimated_work_bytes_=work;r->labels_=std::move(labels);
        struct Stats {std::uint64_t n=0,sx=0,sy=0,minx=std::numeric_limits<std::uint64_t>::max(),miny=std::numeric_limits<std::uint64_t>::max(),maxx=0,maxy=0;};
        std::vector<Stats> stats(components);
        for(std::size_t i=0;i<count;++i) {
            if((i&4095)==0) checkpoint(cancelled);const auto label=r->labels_[i];if(!label) continue;const auto xx=i%width,yy=i/width;auto& s=stats[label-1];
            // QCL4 already checked every opaque domain sample against the ROI
            // and rejected every label outside that domain. Reuse that proof
            // instead of repeating a geometry predicate for each body pixel.
            if(body && (x+xx<left || y+yy<top || x+xx>=right || y+yy>=bottom ||
                        (version<4 && !roi.contains({(double(x)+xx+.5)*scale,(double(y)+yy+.5)*scale})))) throw std::invalid_argument("cell label exceeds source ROI");
            ++s.n;s.sx+=xx;s.sy+=yy;s.minx=std::min<std::uint64_t>(s.minx,xx);s.miny=std::min<std::uint64_t>(s.miny,yy);s.maxx=std::max<std::uint64_t>(s.maxx,xx);s.maxy=std::max<std::uint64_t>(s.maxy,yy);
        }
        r->components_.reserve(components);
        for(std::size_t i=0;i<components;++i) {
            const auto& s=stats[i];const auto& f=result->features_[i];if(!s.n || s.n!=(body?f.cell_pixels:f.nucleus_pixels)) throw std::invalid_argument("cell label and feature counts disagree");
            r->foreground_pixels_+=s.n;r->components_.push_back({static_cast<std::uint32_t>(i+1),s.n,{double(x+s.minx)*scale,double(y+s.miny)*scale,double(s.maxx-s.minx+1)*scale,double(s.maxy-s.miny+1)*scale},
                {(double(x)+double(s.sx)/double(s.n)+.5)*scale,(double(y)+double(s.sy)/double(s.n)+.5)*scale}});
        }
        r->threshold_pixels_=r->cleaned_pixels_=body?r->foreground_pixels_:threshold;
        if(r->foreground_pixels_>(body?samples:threshold)) throw std::invalid_argument("invalid cell raster sample counts");return r;
    };
    result->nuclei_=raster(std::move(nucleus_labels),false);result->cells_=raster(std::move(body_labels),true);
    const auto& nuclei=result->nuclei_->labels();const auto& bodies=result->cells_->labels();
    for(const auto& marker:result->markers_) if(marker.result_label && nuclei[marker.pixel_index]!=marker.result_label &&
       (p.algorithm==CellAlgorithm::DistanceWatershed || nuclei[marker.pixel_index])) throw std::invalid_argument("cell marker does not lie in its nucleus or clipped context");
    if(version==4) {
        if(components && p.cell_expansion>0) {
            const auto expected=cell_detail::expand_cells_watershed(nuclei,historical_domain,width,height,p.cell_expansion,p.work_bytes,cancelled);
            if(expected.labels!=bodies) throw std::invalid_argument("sampled cell body differs from its historical watershed recipe");
        } else if(nuclei!=bodies) throw std::invalid_argument("sampled cell body violates its empty/zero-radius recipe");
    }
    std::vector<double> distance;std::vector<std::uint32_t> nearest;
    if(version<4 && components && p.cell_expansion>0) {distance.resize(count);for(std::size_t i=0;i<count;++i) distance[i]=nuclei[i]?0:cell_detail::infinity;cell_detail::distance(distance,width,height,false,&nearest,cancelled);}
    for(std::size_t i=0;i<count;++i) {
        if((i&4095)==0) checkpoint(cancelled);
        if(nuclei[i] && nuclei[i]!=bodies[i]) throw std::invalid_argument("cell body does not contain its nucleus");
        if(p.cell_expansion==0 && nuclei[i]!=bodies[i]) throw std::invalid_argument("cell expansion violates zero radius");
        if(version<4 && bodies[i] && p.cell_expansion>0 && (distance[i]>p.cell_expansion*p.cell_expansion || nearest[i]==std::numeric_limits<std::uint32_t>::max() || bodies[i]!=nuclei[nearest[i]])) throw std::invalid_argument("cell body violates nearest nucleus assignment");
    }
    // Geometry is derived from the validated historical labels, never from
    // today's pixels. Preserve readable numeric-only runs if their original
    // contour limits cannot support geometry in this build.
    try {restore_cell_contours(*result,cancelled);}
    catch(const std::length_error&) {result->nucleus_contours_.reset();result->cell_contours_.reset();}
    catch(const std::runtime_error&) {result->nucleus_contours_.reset();result->cell_contours_.reset();}
    checkpoint(cancelled);return result;
}
}

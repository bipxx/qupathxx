#include "cell_archive.hpp"
#include "scientific_cells.hpp"
#include "scientific_raster.hpp"
#include "scientific_cell_recipe.hpp"
#include "provenance_codec.hpp"
#include "geometry_archive.hpp"
#include "cell_contours.hpp"
#include "qupathxx/provenance.hpp"
#include "sha256.hpp"
#include "grid_cell_descriptor.hpp"
#include <cstring>

namespace qpx {
namespace {
using cell_detail::checkpoint;
constexpr std::size_t limit=256ULL*1024*1024;
constexpr std::uint64_t work_limit=512ULL*1024*1024;
std::uint64_t archive_work(std::size_t n,std::size_t count,std::size_t markers,std::size_t provenance,std::size_t geometry,std::size_t contours,std::size_t encoded) {
    // Historical decoding/canonicalization has its own fixed cap, distinct
    // from the captured detector's work limit and Document retention quota.
    return 128ULL*n+1024ULL*count+sizeof(CellMarker)*markers+2ULL*provenance+4ULL*geometry+2ULL*contours+4ULL*encoded+65536;
}
bool hex(const std::string& s) {return s.size()==64 && std::all_of(s.begin(),s.end(),[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');});}
struct Writer {
    std::vector<std::uint8_t> bytes={'Q','C','L',5};
    void integer(std::uint64_t v) {for(unsigned i=0;i<8;++i) bytes.push_back(std::uint8_t(v>>(8*i)));}
    void small(std::uint32_t v) {for(unsigned i=0;i<4;++i) bytes.push_back(std::uint8_t(v>>(8*i)));}
    void real(double v) {if(!std::isfinite(v)) throw std::invalid_argument("nonfinite grid cell archive number");std::uint64_t b;std::memcpy(&b,&v,8);integer(b);}
    void blob(const std::vector<std::uint8_t>& v) {integer(v.size());bytes.insert(bytes.end(),v.begin(),v.end());}
    void labels(const std::vector<std::uint32_t>& v,const std::atomic<bool>* cancelled) {
        std::size_t runs=0;for(std::size_t i=0;i<v.size();++i) {if((i&4095)==0) checkpoint(cancelled);runs+=!i || v[i]!=v[i-1];}
        const bool rle=8*runs<4*v.size();integer(rle);integer(rle?runs:v.size());
        if(!rle) {for(std::size_t i=0;i<v.size();++i) {if((i&4095)==0) checkpoint(cancelled);small(v[i]);}return;}
        for(std::size_t i=0;i<v.size();) {
            checkpoint(cancelled);auto end=i+1;while(end<v.size() && v[end]==v[i]) {if((end&4095)==0) checkpoint(cancelled);++end;}
            small(std::uint32_t(end-i));small(v[i]);i=end;
        }
    }
};
struct Reader {
    const std::uint8_t* p;std::size_t left;
    std::uint64_t integer(std::uint64_t maximum=std::numeric_limits<std::uint64_t>::max(),unsigned n=8) {
        if(left<n) throw std::invalid_argument("truncated grid cell archive");std::uint64_t v=0;
        for(unsigned i=0;i<n;++i) v|=std::uint64_t(p[i])<<(8*i);p+=n;left-=n;
        if(v>maximum) throw std::invalid_argument("grid cell archive integer exceeds limit");return v;
    }
    std::int64_t signed_integer() {const auto b=integer();std::int64_t v;std::memcpy(&v,&b,8);return v;}
    double real() {const auto b=integer();double v;std::memcpy(&v,&b,8);if(!std::isfinite(v)) throw std::invalid_argument("nonfinite grid cell archive number");return v;}
    std::pair<const std::uint8_t*,std::size_t> blob(std::size_t maximum) {const auto n=std::size_t(integer(maximum));if(n>left) throw std::invalid_argument("truncated grid cell archive blob");const auto start=p;p+=n;left-=n;return {start,n};}
    std::vector<std::uint32_t> labels(std::size_t n,std::size_t count,const std::atomic<bool>* cancelled) {
        const bool rle=integer(1)!=0;const auto entries=integer(n);
        if((!rle && entries!=n) || (rle && !entries) || entries*(rle?8:4)>left) throw std::invalid_argument("invalid grid cell label length");
        std::vector<std::uint32_t> out(n);std::size_t at=0;std::uint32_t previous=std::numeric_limits<std::uint32_t>::max();
        for(std::size_t i=0;i<entries;++i) {
            checkpoint(cancelled);const auto run=rle?integer(n,4):1,label=integer(count,4);
            if(!run || run>n-at || (rle && label==previous)) throw std::invalid_argument("invalid grid cell label run");
            const auto stop=at+run;while(at<stop) {checkpoint(cancelled);const auto size=std::min<std::size_t>(65536,stop-at);std::fill_n(out.begin()+at,size,std::uint32_t(label));at+=size;}
            previous=std::uint32_t(label);
        }
        if(at!=n) throw std::invalid_argument("incomplete grid cell label raster");return out;
    }
};
std::string checksum(const std::uint8_t* data,std::size_t size,const Roi& roi,const std::atomic<bool>* cancelled) {
    Sha256 hash;const auto geometry=GeometryArchiveCodec::encode(roi);hash.integer(geometry.size());hash.update(geometry.data(),geometry.size());
    for(std::size_t at=0;at<size;) {checkpoint(cancelled);const auto n=std::min<std::size_t>(65536,size-at);hash.update(data+at,n);at+=n;}return hash.finish();
}
void parameters(Writer& w,const GridCellParameters& p) {
    cell_detail::grid_parameters_write(w,p);
}
GridCellParameters parameters(Reader& d) {
    return cell_detail::grid_parameters_read(d);
}
auto resolved(const ResolvedGridCellParameters& r) {return std::make_tuple(r.step_x,r.step_y,r.sigma_x,r.sigma_y,r.min_nucleus_samples,r.max_nucleus_samples);}
bool source_support(const CellSamplingGrid& grid,std::size_t i) {return grid.source_support(i);}
void feature_means(const CellFeature& f) {
    for(unsigned c=0;c<3;++c) {
        for(auto v:{f.nucleus_rgb[c],f.cell_rgb[c],f.cytoplasm_rgb?(*f.cytoplasm_rgb)[c]:0}) if(v<0 || v>255) throw std::invalid_argument("invalid grid cell RGB mean");
        if(!f.cytoplasm_rgb) {
            if(f.nucleus_rgb[c]!=f.cell_rgb[c] || f.nucleus_channels[c]!=f.cell_channels[c]) throw std::invalid_argument("empty grid cytoplasm has different means");
        } else for(bool channels:{false,true}) {
            const auto a=channels?f.nucleus_channels[c]:f.nucleus_rgb[c],b=channels?f.cell_channels[c]:f.cell_rgb[c],v=channels?(*f.cytoplasm_channels)[c]:(*f.cytoplasm_rgb)[c];
            const auto expected=a*(double(f.nucleus_pixels)/double(f.cell_pixels))+v*(double(f.cell_pixels-f.nucleus_pixels)/double(f.cell_pixels));
            if(!std::isfinite(expected) || std::abs(b-expected)>1e-10*(1+std::max({std::abs(a),std::abs(b),std::abs(v)}))) throw std::invalid_argument("inconsistent grid cell compartment means");
        }
    }
}
}
std::vector<std::uint8_t> CellArchiveCodec::encode_grid(const CellResult& r,const std::atomic<bool>* cancelled) {
    checkpoint(cancelled);const auto& p=*r.grid_parameters();p.validate();
    if(!r.nuclei() || !r.cells() || !r.nuclei()->provenance() ||
       (!r.nuclei()->scientific_grid() && !r.nuclei()->nominal_sampling())) throw std::invalid_argument("missing grid cell archive snapshot");
    const auto& n=*r.nuclei();const CellSamplingGrid grid=n.nominal_sampling()?CellSamplingGrid(n.provenance()->source.width,n.provenance()->source.height,*n.nominal_sampling()):CellSamplingGrid(*n.scientific_grid());
    if(std::string(r.algorithm())!=grid.algorithm()) throw std::invalid_argument("grid cell archive algorithm differs");
    const auto provenance=ProvenanceCodec::encode(*n.provenance());
    if(r.domain_bits_.size()!=(n.labels().size()+7)/8 || !hex(n.input_sha256()) || n.input_sha256()!=r.cells()->input_sha256() || r.elapsed_ms()<0)
        throw std::invalid_argument("invalid grid cell archive snapshot");
    const auto encoded_bound=8ULL*n.labels().size()+160ULL*r.features().size()+20ULL*r.markers().size()+r.domain_bits_.size()+provenance.size()+4096;
    if(encoded_bound>limit || archive_work(n.labels().size(),r.features().size(),r.markers().size(),n.provenance()->payload_bytes(),n.roi().payload_bytes(),p.contours.work_bytes,std::size_t(encoded_bound))>work_limit)
        throw std::length_error("grid cell archive exceeds encoded byte limit");
    Writer w;w.bytes[3]=grid.nominal?6:5;parameters(w,p);cell_grid_write(w,grid);
    w.blob(provenance);for(auto v:n.white()) w.real(v);for(const auto& row:n.stains()) for(auto v:row) w.real(v);
    const auto& q=*r.resolved_grid_parameters();for(auto v:{q.step_x,q.step_y,q.sigma_x,q.sigma_y}) w.real(v);w.integer(q.min_nucleus_samples);w.integer(q.max_nucleus_samples);
    for(auto v:{n.samples(),n.threshold_pixels(),std::uint64_t(r.raw_peaks()),std::uint64_t(r.markers().size()),std::uint64_t(r.features().size())}) w.integer(v);
    w.real(r.elapsed_ms());w.bytes.insert(w.bytes.end(),n.input_sha256().begin(),n.input_sha256().end());
    for(const auto& m:r.markers()) {checkpoint(cancelled);w.small(m.label);w.small(m.result_label);w.small(m.pixel_index);w.real(m.radius);}
    for(const auto& f:r.features()) {
        checkpoint(cancelled);w.integer(f.nucleus_pixels);w.integer(f.cell_pixels);
        for(const auto& row:{f.nucleus_rgb,f.cell_rgb,f.nucleus_channels,f.cell_channels}) for(auto v:row) w.real(v);
        if(f.cytoplasm_rgb) {for(auto v:*f.cytoplasm_rgb) w.real(v);for(auto v:*f.cytoplasm_channels) w.real(v);}
    }
    w.labels(n.labels(),cancelled);w.labels(r.cells()->labels(),cancelled);w.blob(r.domain_bits_);
    const auto digest=checksum(w.bytes.data(),w.bytes.size(),n.roi(),cancelled);w.bytes.insert(w.bytes.end(),digest.begin(),digest.end());
    if(w.bytes.size()>limit) throw std::length_error("grid cell archive exceeds encoded byte limit");return w.bytes;
}
std::shared_ptr<const CellResult> CellArchiveCodec::decode_grid(const std::uint8_t* data,std::size_t size,Roi roi,const ImageMetadata& metadata,const std::atomic<bool>* cancelled) {
    checkpoint(cancelled);if(!data || size<512 || size>limit) throw std::invalid_argument("invalid grid cell archive size");
    const std::string digest(reinterpret_cast<const char*>(data+size-64),64);
    if(!hex(digest) || checksum(data,size-64,roi,cancelled)!=digest) throw std::invalid_argument("grid cell archive checksum mismatch");
    Reader d{data+4,size-68};const auto p=parameters(d);const auto grid=cell_grid_read(d,data[3]==6,p.work_bytes);
    if(grid.image_width()!=metadata.width || grid.image_height()!=metadata.height || !roi.is_area()) throw std::invalid_argument("grid cell archive image or ROI mismatch");
    const auto b=roi.bounds();if(b.x<0 || b.y<0 || b.x+b.width>metadata.width || b.y+b.height>metadata.height) throw std::invalid_argument("grid cell archive ROI exceeds image");
    const auto blob=d.blob(4ULL*1024*1024);const auto provenance=ProvenanceCodec::decode(blob.first,blob.second);
    if(provenance->source.width!=metadata.width || provenance->source.height!=metadata.height || provenance->nucleus) throw std::invalid_argument("grid cell archive provenance mismatch");
    ColorVector white;std::array<ColorVector,3> stains;for(auto& v:white) v=d.real();for(auto& row:stains) for(auto& v:row) v=d.real();StainTransform::validate_normalized(stains,white);
    ResolvedGridCellParameters q;q.step_x=d.real();q.step_y=d.real();q.sigma_x=d.real();q.sigma_y=d.real();q.min_nucleus_samples=d.integer();q.max_nucleus_samples=d.integer();
    if(resolved(q)!=resolved(resolve_grid_cell_parameters(grid,p,*provenance))) throw std::invalid_argument("grid cell archive resolved units or area differ");
    grid_cell_work_bytes(grid,p,q,roi,*provenance,provenance->source,cancelled);
    const auto n=std::size_t(grid.width()*grid.height());
    const auto samples=d.integer(n),threshold=d.integer(samples),raw=d.integer(std::min(p.max_cells,n)),markers=d.integer(raw),count=d.integer(std::min(markers,samples));
    if(!samples || archive_work(n,std::size_t(count),std::size_t(markers),provenance->payload_bytes(),roi.payload_bytes(),p.contours.work_bytes,size)>work_limit)
        throw std::invalid_argument("grid cell archive exceeds decoded budget");
    auto result=std::shared_ptr<CellResult>(new CellResult());result->algorithm_=grid.algorithm();result->grid_parameters_=p;result->resolved_grid_parameters_=q;
    result->parameters_.algorithm=CellAlgorithm::GridLogWatershedBodies;result->parameters_.max_cells=p.max_cells;result->parameters_.work_bytes=p.work_bytes;result->parameters_.contours=p.contours;
    result->raw_peaks_=std::size_t(raw);result->elapsed_ms_=d.real();if(result->elapsed_ms_<0 || d.left<64) throw std::invalid_argument("invalid grid cell elapsed time or digest");
    const std::string input(reinterpret_cast<const char*>(d.p),64);d.p+=64;d.left-=64;if(!hex(input)) throw std::invalid_argument("invalid grid cell input digest");
    if(20ULL*markers+112ULL*count>d.left) throw std::invalid_argument("truncated grid cell markers or features");
    result->markers_.reserve(std::size_t(markers));std::vector<std::uint8_t> owners(std::size_t(count)+1);std::uint32_t previous=0;
    const auto radius_bound=std::hypot(double(grid.width())*q.step_x,double(grid.height())*q.step_y);
    for(std::size_t i=0;i<markers;++i) {
        checkpoint(cancelled);CellMarker m;m.label=std::uint32_t(d.integer(markers,4));m.result_label=std::uint32_t(d.integer(count,4));m.pixel_index=std::uint32_t(d.integer(n-1,4));m.radius=d.real();
        if(m.label!=i+1 || (i && m.pixel_index<=previous) || m.radius<p.min_peak_radius || m.radius>radius_bound || !source_support(grid,m.pixel_index) || (m.result_label && owners[m.result_label]++))
            throw std::invalid_argument("invalid grid cell marker");
        m.center={grid.center_x(std::int64_t(m.pixel_index%grid.width())),grid.center_y(std::int64_t(m.pixel_index/grid.width()))};
        previous=m.pixel_index;result->markers_.push_back(m);
    }
    for(std::size_t i=1;i<=count;++i) if(!owners[i]) throw std::invalid_argument("grid cell component has no marker");
    result->features_.resize(std::size_t(count));
    for(std::size_t i=0;i<count;++i) {
        checkpoint(cancelled);auto& f=result->features_[i];f.label=std::uint32_t(i+1);f.nucleus_pixels=d.integer(samples);f.cell_pixels=d.integer(samples);
        if(f.nucleus_pixels<q.min_nucleus_samples || (q.max_nucleus_samples && f.nucleus_pixels>q.max_nucleus_samples) || f.cell_pixels<f.nucleus_pixels) throw std::invalid_argument("invalid grid cell feature counts");
        for(auto* row:{&f.nucleus_rgb,&f.cell_rgb,&f.nucleus_channels,&f.cell_channels}) for(auto& v:*row) v=d.real();
        if(f.cell_pixels>f.nucleus_pixels) {f.cytoplasm_rgb=ColorVector{};f.cytoplasm_channels=ColorVector{};for(auto& v:*f.cytoplasm_rgb) v=d.real();for(auto& v:*f.cytoplasm_channels) v=d.real();}
        feature_means(f);
    }
    auto nuclei=d.labels(n,std::size_t(count),cancelled),bodies=d.labels(n,std::size_t(count),cancelled);
    const auto bits=d.blob((n+7)/8);if(bits.second!=(n+7)/8) throw std::invalid_argument("invalid grid cell domain length");
    result->domain_bits_.assign(bits.first,bits.first+bits.second);
    if(d.left || (n%8 && result->domain_bits_.back()>>(n%8))) throw std::invalid_argument("noncanonical grid cell domain or trailing bytes");
    std::vector<std::uint8_t> domain(n);std::uint64_t opaque=0;
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);domain[i]=(result->domain_bits_[i/8]>>(i%8))&1;opaque+=domain[i];
        if(domain[i] && (!source_support(grid,i) || !grid.core_roi_contains(roi,p.sampling_policy,i%grid.width(),i/grid.width()))) throw std::invalid_argument("grid cell domain exceeds captured ROI or source support");
        if((nuclei[i] || bodies[i]) && !domain[i]) throw std::invalid_argument("grid cell label exceeds its domain");
        if(nuclei[i] && nuclei[i]!=bodies[i]) throw std::invalid_argument("grid cell body loses a nucleus");
    }
    if(opaque!=samples) throw std::invalid_argument("grid cell sample count differs from domain");
    const auto expected=cell_detail::expand_grid_cells(nuclei,domain,std::size_t(grid.width()),std::size_t(grid.height()),{q.step_x,q.step_y},p.cell_expansion,p.work_bytes,cancelled);
    if(expected.labels!=bodies) throw std::invalid_argument("grid cell body differs from its historical metric recipe");
    for(const auto& m:result->markers_) if(m.result_label && nuclei[m.pixel_index] && nuclei[m.pixel_index]!=m.result_label) throw std::invalid_argument("grid cell marker label mismatch");
    auto raster=[&](std::vector<std::uint32_t> labels,bool body) {
        auto mask=std::shared_ptr<TissueResult>(new TissueResult());mask->scientific_grid_=grid.area;mask->nominal_sampling_=grid.nominal;if(grid.nominal) mask->downsample_=grid.step_x();mask->roi_=roi;mask->provenance_=provenance;
        mask->algorithm_=grid.nominal?(body?"qpx.nominal-cell-bodies/1":"qpx.nominal-cell-nuclei/1"):(body?"qpx.grid-cell-bodies/1":"qpx.grid-cell-nuclei/1");mask->input_format_override_=grid.input_format();mask->input_sha256_=input;
        mask->object_id_=provenance->object_id;mask->object_revision_=provenance->object_revision;mask->document_revision_=provenance->document_revision;
        mask->white_=white;mask->stains_=stains;mask->image_bounds_={0,0,double(metadata.width),double(metadata.height)};
        mask->parameters_.level=0;mask->parameters_.feature=p.feature;mask->parameters_.threshold=p.threshold;mask->parameters_.max_components=p.max_cells;mask->parameters_.work_bytes=p.work_bytes;mask->parameters_.sampling_policy=p.sampling_policy;
        mask->components_=grid.components(labels,std::uint32_t(count),cancelled);mask->labels_=std::move(labels);
        const auto x=mask->sample_edge_x(0),y=mask->sample_edge_y(0);mask->bounds_={x,y,mask->sample_edge_x(grid.width())-x,mask->sample_edge_y(grid.height())-y};
        mask->samples_=samples;mask->raw_components_=count;mask->estimated_work_bytes_=std::size_t(cell_raster_work_bytes(grid,std::uint32_t(count),roi,*provenance));
        for(std::size_t i=0;i<count;++i) {const auto size=mask->components_[i].pixels;if(size!=(body?result->features_[i].cell_pixels:result->features_[i].nucleus_pixels)) throw std::invalid_argument("grid cell feature and label counts differ");mask->foreground_pixels_+=size;}
        if(mask->foreground_pixels_>(body?samples:threshold)) throw std::invalid_argument("grid cell foreground exceeds its sample counters");
        mask->threshold_pixels_=mask->cleaned_pixels_=body?mask->foreground_pixels_:threshold;return mask;
    };
    result->nuclei_=raster(std::move(nuclei),false);result->cells_=raster(std::move(bodies),true);
    const auto canonical=encode_grid(*result,cancelled);
    if(canonical.size()!=size || !std::equal(canonical.begin(),canonical.end(),data)) throw std::invalid_argument("noncanonical grid cell archive encoding");
    try {restore_cell_contours(*result,cancelled);} catch(const std::length_error&) {result->nucleus_contours_.reset();result->cell_contours_.reset();}
    catch(const std::runtime_error&) {result->nucleus_contours_.reset();result->cell_contours_.reset();}
    checkpoint(cancelled);return result;
}
}

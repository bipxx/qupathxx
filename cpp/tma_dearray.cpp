#include "qupathxx/tma_dearray.hpp"
#include "qupathxx/resampling.hpp"
#include "tma_internal.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace qpx {
namespace {
constexpr double pi=3.14159265358979323846;
constexpr auto absent=std::numeric_limits<std::size_t>::max();
void checkpoint(const std::atomic<bool>* flag) {
    if(flag && flag->load(std::memory_order_relaxed)) throw std::runtime_error("TMA dearraying cancelled");
}
void finite_range(double value,double low,double high,const char* message) {
    if(!std::isfinite(value) || value<low || value>high) throw std::invalid_argument(message);
}
std::size_t pixels(std::int64_t w,std::int64_t h) {
    if(w<1 || h<1 || w>4096 || h>4096) throw std::invalid_argument("TMA preview dimensions must be 1..4096");
    return static_cast<std::size_t>(w*h);
}
void geometry(std::int64_t w,std::int64_t h,std::int64_t iw,std::int64_t ih,const TmaDearrayOptions& o) {
    pixels(w,h);
    if(iw<w || ih<h || iw>1000000000000000LL || ih>1000000000000000LL)
        throw std::invalid_argument("TMA preview requires positive, non-upscaled level-zero geometry");
    if(o.core_width*double(w)/double(iw)<6 || o.core_height*double(h)/double(ih)<6)
        throw std::invalid_argument("TMA cores require at least six preview pixels on both axes");
}
void real(Sha256& hash,double value) {std::uint64_t bits;static_assert(sizeof(bits)==sizeof(value));std::memcpy(&bits,&value,sizeof(bits));hash.integer(bits);}
std::string recipe(const TmaDearrayResult& result,const TmaDearrayOptions& o) {
    Sha256 hash;const std::string domain="qpx.tma-dearray/1";hash.update(domain.data(),domain.size());
    hash.update(result.input_sha256.data(),result.input_sha256.size());hash.integer(o.rows);hash.integer(o.columns);
    for(double value:{o.core_width,o.core_height,o.bounds_scale,o.missing_density,o.min_valid_fraction,o.min_component_area,
                     o.max_component_area,o.min_component_extent,o.max_fit_error,o.max_rotation_degrees}) real(hash,value);
    hash.integer(static_cast<unsigned>(o.polarity));hash.integer(o.max_candidates);hash.integer(o.work_bytes);hash.integer(o.pair_checks);
    hash.integer(o.thumbnail.max_width);hash.integer(o.thumbnail.max_height);
    hash.integer(o.thumbnail.source_pixel_limit);hash.integer(o.thumbnail.output_pixel_limit);
    hash.integer(o.name.size());hash.update(o.name.data(),o.name.size());hash.integer(bool(result.thumbnail_plan));
    if(result.thumbnail_plan) {
        const auto& p=*result.thumbnail_plan;hash.integer(p.level);hash.integer(p.source_width);hash.integer(p.source_height);
        real(hash,p.level_downsample);real(hash,p.downsample_x);real(hash,p.downsample_y);
    }
    return hash.finish();
}
std::uint8_t otsu(const std::array<std::uint64_t,256>& histogram) {
    std::uint64_t total=0,sum=0;
    for(std::size_t i=0;i<histogram.size();++i) {total+=histogram[i];sum+=i*histogram[i];}
    std::uint64_t left=0,left_sum=0;double best=-1;std::uint8_t threshold=0;
    for(std::size_t i=0;i<255;++i) {
        left+=histogram[i];left_sum+=i*histogram[i];if(!left || left==total) continue;
        const auto delta=double(left_sum)*double(total)-double(sum)*double(left);
        const auto score=delta*delta/(double(left)*double(total-left));
        if(score>best) {best=score;threshold=static_cast<std::uint8_t>(i);}
    }
    if(best<0) throw std::invalid_argument("TMA preview has no tissue/background contrast");
    return threshold;
}
struct Candidate {double x,y;};
std::vector<Candidate> components(const std::vector<std::uint8_t>& mask,const std::vector<std::uint8_t>& valid,
    std::int64_t w,std::int64_t h,double cw,double ch,const TmaDearrayOptions& o,const std::atomic<bool>* stop) {
    std::vector<std::uint8_t> seen(mask.size());std::vector<std::uint32_t> queue;queue.reserve(mask.size());
    const auto limit=std::min(o.max_candidates,o.rows*o.columns);std::vector<Candidate> out;out.reserve(limit);
    const auto expected=pi*cw*ch/4;
    for(std::size_t seed=0;seed<mask.size();++seed) {
        if((seed&4095)==0) checkpoint(stop);
        if(!mask[seed] || seen[seed]) continue;
        queue.clear();queue.push_back(static_cast<std::uint32_t>(seed));seen[seed]=1;
        std::int64_t minx=w,miny=h,maxx=0,maxy=0;double sx=0,sy=0;bool boundary=false;
        for(std::size_t at=0;at<queue.size();++at) {
            if((at&4095)==0) checkpoint(stop);
            const auto x=std::int64_t(queue[at]%w),y=std::int64_t(queue[at]/w);
            minx=std::min(minx,x);miny=std::min(miny,y);maxx=std::max(maxx,x);maxy=std::max(maxy,y);sx+=double(x)+.5;sy+=double(y)+.5;
            for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
                if(!dx && !dy) continue;
                const auto xx=x+dx,yy=y+dy;
                if(xx<0 || yy<0 || xx>=w || yy>=h) {boundary=true;continue;}
                const auto next=static_cast<std::size_t>(yy*w+xx);if(!valid[next]) boundary=true;
                if(mask[next] && !seen[next]) {seen[next]=1;queue.push_back(static_cast<std::uint32_t>(next));}
            }
        }
        const auto area=double(queue.size()),bw=double(maxx-minx+1),bh=double(maxy-miny+1);
        const auto aspect=(bw/cw)/(bh/ch);
        if(boundary || area<expected*o.min_component_area || area>expected*o.max_component_area ||
           area/(bw*bh)<o.min_component_extent || aspect<.5 || aspect>2) continue;
        if(out.size()==limit) throw std::length_error("TMA candidate count exceeds requested grid or candidate limit");
        out.push_back({sx/area,sy/area});
    }
    if(out.empty()) throw std::invalid_argument("TMA preview contains no credible core candidates");
    return out;
}
// Projection groups are separated by observed gaps. An unobserved requested
// row/column cannot be invented by splitting a tight cluster into extra labels.
bool groups(const std::vector<Candidate>& points,bool row,double angle,double cw,double ch,std::size_t count,
    std::vector<std::size_t>& labels,const std::atomic<bool>* stop) {
    if(count>points.size()) return false;
    std::vector<std::pair<double,std::size_t>> projected;projected.reserve(points.size());
    const auto c=std::cos(angle*pi/180),s=std::sin(angle*pi/180);
    for(std::size_t i=0;i<points.size();++i) {
        const auto x=points[i].x/cw,y=points[i].y/ch;projected.emplace_back(row?-s*x+c*y:c*x+s*y,i);
    }
    checkpoint(stop);std::sort(projected.begin(),projected.end());checkpoint(stop);
    std::vector<std::pair<double,std::size_t>> gaps;gaps.reserve(points.size()-1);
    for(std::size_t i=1;i<projected.size();++i) gaps.emplace_back(projected[i].first-projected[i-1].first,i);
    std::sort(gaps.begin(),gaps.end(),[](auto a,auto b){return a.first>b.first || (a.first==b.first && a.second<b.second);});
    if(count>1 && gaps[count-2].first<.5) return false;
    std::vector<std::size_t> cuts;cuts.reserve(count-1);
    for(std::size_t i=0;i+1<count;++i) cuts.push_back(gaps[i].second);
    std::sort(cuts.begin(),cuts.end());labels.resize(points.size());std::size_t label=0;
    for(std::size_t i=0;i<projected.size();++i) {if(label<cuts.size() && i==cuts[label]) ++label;labels[projected[i].second]=label;}
    return true;
}
bool solve(double a[3][5],std::size_t dimensions,std::array<double,3>& x,std::array<double,3>& y) {
    for(std::size_t column=0;column<dimensions;++column) {
        auto pivot=column;
        for(std::size_t i=column+1;i<dimensions;++i) if(std::abs(a[i][column])>std::abs(a[pivot][column])) pivot=i;
        if(std::abs(a[pivot][column])<1e-10) return false;
        for(std::size_t j=0;j<5;++j) std::swap(a[pivot][j],a[column][j]);
        const auto scale=a[column][column];for(std::size_t j=0;j<5;++j) a[column][j]/=scale;
        for(std::size_t i=0;i<dimensions;++i) if(i!=column) {
            const auto factor=a[i][column];for(std::size_t j=0;j<5;++j) a[i][j]-=factor*a[column][j];
        }
    }
    for(std::size_t i=0;i<dimensions;++i) {x[i]=a[i][3];y[i]=a[i][4];}return true;
}
std::array<double,3> features(std::size_t row,std::size_t col,const TmaDearrayOptions& o) {
    std::array<double,3> values{1,0,0};std::size_t at=1;
    if(o.columns>1) values[at++]=(double(col)-double(o.columns-1)/2)/double(o.columns-1);
    if(o.rows>1) values[at]=(double(row)-double(o.rows-1)/2)/double(o.rows-1);
    return values;
}
struct Fit {std::vector<std::size_t> indices;std::array<double,3> x{},y{};double error=std::numeric_limits<double>::infinity();};
Point predict(const Fit& fit,std::size_t row,std::size_t col,const TmaDearrayOptions& o,double cw,double ch) {
    const auto f=features(row,col,o);Point p;
    for(std::size_t i=0;i<3;++i) {p.x+=f[i]*fit.x[i]*cw;p.y+=f[i]*fit.y[i]*ch;}return p;
}
Fit lattice(const std::vector<Candidate>& points,double cw,double ch,const TmaDearrayOptions& o,const std::atomic<bool>* stop) {
    const auto count=points.size();
    if(count<std::max(o.rows,o.columns) || std::uint64_t(count)*(count-1)/2>o.pair_checks)
        throw std::length_error("TMA lattice lacks observed axes or exceeds pair-work limit");
    std::array<std::uint64_t,3601> horizontal{},vertical{};
    auto add=[&](auto& histogram,double angle) {
        if(std::abs(angle)>o.max_rotation_degrees) return;
        const auto bin=static_cast<std::size_t>(std::llround((angle+45)*40));++histogram.at(bin);
    };
    std::uint64_t pairs=0;
    for(std::size_t i=0;i<count;++i) for(std::size_t j=i+1;j<count;++j) {
        if((++pairs&4095)==0) checkpoint(stop);
        auto dx=(points[j].x-points[i].x)/cw,dy=(points[j].y-points[i].y)/ch;
        if(std::max(std::abs(dx),std::abs(dy))>4 || std::hypot(dx,dy)<.75) continue;
        if(dx<0) {dx=-dx;dy=-dy;}
        add(horizontal,std::atan2(dy,dx)*180/pi);
        if(dy<0) {dx=-dx;dy=-dy;}
        add(vertical,std::atan2(-dx,dy)*180/pi);
    }
    auto median=[](const auto& histogram)->std::optional<double> {
        const auto total=std::accumulate(histogram.begin(),histogram.end(),std::uint64_t{});if(!total) return {};
        std::uint64_t sum=0;for(std::size_t i=0;i<histogram.size();++i) if((sum+=histogram[i])>(total-1)/2) return double(i)/40-45;
        return {};
    };
    const auto ha=median(horizontal),va=median(vertical);
    std::vector<std::pair<double,double>> hypotheses;hypotheses.reserve(190);hypotheses.emplace_back(0,0);
    for(double a=-o.max_rotation_degrees;a<=o.max_rotation_degrees+1e-9;a+=.5) hypotheses.emplace_back(a,a);
    for(double dh:{-.25,-.1,0.,.1,.25}) for(double dv:{-.25,-.1,0.,.1,.25})
        hypotheses.emplace_back(va.value_or(ha.value_or(0))+dv,ha.value_or(va.value_or(0))+dh);
    Fit best;std::vector<std::size_t> row,col;
    const auto dimensions=std::size_t(1)+(o.columns>1)+(o.rows>1);
    for(const auto& angles:hypotheses) {
        checkpoint(stop);
        if(!groups(points,false,angles.first,cw,ch,o.columns,col,stop) || !groups(points,true,angles.second,cw,ch,o.rows,row,stop)) continue;
        Fit current;current.indices.assign(o.rows*o.columns,absent);double matrix[3][5]{};bool collision=false;
        for(std::size_t i=0;i<count;++i) {
            const auto slot=row[i]*o.columns+col[i];if(current.indices[slot]!=absent) {collision=true;break;}current.indices[slot]=i;
            const auto f=features(row[i],col[i],o);
            for(std::size_t j=0;j<dimensions;++j) {
                for(std::size_t k=0;k<dimensions;++k) matrix[j][k]+=f[j]*f[k];
                matrix[j][3]+=f[j]*points[i].x/cw;matrix[j][4]+=f[j]*points[i].y/ch;
            }
        }
        if(collision || !solve(matrix,dimensions,current.x,current.y)) continue;
        const auto origin=predict(current,0,0,o,cw,ch);Point dc{},dr{};
        if(o.columns>1) {const auto p=predict(current,0,1,o,cw,ch);dc={(p.x-origin.x)/cw,(p.y-origin.y)/ch};}
        if(o.rows>1) {const auto p=predict(current,1,0,o,cw,ch);dr={(p.x-origin.x)/cw,(p.y-origin.y)/ch};}
        if((o.columns>1 && (dc.x<=0 || std::hypot(dc.x,dc.y)<.75 || std::abs(std::atan2(dc.y,dc.x)*180/pi)>o.max_rotation_degrees+1e-6)) ||
           (o.rows>1 && (dr.y<=0 || std::hypot(dr.x,dr.y)<.75 || std::abs(std::atan2(-dr.x,dr.y)*180/pi)>o.max_rotation_degrees+1e-6)) ||
           (o.rows>1 && o.columns>1 && dc.x*dr.y-dc.y*dr.x<.1)) continue;
        double sum=0,maximum=0;
        for(std::size_t i=0;i<count;++i) {
            const auto p=predict(current,row[i],col[i],o,cw,ch);
            const auto e=std::hypot((p.x-points[i].x)/cw,(p.y-points[i].y)/ch);sum+=e*e;maximum=std::max(maximum,e);
        }
        current.error=std::sqrt(sum/double(count));
        if(maximum<=o.max_fit_error && current.error<best.error) best=std::move(current);
    }
    if(best.indices.empty()) throw std::invalid_argument("TMA requested rows/columns are ambiguous; review grid dimensions or candidates");
    return best;
}
bool same_metadata(const ImageMetadata& a,const ImageMetadata& b) {
    if(a.width!=b.width || a.height!=b.height || a.pixel_type!=b.pixel_type || a.layout!=b.layout || a.vendor!=b.vendor ||
       a.pixel_width_um!=b.pixel_width_um || a.pixel_height_um!=b.pixel_height_um || a.levels.size()!=b.levels.size()) return false;
    for(std::size_t i=0;i<a.levels.size();++i) if(a.levels[i].width!=b.levels[i].width || a.levels[i].height!=b.levels[i].height || a.levels[i].downsample!=b.levels[i].downsample) return false;
    return true;
}
}

void TmaDearrayOptions::validate() const {
    if(!rows || !columns || rows>512 || columns>512 || rows>10000/columns) throw std::invalid_argument("TMA requested grid exceeds dimensions");
    finite_range(core_width,1e-9,1e12,"invalid TMA core width");finite_range(core_height,1e-9,1e12,"invalid TMA core height");
    finite_range(bounds_scale,.5,1.5,"invalid TMA bounds scale");finite_range(missing_density,0,1,"invalid TMA missing density");
    finite_range(min_valid_fraction,1e-9,1,"invalid TMA valid support fraction");
    finite_range(min_component_area,1e-6,10,"invalid TMA minimum component area");finite_range(max_component_area,min_component_area,10,"invalid TMA maximum component area");
    finite_range(min_component_extent,1e-6,1,"invalid TMA component extent");finite_range(max_fit_error,1e-6,.5,"invalid TMA lattice residual");
    finite_range(max_rotation_degrees,0,40,"invalid TMA rotation limit");
    if(polarity!=TmaPolarity::DarkOnLight && polarity!=TmaPolarity::LightOnDark) throw std::invalid_argument("invalid TMA polarity");
    if(!max_candidates || max_candidates>10000 || !work_bytes || work_bytes>512u*1024u*1024u || !pair_checks || pair_checks>50000000)
        throw std::invalid_argument("invalid TMA working/candidate/pair limit");
    const auto n=std::min(max_candidates,rows*columns);
    if(std::uint64_t(n)*(n-1)/2>pair_checks) throw std::length_error("TMA candidate limit exceeds pair-work admission");
    detail::bounded_utf8(name,256);if(name.empty()) throw std::invalid_argument("TMA name must not be empty");thumbnail.validate();
}
std::size_t TmaDearrayResult::payload_bytes() const {
    std::size_t bytes=sizeof(*this)+grid.cores.capacity()*sizeof(QptmaCore)+cores.capacity()*sizeof(TmaDearrayCore)+
        grid.source_hint.capacity()+grid.image_name.capacity()+input_sha256.capacity()+recipe_sha256.capacity();
    for(const auto& core:grid.cores) bytes+=core.name.capacity()+core.case_id.capacity();
    return bytes;
}
std::size_t tma_dearray_work_bytes(std::int64_t w,std::int64_t h,const TmaDearrayOptions& o) {
    o.validate();const auto n=pixels(w,h);
    return 65536+n*12+std::min(o.max_candidates,o.rows*o.columns)*512+o.rows*o.columns*1024;
}
// Mutable only until the public entry point returns the immutable proposal.
static std::shared_ptr<TmaDearrayResult> detect_preview(const PixelBuffer& preview,std::int64_t iw,std::int64_t ih,const TmaDearrayOptions& o,const std::atomic<bool>* stop) {
    checkpoint(stop);o.validate();const auto w=preview.width(),h=preview.height();geometry(w,h,iw,ih,o);
    const auto required=tma_dearray_work_bytes(w,h,o);if(required>o.work_bytes) throw std::length_error("TMA detection exceeds working payload budget");
    const auto n=pixels(w,h);if(preview.bytes().size()!=n*4) throw std::invalid_argument("TMA preview byte count differs");
    const auto cw=o.core_width*double(w)/double(iw),ch=o.core_height*double(h)/double(ih);
    std::vector<std::uint8_t> gray(n),mask(n),valid(n);std::array<std::uint64_t,256> histogram{};
    const auto& rgba=preview.bytes();
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(stop);
        if(rgba[i*4+3]!=255) continue;
        valid[i]=1;
        auto value=(77u*rgba[i*4]+150u*rgba[i*4+1]+29u*rgba[i*4+2]+128u)>>8;
        if(o.polarity==TmaPolarity::DarkOnLight) value=255-value;
        gray[i]=static_cast<std::uint8_t>(value);++histogram[value];
    }
    const auto threshold=otsu(histogram);
    for(std::size_t i=0;i<n;++i) {if((i&4095)==0) checkpoint(stop);mask[i]=valid[i] && gray[i]>threshold;}
    const auto candidates=components(mask,valid,w,h,cw,ch,o,stop);const auto fit=lattice(candidates,cw,ch,o,stop);
    auto result=std::make_shared<TmaDearrayResult>();result->image_width=iw;result->image_height=ih;result->preview_width=w;result->preview_height=h;
    result->candidates=candidates.size();result->estimated_work_bytes=required;result->lattice_rmse=fit.error;result->threshold=threshold;
    result->grid.rows=o.rows;result->grid.columns=o.columns;result->grid.image_name=o.name;
    result->grid.cores.reserve(o.rows*o.columns);result->cores.reserve(o.rows*o.columns);
    TmaGridOptions labels;labels.rows=o.rows;labels.columns=o.columns;const auto layout=TmaCodec::grid(labels);
    for(std::size_t row=0;row<o.rows;++row) for(std::size_t col=0;col<o.columns;++col) {
        checkpoint(stop);const auto index=fit.indices[row*o.columns+col];const auto predicted=predict(fit,row,col,o,cw,ch);
        const auto center=index==absent?predicted:Point{candidates[index].x,candidates[index].y};
        TmaDearrayCore core;core.center={center.x*double(iw)/double(w),center.y*double(ih)/double(h)};core.observed=index!=absent;
        if(core.observed) core.fit_error=std::hypot((center.x-predicted.x)/cw,(center.y-predicted.y)/ch);
        const Rect bounds{core.center.x-o.core_width*o.bounds_scale/2,core.center.y-o.core_height*o.bounds_scale/2,o.core_width*o.bounds_scale,o.core_height*o.bounds_scale};
        if(bounds.x<0 || bounds.y<0 || bounds.x+bounds.width>double(iw) || bounds.y+bounds.height>double(ih))
            throw std::invalid_argument("TMA inferred core lies outside image; review dimensions or core size");
        std::uint64_t total=0,supported=0,foreground=0,visited=0;
        const auto x0=std::max<std::int64_t>(0,static_cast<std::int64_t>(std::floor(center.x-cw/2))),x1=std::min(w,static_cast<std::int64_t>(std::ceil(center.x+cw/2)));
        const auto y0=std::max<std::int64_t>(0,static_cast<std::int64_t>(std::floor(center.y-ch/2))),y1=std::min(h,static_cast<std::int64_t>(std::ceil(center.y+ch/2)));
        for(auto y=y0;y<y1;++y) for(auto x=x0;x<x1;++x) {
            if((visited++&4095)==0) checkpoint(stop);
            const auto dx=(double(x)+.5-center.x)/(cw/2),dy=(double(y)+.5-center.y)/(ch/2);
            if(dx*dx+dy*dy>1) continue;
            ++total;const auto i=static_cast<std::size_t>(y*w+x);supported+=valid[i]!=0;foreground+=mask[i]!=0;
        }
        if(!total || !supported) throw std::invalid_argument("TMA inferred core has no valid support");
        core.valid_fraction=double(supported)/double(total);core.tissue_density=double(foreground)/double(supported);
        if(core.valid_fraction<o.min_valid_fraction) throw std::invalid_argument("TMA inferred core has insufficient valid support");
        result->grid.cores.push_back({layout->row_labels()[row]+"-"+layout->column_labels()[col],"",bounds,core.tissue_density<o.missing_density});result->cores.push_back(core);
    }
    result->grid.validate();Sha256 hash;const std::string domain="qpx.tma-preview-input/1";hash.update(domain.data(),domain.size());
    hash.integer(iw);hash.integer(ih);hash.integer(w);hash.integer(h);
    for(std::size_t at=0;at<rgba.size();at+=65536) {checkpoint(stop);hash.update(rgba.data()+at,std::min<std::size_t>(65536,rgba.size()-at));}
    result->input_sha256=hash.finish();
    result->recipe_sha256=recipe(*result,o);checkpoint(stop);return result;
}
std::shared_ptr<const TmaDearrayResult> detect_tma_grid(const PixelBuffer& preview,std::int64_t iw,std::int64_t ih,TmaDearrayOptions o,const std::atomic<bool>* stop) {
    return detect_preview(preview,iw,ih,o,stop);
}
std::shared_ptr<const TmaDearrayResult> detect_tma_grid(const ImageServer& server,TmaDearrayOptions o,const std::atomic<bool>* stop) {
    checkpoint(stop);o.validate();if(server.closed()) throw std::runtime_error("TMA source is closed");
    const auto metadata=server.metadata();const auto plan=plan_thumbnail(metadata,o.thumbnail);
    geometry(plan.width,plan.height,metadata.width,metadata.height,o);
    const auto required=tma_dearray_work_bytes(plan.width,plan.height,o)+plan.source_pixels*4;
    if(required>o.work_bytes) throw std::length_error("TMA source and detection exceed working payload budget");
    const auto nx=std::uint64_t(plan.source_width+plan.width-std::gcd(plan.source_width,plan.width));
    const auto ny=std::uint64_t(plan.source_height+plan.height-std::gcd(plan.source_height,plan.height));
    if(nx>(1ULL<<28)/ny) throw std::length_error("TMA preview resampling exceeds overlap work limit");
    PixelBuffer preview(1,1);
    {
        auto source=server.read_region({0,0,plan.level,plan.source_width,plan.source_height});checkpoint(stop);
        if(source.width()!=plan.source_width || source.height()!=plan.source_height || source.bytes().size()!=plan.source_pixels*4)
            throw std::runtime_error("TMA reader returned unexpected dimensions or bytes");
        preview=resample_area(source,plan.width,plan.height,stop);
    }
    auto result=detect_preview(preview,metadata.width,metadata.height,o,stop);
    result->thumbnail_plan=plan;result->estimated_work_bytes=required;result->recipe_sha256=recipe(*result,o);
    if(server.closed() || !same_metadata(metadata,server.metadata())) throw std::runtime_error("TMA source metadata changed during dearraying");
    checkpoint(stop);return result;
}
}

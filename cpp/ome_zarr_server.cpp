#include "qupathxx/ome_zarr.hpp"
#include "qupathxx/workflow.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <locale>
#include "ome_zarr_internal.hpp"
namespace qpx {
void OmeZarrOpenOptions::validate()const{source.validate();view.validate();if(view.series || work_bytes<16u*1024*1024 || work_bytes>1024ull*1024*1024)throw std::invalid_argument("Invalid OME-Zarr image/view work options");}
std::string ome_zarr_view_encode(const TiffView& view){
    view.validate(true);if(view.series)throw std::invalid_argument("OME-Zarr image group has one series");std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17)<<"qpx.ome-zarr-view/1\n"<<view.z<<' '<<view.t<<'\n';for(std::size_t i=0;i<3;++i)out<<view.channels[i]<<' '<<view.lower[i]<<' '<<view.upper[i]<<'\n';const auto text=out.str();Sha256 hash;hash.update(text.data(),text.size());return text+hash.finish();
}
TiffView ome_zarr_view_decode(const std::string& text){
    if(text.size()<80 || text.size()>4096)throw std::invalid_argument("Invalid OME-Zarr view length");const auto n=text.size()-64;Sha256 hash;hash.update(text.data(),n);if(hash.finish()!=text.substr(n))throw std::invalid_argument("OME-Zarr view checksum mismatch");std::istringstream in(text.substr(0,n));in.imbue(std::locale::classic());TiffView view;std::string header;std::getline(in,header);if(header!="qpx.ome-zarr-view/1" || !(in>>view.z>>view.t))throw std::invalid_argument("Invalid OME-Zarr view");for(std::size_t i=0;i<3;++i)if(!(in>>view.channels[i]>>view.lower[i]>>view.upper[i]))throw std::invalid_argument("Invalid OME-Zarr channel");in>>std::ws;if(!in.eof() || ome_zarr_view_encode(view)!=text)throw std::invalid_argument("Noncanonical OME-Zarr view");return view;
}
#ifdef QUPATHXX_HAS_OME_ZARR
namespace {
class ZarrServer final:public ImageServer {
    std::shared_ptr<const OmeZarrSource> source_;std::shared_ptr<const RawImageMetadata> raw_;ImageMetadata image_;
    std::shared_ptr<const OmeZarrOpenOptions> options_;std::shared_ptr<const TiffView> view_;std::shared_ptr<const ReaderSource> observed_;std::atomic<bool> closed_{false};
#ifdef QUPATHXX_HAS_NGFF05
    const detail::Ngff05Display* producer_display_=nullptr;

    template<class Target>
    void composite_producer_display(RawRegionRequest block,std::size_t work,
        std::vector<std::uint8_t>& rgba,const Target& target)const{
        const auto pixels=std::size_t(block.region.width*block.region.height);
        if(producer_display_->channels.empty()){
            const auto raw=source_->read(block,work);
            for(std::size_t pixel=0;pixel<pixels;++pixel)rgba[target(pixel)+3]=raw.valid()[pixel]?255:0;
            return;
        }
        std::vector<std::array<double,3>> sums(pixels);
        for(const auto& channel:producer_display_->channels){
            block.first_channel=channel.channel;
            const auto raw=source_->read(block,work);
            std::visit([&](const auto& values){
                for(std::size_t pixel=0;pixel<pixels;++pixel){
                    const auto value=static_cast<double>(values[pixel]);
                    if(!raw.valid()[pixel] || !std::isfinite(value)){
                        rgba[target(pixel)+3]=0;
                        continue;
                    }
                    auto intensity=value<=channel.start?0.:value>=channel.end?1.:
                        (value-channel.start)/(channel.end-channel.start);
                    if(channel.inverted)intensity=1-intensity;
                    for(std::size_t component=0;component<3;++component)
                        sums[pixel][component]+=intensity*channel.rgb[component];
                }
            },raw.storage());
        }
        for(std::size_t pixel=0;pixel<pixels;++pixel){
            const auto at=target(pixel);
            if(rgba[at+3])for(std::size_t component=0;component<3;++component)
                rgba[at+component]=static_cast<std::uint8_t>(std::floor(std::min(255.,sums[pixel][component])+.5));
        }
    }
#endif
    void available(const std::atomic<bool>* c=nullptr)const{workflow_checkpoint(c);if(closed_)throw std::runtime_error("OME-Zarr image server is closed");}
    std::size_t view_work_bytes()const{return sizeof(*this)+raw_->payload_bytes()+observed_->payload_bytes()+image_.levels.capacity()*sizeof(ImageLevel)+image_.vendor.capacity()+4096;}
public:
    ZarrServer(std::shared_ptr<const OmeZarrSource> source,OmeZarrOpenOptions options,const std::atomic<bool>* c
#ifdef QUPATHXX_HAS_NGFF05
        ,const detail::Ngff05Display* display=nullptr
#endif
        ):source_(std::move(source)){
        options.validate();source_->require_current(c);raw_=std::make_shared<const RawImageMetadata>(source_->metadata());auto& v=options.view;
#ifdef QUPATHXX_HAS_NGFF05
        producer_display_=display;
#endif
        if(v.z>=raw_->z_planes || v.t>=raw_->timepoints)throw std::out_of_range("OME-Zarr view plane outside source");
        if(v.channels[0]==-2)for(unsigned color=0;color<3;++color)v.channels[color]=raw_->channels==1?0:color<raw_->channels?std::int32_t(color):-1;
        RawPixelBuffer type(1,1,1,raw_->sample_type);std::visit([&](const auto& values){using T=typename std::decay_t<decltype(values)>::value_type;
            for(unsigned color=0;color<3;++color){if(v.channels[color]>=std::int32_t(raw_->channels))throw std::out_of_range("OME-Zarr view channel outside source");if(std::isnan(v.lower[color])){if constexpr(std::is_floating_point_v<T>){v.lower[color]=0;v.upper[color]=1;}else{v.lower[color]=double(std::numeric_limits<T>::lowest());v.upper[color]=double(std::numeric_limits<T>::max());}}}
        },type.storage());v.validate(true);view_=std::make_shared<const TiffView>(v);options_=std::make_shared<const OmeZarrOpenOptions>(options);
        image_.width=raw_->width;image_.height=raw_->height;image_.levels=raw_->levels;image_.pixel_width_um=raw_->pixel_width_um;image_.pixel_height_um=raw_->pixel_height_um;image_.vendor="OME-Zarr NGFF0.4";
        const auto text=ome_zarr_view_encode(v);const auto original=source_->reader_source();observed_=make_directory_reader_source("ome-zarr:"+text.substr(text.size()-64),source_->directory(),original->primary,original->directory_sha256);
    }
    const ImageMetadata& metadata()const override{return image_;}
    std::string reader_id()const override{return "ome-zarr";}
    std::string source_locator()const override{return source_->directory();}
    std::shared_ptr<const OmeZarrSource> source()const{return source_;}
    std::shared_ptr<const ReaderSource> reader_source()const override{return observed_;}
    std::shared_ptr<const RawImageMetadata> raw_metadata(const std::atomic<bool>* c=nullptr)const override{available(c);return raw_;}
    std::shared_ptr<const TiffView> raw_view()const override{return view_;}
    std::shared_ptr<const OmeZarrOpenOptions> ome_zarr_options()const override{return options_;}
    void require_raw_current(const std::atomic<bool>* c=nullptr)const override{available(c);source_->require_current(c);}
    RawPixelBuffer read_raw_coordinates(const RawRegionRequest& r,const std::atomic<bool>* c,bool level_coordinates)const{
        available(c);
        const auto own=view_work_bytes();
        if(own>=options_->work_bytes)throw WorkflowCaptureLimit("OME-Zarr view metadata exceeds work limit");
        const auto work=options_->work_bytes-own;
        auto result=level_coordinates?source_->read_level(r,work,c):source_->read(r,work,c);
        available(c);
        return result;
    }
    RawPixelBuffer read_raw(const RawRegionRequest& r,const std::atomic<bool>* c=nullptr)const override{return read_raw_coordinates(r,c,false);}
    std::shared_ptr<ImageServer> with_raw_view(const TiffView& view,const std::atomic<bool>* c=nullptr)const override{available(c);auto options=*options_;options.view=view;return std::make_shared<ZarrServer>(source_,options,c);}
    PixelBuffer read_region(const RegionRequest& r)const override{
        available();validate_region(image_,r);const auto output_bytes=region_byte_count(r.width,r.height),own=view_work_bytes();const auto factor=std::int64_t(image_.levels[r.level].downsample);
        RawRegionRequest block;block.region={r.x,r.y,r.level,std::min<std::int64_t>(256,r.width),std::min<std::int64_t>(256,r.height)};block.z=view_->z;block.t=view_->t;
        const auto required=std::max(source_->read_work_bytes(block),2*source_->payload_bytes());
        std::size_t accumulator_bytes=0;
#ifdef QUPATHXX_HAS_NGFF05
        if(producer_display_){
            if(!producer_display_->error.empty())throw std::invalid_argument(producer_display_->error);
            if(!producer_display_->channels.empty())
                accumulator_bytes=std::size_t(block.region.width*block.region.height)*sizeof(std::array<double,3>);
        }
#endif
        if(own>=options_->work_bytes || output_bytes>=options_->work_bytes-own ||
           accumulator_bytes>options_->work_bytes-own-output_bytes ||
           required>options_->work_bytes-own-output_bytes-accumulator_bytes)
            throw WorkflowCaptureLimit("OME-Zarr RGB output and decoder exceed shared work limit");
        const auto read_work=options_->work_bytes-own-output_bytes-accumulator_bytes;
        require_raw_current();
        PixelBuffer output(r.width,r.height);auto& rgba=output.writable_bytes();std::vector<std::int32_t> channels;for(auto c:view_->channels)if(c>=0 && std::find(channels.begin(),channels.end(),c)==channels.end())channels.push_back(c);
        for(std::int64_t y=0;y<r.height;y+=256)for(std::int64_t x=0;x<r.width;x+=256){available();block.region={r.x+x*factor,r.y+y*factor,r.level,std::min<std::int64_t>(256,r.width-x),std::min<std::int64_t>(256,r.height-y)};
            const auto target=[&](std::size_t i){return (std::size_t(y+std::int64_t(i/std::size_t(block.region.width)))*std::size_t(r.width)+std::size_t(x)+i%std::size_t(block.region.width))*4;};
            const auto pixels=std::size_t(block.region.width*block.region.height);for(std::size_t i=0;i<pixels;++i)rgba[target(i)+3]=255;
#ifdef QUPATHXX_HAS_NGFF05
            if(producer_display_){
                composite_producer_display(block,read_work,rgba,target);
                continue;
            }
#endif
            for(auto channel:channels){block.first_channel=std::uint32_t(channel);const auto raw=source_->read(block,read_work);
                std::visit([&](const auto& values){for(std::size_t i=0;i<pixels;++i){const auto at=target(i);const auto value=double(values[i]);if(!raw.valid()[i] || !std::isfinite(value)){rgba[at+3]=0;continue;}for(unsigned color=0;color<3;++color)if(view_->channels[color]==channel){const auto lo=view_->lower[color],hi=view_->upper[color];const auto fraction=value<=lo?0:value>=hi?1:(value-lo)/(hi-lo);rgba[at+color]=std::uint8_t(std::floor(fraction*255+.5));}}},raw.storage());
            }
            for(std::size_t i=0;i<pixels;++i){const auto at=target(i);if(!rgba[at+3])rgba[at]=rgba[at+1]=rgba[at+2]=0;}
        }require_raw_current();return output;
    }
    void close()override{closed_=true;}
    bool closed()const override{return closed_;}
};
}
std::shared_ptr<const OmeZarrSource> detail::capture_native_ome_zarr_source(const ImageServer& server){
    const auto* native=dynamic_cast<const ZarrServer*>(&server);
    return native?native->source():nullptr;
}
RawPixelBuffer detail::read_native_ome_zarr_level(const ImageServer& server,const RawRegionRequest& r,const std::atomic<bool>* c){
    const auto* native=dynamic_cast<const ZarrServer*>(&server);
    if(!native)throw std::invalid_argument("Level-array reads require a native OME-Zarr server");
    return native->read_raw_coordinates(r,c,true);
}
std::shared_ptr<ImageServer> open_ome_zarr(const std::string& path,OmeZarrOpenOptions options,const std::atomic<bool>* c){options.validate();return std::make_shared<ZarrServer>(std::make_shared<OmeZarrSource>(path,options.source,c),options,c);}
#ifdef QUPATHXX_HAS_NGFF05
std::shared_ptr<ImageServer> detail::open_ngff05_image(const std::string& path,OmeZarrOpenOptions options,const std::atomic<bool>* c){
    options.validate();
    options.source.inventory_bytes=std::min(options.source.inventory_bytes,options.work_bytes/4);
    options.source.metadata_bytes=std::min(options.source.metadata_bytes,options.work_bytes/64);
    options.source.chunk_bytes=std::min(options.source.chunk_bytes,options.work_bytes/8);
    options.source.encoded_bytes=std::min(options.source.encoded_bytes,options.work_bytes/4);
    auto source=Ngff05SourceFactory::open(path,options.source,c);
    const auto display=Ngff05SourceFactory::display(*source);
    return std::make_shared<ZarrServer>(std::move(source),options,c,display);
}
#endif
#else
std::shared_ptr<const OmeZarrSource> detail::capture_native_ome_zarr_source(const ImageServer&){return {};}
RawPixelBuffer detail::read_native_ome_zarr_level(const ImageServer&,const RawRegionRequest&,const std::atomic<bool>*){throw std::runtime_error("OME-Zarr support was not enabled");}
std::shared_ptr<ImageServer> open_ome_zarr(const std::string&,OmeZarrOpenOptions,const std::atomic<bool>*){throw std::runtime_error("OME-Zarr support was not enabled");}
#endif
}

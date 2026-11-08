#include "qupathxx/scientific_sampling.hpp"
#include "sha256.hpp"
#include <optional>
#include <stdexcept>

namespace qpx {
namespace {
void checkpoint(const std::atomic<bool>* cancelled) {
    if(cancelled && cancelled->load()) throw std::runtime_error("scientific RGB sampling cancelled");
}
std::int64_t numerator(const ScientificGridAxis& axis,std::int64_t index) {
    return axis.origin()*axis.samples()+index*axis.extent();
}
}
ScientificRgbGrid read_scientific_rgb(const ImageServer& server,const ScientificGrid& grid,
    const std::atomic<bool>* cancelled) {
    checkpoint(cancelled);
    const auto& metadata=server.metadata();
    if(server.closed()) throw std::runtime_error("scientific RGB reader is closed");
    if(metadata.width!=grid.image_width() || metadata.height!=grid.image_height() || metadata.levels.empty() ||
       metadata.levels[0].width!=metadata.width || metadata.levels[0].height!=metadata.height ||
       metadata.levels[0].downsample!=1. || metadata.pixel_type!=PixelType::UInt8 || metadata.layout!=PixelLayout::RGBA)
        throw std::invalid_argument("scientific RGB grid requires matching unit-scale level-zero RGBA8 metadata");
    const auto& ax=grid.x();const auto& ay=grid.y();
    // The global overlap cap implies this bound; retain the explicit guard so
    // exact integer accumulation cannot overflow if admission evolves later.
    if(std::uint64_t(ax.extent())>(1ULL<<28)/std::uint64_t(ay.extent()))
        throw std::length_error("scientific RGB footprint exceeds exact accumulation limit");
    const auto total=std::uint64_t(ax.extent())*std::uint64_t(ay.extent());
    const auto bounds=grid.clipped_read_bounds();
    std::optional<PixelBuffer> source;
    checkpoint(cancelled);
    if(bounds.width && bounds.height) {
        const RegionRequest request(bounds.x,bounds.y,0,bounds.width,bounds.height);
        validate_region(metadata,request);
        source.emplace(server.read_region(request));checkpoint(cancelled);
        if(source->width()!=bounds.width || source->height()!=bounds.height ||
           source->bytes().size()!=region_byte_count(bounds.width,bounds.height))
            throw std::runtime_error("scientific RGB reader returned unexpected dimensions or byte count");
    }
    Sha256 snapshot;constexpr char format[]="qpx.level-zero-rgba-window/1";
    snapshot.update(format,sizeof(format)-1);snapshot.integer(metadata.width);snapshot.integer(metadata.height);
    for(auto value:{bounds.x,bounds.y,bounds.width,bounds.height}) snapshot.integer(value);
    if(source) for(std::size_t at=0;at<source->bytes().size();) {
        checkpoint(cancelled);const auto count=std::min<std::size_t>(65536,source->bytes().size()-at);
        snapshot.update(source->bytes().data()+at,count);at+=count;
    }
    ScientificRgbGrid output(grid);output.decoded_source_sha256_=snapshot.finish();std::uint64_t visited=0;
    for(auto y=ay.first_index();y<ay.stop_index();++y) {
        checkpoint(cancelled);
        const auto first_y=ay.edge(y).floor(),stop_y=ay.edge(y+1).ceil();
        const auto top=numerator(ay,y),bottom=top+ay.extent();
        for(auto x=ax.first_index();x<ax.stop_index();++x) {
            if((++visited&4095)==0) checkpoint(cancelled);
            const auto first_x=ax.edge(x).floor(),stop_x=ax.edge(x+1).ceil();
            if(!source || first_x<0 || first_y<0 || stop_x>metadata.width || stop_y>metadata.height) continue;
            const auto left=numerator(ax,x),right=left+ax.extent();
            std::uint64_t channels[3]={};bool valid=true;
            for(auto sy=first_y;sy<stop_y && valid;++sy) {
                const auto wy=std::min(bottom,(sy+1)*ay.samples())-std::max(top,sy*ay.samples());
                for(auto sx=first_x;sx<stop_x;++sx) {
                    if((++visited&4095)==0) checkpoint(cancelled);
                    const auto index=static_cast<std::size_t>(((sy-bounds.y)*bounds.width+sx-bounds.x)*4);
                    const auto& bytes=source->bytes();
                    if(bytes[index+3]!=255) {valid=false;break;}
                    const auto wx=std::min(right,(sx+1)*ax.samples())-std::max(left,sx*ax.samples());
                    const auto weight=std::uint64_t(wx)*std::uint64_t(wy);
                    for(unsigned c=0;c<3;++c) channels[c]+=weight*bytes[index+c];
                }
            }
            if(valid) {
                const auto index=static_cast<std::size_t>((y-ay.first_index())*grid.width()+x-ax.first_index());
                output.valid_[index]=1;
                // Numerator <=255*2^28 and denominator <=2^28 are exactly
                // representable in binary64; the division rounds only once.
                for(unsigned c=0;c<3;++c) output.rgb_[index*3+c]=double(channels[c])/double(total);
            }
        }
    }
    checkpoint(cancelled);return output;
}
}

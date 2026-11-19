#pragma once
#include "qupathxx/channel_source.hpp"
#include "qupathxx/source_identity.hpp"
namespace qpx {
namespace detail { struct Ngff05SourceFactory; }
struct OmeZarrOptions {
    std::size_t metadata_bytes=1024*1024,chunk_bytes=16u*1024*1024,encoded_bytes=32u*1024*1024;
    std::size_t inventory_bytes=64u*1024*1024,max_files=100000;
    void validate()const;
};
struct OmeZarrLevel {
    std::string path;
    std::vector<std::uint64_t> shape,chunks;
    // Effective dataset transform followed by the multiscales transform, in
    // declared axis order/units. Original physical origin is never discarded.
    std::vector<double> scale,translation;
};
class OmeZarrSource final:public ChannelSource {
public:
    explicit OmeZarrSource(const std::string& directory,OmeZarrOptions={},const std::atomic<bool>* =nullptr);
    const RawImageMetadata& metadata()const override;
    const std::string& domain()const override;
    const std::string& sha256()const override;
    std::size_t payload_bytes()const override;
    std::size_t read_work_bytes(const RawRegionRequest&)const override;
    void require_current(const std::atomic<bool>* =nullptr)const override;
    RawPixelBuffer read(const RawRegionRequest&,std::size_t work_bytes=256u*1024*1024,const std::atomic<bool>* =nullptr)const override;
    // Use XY array indices at the selected level instead of level-zero coordinates.
    RawPixelBuffer read_level(const RawRegionRequest&,std::size_t work_bytes=256u*1024*1024,const std::atomic<bool>* =nullptr)const;
    const std::vector<std::string>& axes()const;
    const std::vector<std::string>& units()const;
    const std::vector<OmeZarrLevel>& levels()const;
    // XY array index of the first requested sample, including any padding.
    std::array<std::int64_t,2> region_level_origin(const RawRegionRequest&)const;
    const std::string& directory()const;
    std::shared_ptr<const ReaderSource> reader_source()const;
private:
    RawPixelBuffer read_region(const RawRegionRequest&,std::size_t,const std::atomic<bool>*,bool level_coordinates)const;
    friend struct detail::Ngff05SourceFactory;
    struct Data;std::shared_ptr<const Data> data_;
    explicit OmeZarrSource(std::shared_ptr<const Data> data):data_(std::move(data)){}
};
bool has_ome_zarr();
struct OmeZarrOpenOptions {
    OmeZarrOptions source;
    TiffView view;
    std::size_t work_bytes=256u*1024*1024;
    void validate()const;
};
std::shared_ptr<ImageServer> open_ome_zarr(const std::string&,OmeZarrOpenOptions={},const std::atomic<bool>* =nullptr);
std::string ome_zarr_view_encode(const TiffView&);
TiffView ome_zarr_view_decode(const std::string&);
}

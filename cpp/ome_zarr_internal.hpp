#pragma once
#include "qupathxx/ome_zarr.hpp"
#include "qupathxx/source_identity.hpp"
#include <nlohmann/json.hpp>
#include <array>
#include <filesystem>
#include <map>
#include <optional>
#ifdef QUPATHXX_HAS_NGFF05
#include "blosc_chunk.hpp"
#include "shard_index.hpp"
#endif
namespace qpx::detail {
using ZarrJson=nlohmann::json;
std::shared_ptr<const OmeZarrSource> capture_native_ome_zarr_source(const ImageServer&);
RawPixelBuffer read_native_ome_zarr_level(const ImageServer&,const RawRegionRequest&,const std::atomic<bool>*);
void zarr_require(bool,const std::string&);
enum class NgffLayout { v04, v05 };
class ZarrStore {
    std::map<std::string,FileVersion> files_;
    std::size_t payload_=0;
    std::map<std::string,FileVersion> observe(const std::atomic<bool>*)const;
    std::vector<std::uint8_t> read_bytes(const std::string&,std::uint64_t,
        std::optional<std::size_t>,std::size_t,bool)const;
public:
    std::filesystem::path root;
    OmeZarrOptions options;
    std::string id;
    ZarrStore(const std::string&,OmeZarrOptions,const std::atomic<bool>*);
#ifdef QUPATHXX_HAS_NGFF05
private:
    const NgffLayout layout_;
public:
    ZarrStore(const std::string&,OmeZarrOptions,const std::atomic<bool>*,NgffLayout);
    NgffLayout layout()const{return layout_;}
    std::vector<std::uint8_t> read_range(const std::string& key,std::uint64_t offset,std::size_t bytes,std::size_t limit)const;
    std::uint64_t member_bytes(const std::string& key)const{return files_.at(key).bytes;}
#else
    constexpr NgffLayout layout()const{return NgffLayout::v04;}
#endif
    std::size_t payload_bytes()const{return payload_;}
    void require_current(const std::atomic<bool>*)const;
    // Metadata must exist. A chunk absent at observation remains a declared
    // hole, not a catch-all for an unreadable, deleted or corrupt source file.
    std::vector<std::uint8_t> read(const std::string&,std::size_t,bool missing_allowed=false)const;
    bool contains(const std::string&)const;
    void verify(const std::string&)const;
    const char* primary_key()const{return layout()==NgffLayout::v05?"zarr.json":".zattrs";}
    std::shared_ptr<const FileVersion> metadata_version()const{return std::make_shared<const FileVersion>(files_.at(primary_key()));}
};
struct ZarrArray {
    OmeZarrLevel level;
    SampleType type;
    bool big_endian=false,fortran=false,undefined_fill=false;
    bool decoded_crc32c=false,encoded_crc32c=false;
    // Logical axis at each position of the serialized row-major chunk.
    std::array<std::size_t,5> sample_axes{0,1,2,3,4};
    std::string separator=".",codec;
#ifdef QUPATHXX_HAS_NGFF05
    bool checksum=false;
    Ngff05Blosc blosc;
    struct Shard {
        std::array<std::uint64_t,5> chunks_per_axis{};
        std::array<std::size_t,5> index_strides{};
        std::size_t entries=0;
        ShardIndexEncoding index_encoding;
        bool index_at_start=false,crc32c=false;
    } shard;
#endif
    ZarrJson fill;
    std::optional<std::uint64_t> floating_fill_bits;
    std::size_t decoded_bytes=0;
};
struct Ngff05DisplayChannel {
    std::uint32_t channel=0;
    std::array<std::uint8_t,3> rgb{};
    double start=0,end=1;
    bool inverted=false;
};
struct Ngff05Display {
    std::vector<Ngff05DisplayChannel> channels;
    std::string error;
};
struct ZarrMetadata {
    RawImageMetadata raw;
    std::vector<std::string> axes,units;
    std::vector<ZarrArray> arrays;
    std::vector<OmeZarrLevel> levels;
    std::optional<Ngff05Display> display;
    int x=-1,y=-1,c=-1,z=-1,t=-1;
    std::size_t payload=0;
};
ZarrMetadata zarr_metadata(const ZarrStore&,const std::atomic<bool>*);
#ifdef QUPATHXX_HAS_NGFF05
struct Ngff05SourceFactory {
    static std::shared_ptr<const OmeZarrSource> open(const std::string&,OmeZarrOptions,const std::atomic<bool>*);
    static const Ngff05Display* display(const OmeZarrSource&);
};
std::shared_ptr<ImageServer> open_ngff05_image(const std::string&,OmeZarrOpenOptions,const std::atomic<bool>*);
#endif
}

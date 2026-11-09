#pragma once
#include "qupathxx/ome_zarr.hpp"
#include "qupathxx/source_identity.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <map>
namespace qpx::detail {
using ZarrJson=nlohmann::json;
void zarr_require(bool,const std::string&);
class ZarrStore {
    std::map<std::string,FileVersion> files_;
    std::size_t payload_=0;
    std::map<std::string,FileVersion> observe(const std::atomic<bool>*)const;
public:
    std::filesystem::path root;
    OmeZarrOptions options;
    std::string id;
    ZarrStore(const std::string&,OmeZarrOptions,const std::atomic<bool>*);
    std::size_t payload_bytes()const{return payload_;}
    void require_current(const std::atomic<bool>*)const;
    // Metadata must exist. A chunk absent at observation remains a declared
    // hole, not a catch-all for an unreadable, deleted or corrupt source file.
    std::vector<std::uint8_t> read(const std::string&,std::size_t,bool missing_allowed=false)const;
    bool contains(const std::string&)const;
    void verify(const std::string&)const;
};
struct ZarrArray {
    OmeZarrLevel level;
    SampleType type;
    bool big_endian=false,fortran=false,undefined_fill=false;
    std::string separator=".",codec;
    ZarrJson fill;
    std::size_t decoded_bytes=0;
};
struct ZarrMetadata {
    RawImageMetadata raw;
    std::vector<std::string> axes,units;
    std::vector<ZarrArray> arrays;
    std::vector<OmeZarrLevel> levels;
    int x=-1,y=-1,c=-1,z=-1,t=-1;
    std::size_t payload=0;
};
ZarrMetadata zarr_metadata(const ZarrStore&,const std::atomic<bool>*);
}

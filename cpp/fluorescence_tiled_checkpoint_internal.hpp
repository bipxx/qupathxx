#pragma once
#include "qupathxx/fluorescence_tiled_checkpoint.hpp"
#include <filesystem>
namespace qpx::detail {
struct FluorescenceThresholdCheckpoint {double threshold=0;std::vector<std::string> inputs;};
class FluorescenceCheckpointStore {
    std::filesystem::path root_;
    FluorescenceCheckpointOptions options_;
    std::string request_,source_;
    std::size_t count_=0,manifest_limit_=0,reserved_=0;
    std::uint64_t bytes_=0;
    bool otsu_=false;
    std::size_t last_index_=SIZE_MAX;std::string last_digest_;
    std::vector<bool> present_,partial_;
    const std::atomic<bool>* cancelled_;
    std::filesystem::path tile_path(std::size_t) const;
    std::vector<std::uint8_t> read(const std::filesystem::path&,std::size_t) const;
    void write(const std::filesystem::path&,const std::vector<std::uint8_t>&);
    std::string save_record(std::size_t,double,const FluorescenceTileResult&,bool);
public:
    FluorescenceCheckpointStore(const std::string&,const std::string& request,const std::string& source,
        std::size_t count,bool otsu,FluorescenceCheckpointOptions,std::size_t work,const std::atomic<bool>*);
    const std::string path() const{return root_.u8string();}
    std::size_t reserved_bytes() const{return reserved_;}
    std::optional<FluorescenceThresholdCheckpoint> global();
    void global(double,const std::vector<std::string>&);
    bool has_tile(std::size_t i)const{return present_.at(i);}
    FluorescenceTileResult load(std::size_t,double,const std::string& recipe,const RawRegionRequest&,
        RegionRequest core,RegionRequest bounds,SampleType);
    std::string save(std::size_t,double,const FluorescenceTileResult&);
    std::string save_partial(std::size_t,double,const FluorescenceTileResult&);
    std::string tile_digest(std::size_t) const;
    void finish(const FluorescenceTiledReport&);
};
}

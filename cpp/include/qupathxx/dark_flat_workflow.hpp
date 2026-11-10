#pragma once
#include "qupathxx/channel_source.hpp"
#include <array>
namespace qpx {
class Document;
struct DarkFlatSourceReference {
    std::string path,reader="tiff",source_sha256;
    std::uint32_t series=0;
};
struct DarkFlatRecipe {
    // Signal, dark, flat, in that order. Initial automatic reopen supports TIFF.
    std::array<DarkFlatSourceReference,3> sources;
    DarkFlatOptions options;
    std::string derived_source_sha256;
    void validate() const;
    std::string sha256() const;
    std::size_t payload_bytes() const;
};
class DarkFlatRequest {
public:
    DarkFlatRequest(const Document&,std::shared_ptr<ImageServer> dark,
        std::shared_ptr<ImageServer> flat,DarkFlatOptions,const std::atomic<bool>* cancelled=nullptr);
    bool matches(const Document&) const;
    void require_current(const Document&,const std::atomic<bool>* cancelled=nullptr) const;
    const DarkFlatRecipe& recipe() const;
    const std::shared_ptr<const ChannelSource>& source() const;
private:
    struct Data;std::shared_ptr<const Data> data_;
};
// Exact typed image-properties storage, one undoable metadata command. No
// pixels/results or rewritten original-channel observations are stored here.
std::string keep_dark_flat_recipe(Document&,const DarkFlatRequest&,const std::atomic<bool>* cancelled=nullptr);
std::vector<std::string> dark_flat_recipe_ids(const Document&);
DarkFlatRecipe load_dark_flat_recipe(const Document&,const std::string&);
void remove_dark_flat_recipe(Document&,const std::string&);
DarkFlatRequest replay_dark_flat_recipe(const Document&,const std::string&,const std::atomic<bool>* cancelled=nullptr);
// Restore after fields were opened on a worker. The owner captures Document;
// the same native identity check is used by automatic replay and desktop load.
DarkFlatRequest restore_dark_flat_recipe(const Document&,const DarkFlatRecipe&,
    std::shared_ptr<ImageServer> dark,std::shared_ptr<ImageServer> flat,
    const std::atomic<bool>* cancelled=nullptr);
class DarkFlatBlock {
public:
    const RawPixelBuffer& samples() const{return samples_;}
    const RawRegionRequest& region() const{return region_;}
    const std::string& sha256() const{return sha_;}
    const std::string& source_sha256() const{return source_;}
    std::uint64_t valid_pixels() const{return valid_;}
    const std::vector<std::uint64_t>& finite_samples() const{return finite_;}
    const std::vector<std::uint64_t>& nonfinite_samples() const{return nonfinite_;}
private:
    DarkFlatBlock(RawPixelBuffer,RawRegionRequest,std::string,const std::atomic<bool>*);
    RawPixelBuffer samples_;RawRegionRequest region_;std::string sha_,source_;
    std::uint64_t valid_=0;std::vector<std::uint64_t> finite_,nonfinite_;
    friend std::shared_ptr<const DarkFlatBlock> read_dark_flat_block(const DarkFlatRequest&,RawRegionRequest,std::size_t,const std::atomic<bool>*);
};
// Native workers read only immutable source/recipe snapshots, never Document.
std::shared_ptr<const DarkFlatBlock> read_dark_flat_block(const DarkFlatRequest&,RawRegionRequest,
    std::size_t work_bytes=256u*1024*1024,const std::atomic<bool>* cancelled=nullptr);
}

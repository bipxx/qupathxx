#pragma once
#include "qupathxx/raw_pixel_frontend.hpp"
namespace qpx {class Project;}
namespace qpx::detail {
// Owner-thread adapter over the existing typed jobs. One active operation;
// workers never access Document. Used by desktop and managed-viewer consumers.
class RawPixelFrontend {
public:
    RawPixelFrontend();~RawPixelFrontend();RawPixelFrontend(const RawPixelFrontend&)=delete;
    bool busy() const;void cancel();void wait(const Document&);
    void poll(const Document&);
    void train(const Document&,const std::vector<std::uint64_t>&,RawPixelTrainingSettings);
    void predict(const Document&,const RawPixelClassifier&,ScientificRawWindow,std::uint64_t,double,std::size_t);
    void save(const Document&,const RawPixelClassifier&,ScientificRawLattice,const std::string&,RawPixelStreamOptions,RawPixelStreamSaveOptions,std::uint64_t);
    void open(const std::string&,RawPixelStreamFileLimits,std::uint64_t,std::shared_ptr<const RawPixelStreamLink> link={});
    void load_model(const std::string&);void save_model(const std::string&,bool);
    void adopt(RawPixelClassifier);
    void discard_contours();
    // Fresh bounded full-grid inference is required even after historical reads.
    void trace(const Document&,const RawPixelClassifier&,ScientificGrid,TissueContourOptions={},std::uint64_t object_id=0,double min_vote=0,std::size_t work_bytes=16u*1024*1024);
    const std::vector<std::uint64_t>& publish(Document&,const std::string& name_prefix="Raw pixel");
    std::uint64_t keep_training(Document&);std::uint64_t keep_link(Document&);
    bool can_keep_training(const Document&) const;void reset_records();
    GuiRawPixelReport report() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
// The file jobs guard reader inputs; frontends also guard project/recovery data.
void guard_raw_pixel_frontend_output(const Project*,const std::string&);
ScientificRawLattice raw_pixel_frontend_lattice(const Document&,const std::optional<ScientificRawLattice>&);
}

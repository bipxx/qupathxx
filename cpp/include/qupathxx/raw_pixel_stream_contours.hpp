#pragma once
#include "qupathxx/raw_pixel_stream.hpp"
#include "qupathxx/tissue.hpp"
#include "qupathxx/raw_pixel_stream_file.hpp"
namespace qpx {
struct RawPixelStreamContourOptions {
    TissueContourOptions contours;
    std::size_t max_boundary_runs=1000000,boundary_work_bytes=64u*1024*1024;
    // Captured producer work plus the larger boundary/geometry phase and bookkeeping.
    // Conservative admission, not process RSS.
    std::size_t work_bytes=512u*1024*1024;
    void validate() const;
};
// Immutable verified historical geometry. It contains no live capture and cannot
// be passed to publish_raw_pixel_stream_contours.
class RawPixelFileContours {
public:
    RawPixelFileContours(const RawPixelFileContours&)=delete;
    const std::shared_ptr<const TissueContours>& contours() const {return contours_;}
    const std::shared_ptr<const RawPixelStreamLink>& link() const {return link_;}
    const std::shared_ptr<const RawPixelStreamFile>& file() const {return file_;}
    const std::vector<std::string>& class_names() const {return classes_;}
    const RawPixelStreamContourOptions& options() const {return options_;}
    std::uint64_t completed_samples() const {return samples_;}
    std::size_t estimated_work_bytes() const {return work_;}
    std::size_t payload_bytes() const;
private:
    RawPixelFileContours()=default;
    std::shared_ptr<const TissueContours> contours_;std::shared_ptr<const RawPixelStreamLink> link_;
    std::shared_ptr<const RawPixelStreamFile> file_;
    std::vector<std::string> classes_;RawPixelStreamContourOptions options_;
    std::uint64_t samples_=0;std::size_t work_=0;
    friend struct RawPixelFileContourKernel;
};
std::shared_ptr<const RawPixelFileContours> trace_raw_pixel_stream_file(std::shared_ptr<const RawPixelStreamFile>,
    RawPixelStreamContourOptions={},const std::atomic<bool>* cancelled=nullptr);
// Requires a complete verified file. Construct a NEW owner-thread capture from
// the current document; matching metadata does not authorize historical pixels.
RawPixelTiledPredictionRequest capture_raw_pixel_stream_replay(const Document&,const RawPixelStreamFile&);
class RawPixelStreamContours {
public:
    RawPixelStreamContours(const RawPixelStreamContours&)=delete;
    const std::shared_ptr<const TissueContours>& contours() const {return contours_;}
    const std::string& request_sha256() const;
    const std::string& input_sha256() const {return input_;}
    const std::string& prediction_sha256() const {return prediction_;}
    const ScientificRawLattice& lattice() const;
    const RawPixelStreamRegion& region() const;
    const std::vector<std::uint64_t>& class_counts() const {return counts_;}
    std::uint64_t completed_windows() const {return windows_;}
    std::uint64_t completed_samples() const {return samples_;}
    std::uint64_t valid_samples() const {return valid_;}
    std::size_t estimated_work_bytes() const {return work_;}
    std::size_t payload_bytes() const;
private:
    RawPixelStreamContours()=default;
    std::shared_ptr<const RawPixelTiledPredictionRequest::Data> snapshot_;
    std::shared_ptr<const TissueContours> contours_;
    std::shared_ptr<const RawPixelStreamFile> replay_file_;
    std::string input_,prediction_;std::vector<std::uint64_t> counts_;
    std::uint64_t windows_=0,samples_=0,valid_=0;std::size_t work_=0;
    friend struct RawPixelStreamContourKernel;
};
std::shared_ptr<const RawPixelStreamContours> trace_raw_pixel_stream(const RawPixelTiledPredictionRequest&,
    RawPixelStreamContourOptions={},const std::atomic<bool>* cancelled=nullptr);
// Re-executes prediction against the fresh request and compares all complete
// saved commitments. The returned live result retains the file version guard.
std::shared_ptr<const RawPixelStreamContours> trace_raw_pixel_stream_replay(const RawPixelTiledPredictionRequest&,
    std::shared_ptr<const RawPixelStreamFile>,RawPixelStreamContourOptions={},const std::atomic<bool>* cancelled=nullptr);
std::vector<std::uint64_t> publish_raw_pixel_stream_contours(Document&,const RawPixelTiledPredictionRequest&,
    const RawPixelStreamContours&,std::string name_prefix="Raw pixel",std::size_t work_bytes=64u*1024*1024);
// Owner-thread controller; only cancel() may run on another thread. It owns one
// whole-stream worker/result slot and never exposes partial geometry for apply.
class RawPixelStreamContourJob {
public:
    RawPixelStreamContourJob();~RawPixelStreamContourJob();
    RawPixelStreamContourJob(const RawPixelStreamContourJob&)=delete;
    bool request(const RawPixelTiledPredictionRequest&,RawPixelStreamContourOptions={});
    bool request_replay(const RawPixelTiledPredictionRequest&,std::shared_ptr<const RawPixelStreamFile>,RawPixelStreamContourOptions={});
    bool busy() const;void cancel();void await_completion();
    RawPixelStreamJobStats poll(const Document&),wait(const Document&);
    std::shared_ptr<const RawPixelStreamContours> result() const;
private:
    bool start(const RawPixelTiledPredictionRequest&,RawPixelStreamContourOptions,std::shared_ptr<const RawPixelStreamFile>);
    struct Impl;std::unique_ptr<Impl> impl_;
};
}

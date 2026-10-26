#pragma once
#include "qupathxx/raw_pixel_stream_file.hpp"
#include "qupathxx/raw_pixel_training_checkpoint.hpp"

namespace qpx {
struct RawPixelFileJobStats {
    std::string state="idle",phase="idle",error;
    std::uint64_t generation=0,total_windows=0,completed_windows=0;
    double elapsed_ms=0;
};
// Controllers have one worker/result slot. All methods except cancel run on
// the constructing thread. await_completion joins without publishing; take
// checks fresh document/source/output identity before transferring the result.
// Destruction cancels and joins before releasing captured readers. A completed
// atomic disk commit is historical output: later owner staleness suppresses its
// delivery but does not delete it or roll it back.
class RawPixelStreamSaveJob {
public:
    RawPixelStreamSaveJob();~RawPixelStreamSaveJob();
    RawPixelStreamSaveJob(const RawPixelStreamSaveJob&)=delete;
    bool request(const RawPixelTiledPredictionRequest&,const std::string&,RawPixelStreamSaveOptions={});
    bool busy() const;void cancel();void await_completion();
    RawPixelFileJobStats poll(const Document&),wait(const Document&);
    std::optional<RawPixelStreamFileSummary> take(const Document&);
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
class RawPixelTrainingCheckpointJob {
public:
    RawPixelTrainingCheckpointJob();~RawPixelTrainingCheckpointJob();
    RawPixelTrainingCheckpointJob(const RawPixelTrainingCheckpointJob&)=delete;
    bool request(const RawPixelTiledTrainingRequest&,const std::string&,RawPixelTrainingCheckpointOptions={});
    bool busy() const;void cancel();void await_completion();
    RawPixelFileJobStats poll(const Document&),wait(const Document&);
    std::optional<RawPixelTrainingCheckpointResult> take(const Document&);
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
// Historical IO only. open verifies the complete saved prefix in a worker;
// read validates exactly one requested block. No decode-ahead or live capture.
// Explicit take releases the pending slot; returned blocks own their arrays.
class RawPixelStreamFileReadJob {
public:
    RawPixelStreamFileReadJob();~RawPixelStreamFileReadJob();
    RawPixelStreamFileReadJob(const RawPixelStreamFileReadJob&)=delete;
    bool open(const std::string&,RawPixelStreamFileLimits={});
    bool read(std::shared_ptr<const RawPixelStreamFile>,std::uint64_t index);
    bool busy() const;void cancel();void await_completion();
    RawPixelFileJobStats poll(),wait();
    std::shared_ptr<const RawPixelStreamFile> take_file();
    std::shared_ptr<const RawPixelWindowClassification> take_block();
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}

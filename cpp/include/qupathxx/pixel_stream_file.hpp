#pragma once
#include "qupathxx/pixel_stream.hpp"
namespace qpx {
struct PixelStreamFileSummary {
    std::uint64_t bytes=0,blocks=0,valid_pixels=0,classified_pixels=0;
    std::vector<std::uint64_t> class_counts;
    std::string sha256;
};
// Numeric historical output. Opening validates bounded metadata and completion;
// read_block validates that block; verify validates every block and final totals.
class PixelStreamFile {
public:
    explicit PixelStreamFile(const std::string& path,std::uint64_t max_bytes=1ULL<<40);
    ~PixelStreamFile();PixelStreamFile(const PixelStreamFile&)=delete;
    std::shared_ptr<const PixelPredictionResult> read_block(std::uint64_t index) const;
    void verify(const std::atomic<bool>* cancelled=nullptr) const;
    void cancel();bool cancelled() const;
    bool verified() const;
    Rect level_bounds() const;std::size_t block_size() const;bool windowed() const;
    const PixelStreamFileSummary& declared_summary() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
    friend struct PixelStreamFileCodec;
    friend PixelStreamFileSummary save_pixel_stream(PixelPredictionStream&,const std::string&,bool,std::uint64_t,const std::atomic<bool>*);
};
PixelStreamFileSummary save_pixel_stream(PixelPredictionStream&,const std::string& path,bool replace_existing=false,std::uint64_t max_bytes=1ULL<<40,const std::atomic<bool>* cancelled=nullptr);
}

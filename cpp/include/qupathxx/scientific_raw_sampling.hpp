#pragma once
#include "scientific_grid.hpp"
#include "raw_image.hpp"
#include "scientific_raw_window.hpp"

namespace qpx {
enum class ScientificRawReason : std::uint8_t { Valid=0,OutsideSource=1,InvalidSource=2,NonfiniteChannel=3 };
struct ScientificRawOptions {
    std::uint32_t first_channel=0,channels=1,z=0,t=0;
    std::size_t work_bytes=256*1024*1024;
    void validate() const;
};
// Source support is per HWC element, not ROI eligibility. Precedence is outside
// footprint, invalid source pixel, then nonfinite channel. Invalid means are +0.
// Valid finite area means round once to binary64, including 64-bit integers.
class ScientificRawGrid {
    ScientificGrid grid_;
    RawImageMetadata metadata_;
    ScientificRawOptions options_;
    std::vector<double> values_;
    std::vector<std::uint8_t> valid_,reasons_;
    std::string reader_,decoded_source_sha256_,input_sha256_;
    std::size_t work_bytes_;
    ScientificRawGrid(ScientificGrid,RawImageMetadata,ScientificRawOptions,std::size_t);
    friend struct ScientificRawKernel;
    friend ScientificRawGrid read_scientific_raw(const ImageServer&,const ScientificGrid&,ScientificRawOptions,const std::atomic<bool>*);
public:
    const ScientificGrid& grid() const {return grid_;}
    const RawImageMetadata& metadata() const {return metadata_;}
    const ScientificRawOptions& options() const {return options_;}
    std::int64_t width() const {return grid_.width();}
    std::int64_t height() const {return grid_.height();}
    std::uint32_t channels() const {return options_.channels;}
    const std::vector<double>& values() const {return values_;}
    const std::vector<std::uint8_t>& valid() const {return valid_;}
    const std::vector<std::uint8_t>& reasons() const {return reasons_;}
    const std::string& reader() const {return reader_;}
    const std::string& decoded_source_sha256() const {return decoded_source_sha256_;}
    const std::string& input_sha256() const {return input_sha256_;}
    std::size_t estimated_work_bytes() const {return work_bytes_;}
};
// A distinct owning type prevents a window from being mistaken for a full grid
// by legacy feature/classifier/archive consumers.
class ScientificRawWindowData {
    ScientificRawWindow grid_;
    RawImageMetadata metadata_;
    ScientificRawOptions options_;
    std::vector<double> values_;
    std::vector<std::uint8_t> valid_,reasons_;
    std::string reader_,decoded_source_sha256_,input_sha256_;
    std::size_t work_bytes_;
    ScientificRawWindowData(ScientificRawWindow,RawImageMetadata,ScientificRawOptions,std::size_t);
    friend struct ScientificRawKernel;
public:
    const ScientificRawWindow& window() const {return grid_;}
    const RawImageMetadata& metadata() const {return metadata_;}
    const ScientificRawOptions& options() const {return options_;}
    std::int64_t width() const {return grid_.width();}std::int64_t height() const {return grid_.height();}
    std::uint32_t channels() const {return options_.channels;}
    const std::vector<double>& values() const {return values_;}
    const std::vector<std::uint8_t>& valid() const {return valid_;}
    const std::vector<std::uint8_t>& reasons() const {return reasons_;}
    const std::string& reader() const {return reader_;}
    const std::string& decoded_source_sha256() const {return decoded_source_sha256_;}
    const std::string& input_sha256() const {return input_sha256_;}
    std::size_t estimated_work_bytes() const {return work_bytes_;}
};
// Includes the clipped raw source window, HWC means/validity/reasons, metadata
// copies and fixed numeric/hash scratch. Reader-owned decode/cache is separate.
// Validates level-zero geometry, dtype, channel/Z/T selection and traversal
// limits before any pixel IO. Does not call a display reader.
std::size_t scientific_raw_work_bytes(const RawImageMetadata&,const ScientificGrid&,ScientificRawOptions={});
// Hash-only capture owns its metadata snapshot and allocates no sampled outputs.
struct ScientificRawDigest {
    RawImageMetadata metadata;
    std::string reader,decoded_source_sha256,input_sha256;
    std::size_t estimated_work_bytes=0;
};
ScientificRawDigest digest_scientific_raw(const ImageServer&,const ScientificGrid&,ScientificRawOptions={},
    const std::atomic<bool>* cancelled=nullptr);
ScientificRawGrid read_scientific_raw(const ImageServer&,const ScientificGrid&,ScientificRawOptions={},
    const std::atomic<bool>* cancelled=nullptr);
std::size_t scientific_raw_window_work_bytes(const RawImageMetadata&,const ScientificRawWindow&,ScientificRawOptions={});
ScientificRawWindowData read_scientific_raw_window(const ImageServer&,const ScientificRawWindow&,ScientificRawOptions={},const std::atomic<bool>* cancelled=nullptr);
ScientificRawDigest digest_scientific_raw_window(const ImageServer&,const ScientificRawWindow&,ScientificRawOptions={},const std::atomic<bool>* cancelled=nullptr);
}

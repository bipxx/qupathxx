#pragma once
#include "qupathxx/fluorescence_cells.hpp"
namespace qpx {struct FluorescenceTileResult;}
namespace qpx::detail {
struct FluorescenceScores {
    std::vector<double> values;
    std::vector<std::uint8_t> eligible;
    std::uint64_t finite=0,nonfinite=0,invalid=0;
};
FluorescenceScores fluorescence_scores(const RawPixelBuffer&,const std::vector<std::uint8_t>&,
    FluorescenceCellOptions,std::optional<std::array<double,2>>,std::size_t fixed_bytes,const std::atomic<bool>*);
std::string fluorescence_input_sha256(const RawPixelBuffer&,const std::vector<std::uint8_t>&,const std::atomic<bool>*);
double fluorescence_otsu_threshold(double lower,double upper,std::uint64_t count,const std::array<std::uint64_t,256>& bins);
FluorescenceTileResult segment_fluorescence_tile_reserved(const RawPixelBuffer&,const std::vector<std::uint8_t>&,
    const RawRegionRequest&,RegionRequest,RegionRequest,const std::string&,FluorescenceCellOptions,
    std::optional<std::array<double,2>>,std::size_t fixed_bytes,const std::atomic<bool>*);
std::size_t fluorescence_cell_work_bytes(std::int64_t width,std::int64_t height,std::uint32_t channels,SampleType,
    const FluorescenceCellOptions&,std::optional<std::array<double,2>> calibration,
    std::size_t raw_capacity_bytes=0,std::size_t domain_capacity_bytes=0,std::size_t fixed_bytes=0);
FluorescenceCellResult segment_fluorescence_cells_reserved(const RawPixelBuffer&,const std::vector<std::uint8_t>&,
    FluorescenceCellOptions,std::optional<std::array<double,2>>,std::size_t fixed_bytes,const std::atomic<bool>*,
    std::vector<std::uint8_t>* cut_foreground=nullptr);
}

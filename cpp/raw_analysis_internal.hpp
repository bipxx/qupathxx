#pragma once
#include "qupathxx/raw_analysis.hpp"
#include "scientific_raw_sampling_internal.hpp"
namespace qpx {
detail::ScientificRawReserve raw_grid_analysis_reserve(const ScientificGrid&,ScientificRawOptions,bool,const std::optional<Roi>&,const RawObjectContext&,const ReaderSource*,std::uint32_t series);
ScientificRawOptions raw_grid_analysis_sampling_options(const RawImageMetadata&,const ScientificGrid&,ScientificRawOptions,bool collect);
std::string raw_grid_analysis_hash(const std::string&,std::optional<double>,std::optional<double>,const std::optional<Roi>&);
bool same_raw_statistics(const RawRegionStatistics&,const RawRegionStatistics&,bool bitwise);
bool same_raw_grid(const std::optional<RawAnalysisGrid>&,const std::optional<ScientificGrid>&,const ScientificRawOptions&);
struct RawAnalysisCodec {
    static std::vector<std::uint8_t> encode(const RawAnalysisRecord&,const ImageMetadata&,const std::atomic<bool>* cancelled=nullptr);
    static std::shared_ptr<const RawAnalysisRecord> decode(const std::uint8_t*,std::size_t,const ImageMetadata&,const std::atomic<bool>* cancelled=nullptr);
};
}

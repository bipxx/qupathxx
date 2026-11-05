#pragma once
#include "qupathxx/analysis_types.hpp"
#include "intensity_statistics_codec.hpp"
#include "sha256.hpp"
#include <cstring>
#include <limits>
#include <vector>
namespace qpx::detail {
inline void validate_region_statistics(const AnalysisResult& r) {
    const auto policy=r.sampling_policy();const bool measured=r.algorithm==roi_statistics_algorithm(policy);
    if(measured!=bool(r.intensity_statistics)) throw std::invalid_argument("region statistics payload/algorithm mismatch");
    if(!measured) return;
    const auto& s=*r.intensity_statistics;
    if(!r.samples || s.samples!=r.samples) throw std::invalid_argument("region statistics sample count differs");
    ColorVector rgb_mean;
    for(std::size_t c=0;c<3;++c) {
        if(!std::isfinite(r.means[c]) || !std::isfinite(r.means[c]*double(r.samples))) throw std::invalid_argument("region statistics mean is not representable");
        rgb_mean[c]=s.rgb.sum[c]/double(r.samples);
    }
    validate_intensity_compartment(s,r.samples,rgb_mean,r.means,"region intensity statistics");
}
struct RegionStatisticsWriter {
    std::vector<std::uint8_t> bytes={'Q','R','I',1};
    void integer(std::uint64_t v) {for(int i=0;i<8;++i) bytes.push_back(std::uint8_t(v>>(8*i)));}
    void real(double v) {std::uint64_t bits;std::memcpy(&bits,&v,8);integer(bits);}
};
struct RegionStatisticsReader {
    const std::uint8_t* p;std::size_t left;
    std::uint64_t integer() {if(left<8) throw std::invalid_argument("truncated region statistics");std::uint64_t v=0;for(int i=0;i<8;++i) v|=std::uint64_t(p[i])<<(8*i);p+=8;left-=8;return v;}
    double real() {const auto bits=integer();double value;std::memcpy(&value,&bits,8);if(!std::isfinite(value)) throw std::invalid_argument("nonfinite region statistic");return value;}
};
inline std::string region_statistics_checksum(const AnalysisResult& r,const std::uint8_t* data,std::size_t size) {
    Sha256 hash;
    for(const auto* s:{&r.algorithm,&r.input_sha256}) {hash.integer(s->size());hash.update(s->data(),s->size());}
    for(auto v:{r.object_id,r.object_revision,r.document_revision,r.samples,std::uint64_t(r.kind),std::uint64_t(r.nucleus)}) hash.integer(v);
    auto real=[&](double value) {if(value==0) value=0.;std::uint64_t bits;std::memcpy(&bits,&value,8);hash.integer(bits);};
    for(auto v:r.means) real(v);for(auto v:r.white) real(v);for(const auto& row:r.stains) for(auto v:row) real(v);
    hash.update(data,size);return hash.finish();
}
inline std::vector<std::uint8_t> encode_region_statistics(const AnalysisResult& r) {
    validate_region_statistics(r);
    if(!r.intensity_statistics) throw std::invalid_argument("region result has no intensity statistics");
    RegionStatisticsWriter w;w.integer(r.samples);write_intensity_compartment(w,*r.intensity_statistics);
    const auto digest=region_statistics_checksum(r,w.bytes.data(),w.bytes.size());w.bytes.insert(w.bytes.end(),digest.begin(),digest.end());return std::move(w.bytes);
}
inline std::shared_ptr<const CompartmentIntensityStatistics> decode_region_statistics(const AnalysisResult& r,const std::uint8_t* data,std::size_t size) {
    if(!data || size<64 || size!=std::size_t(r.samples>1?268:220) || data[0]!='Q' || data[1]!='R' || data[2]!='I' || data[3]!=1)
        throw std::invalid_argument("invalid region statistics encoding");
    if(std::string(reinterpret_cast<const char*>(data+size-64),64)!=region_statistics_checksum(r,data,size-64)) throw std::invalid_argument("region statistics checksum mismatch");
    RegionStatisticsReader reader{data+4,size-68};const auto samples=reader.integer();
    if(!samples || samples!=r.samples) throw std::invalid_argument("region statistics captured count differs");
    auto stats=std::make_shared<const CompartmentIntensityStatistics>(read_intensity_compartment(reader,samples));
    if(reader.left) throw std::invalid_argument("trailing region statistics bytes");
    auto value=r;value.intensity_statistics=stats;validate_region_statistics(value);return stats;
}
}

#pragma once
#include "qupathxx/scientific_raw_sampling.hpp"
namespace qpx::detail {
// Caller-owned captured payload and additional metadata copies coexist with
// sampler allocations. Admission uses the sampler's single metadata epoch.
struct ScientificRawReserve {
    std::size_t fixed_bytes=0,metadata_copies=0;
    std::optional<std::uint32_t> expected_series;
    // Synchronous borrowed output: observe provenance after metadata opens the
    // reader, before decoder IO; no pointer is stored in returned results.
    std::shared_ptr<const ReaderSource>* observed_source=nullptr;
    std::size_t source_copies=0;
};
std::size_t scientific_raw_reserved_work_bytes(const RawImageMetadata&,const ScientificGrid&,ScientificRawOptions,bool,const ScientificRawReserve&);
ScientificRawGrid read_scientific_raw_reserved(const ImageServer&,const ScientificGrid&,ScientificRawOptions,const ScientificRawReserve&,const std::atomic<bool>*);
ScientificRawWindowData read_scientific_raw_window_reserved(const ImageServer&,const ScientificRawWindow&,ScientificRawOptions,const ScientificRawReserve&,const std::atomic<bool>*);
ScientificRawDigest digest_scientific_raw_reserved(const ImageServer&,const ScientificGrid&,ScientificRawOptions,const ScientificRawReserve&,const std::atomic<bool>*);
std::string scientific_raw_input_sha256(const RawImageMetadata&,const ScientificGrid&,const std::string& reader,const std::string& decoded,const std::atomic<bool>* cancelled=nullptr);
std::string scientific_raw_input_sha256(const RawImageMetadata&,const ScientificRawWindow&,const std::string& reader,const std::string& decoded,const std::atomic<bool>* cancelled=nullptr);
std::string scientific_raw_input_sha256(const RawImageMetadata&,const ScientificRawLattice&,const std::string& reader,const std::string& decoded,const std::atomic<bool>* cancelled=nullptr);
}

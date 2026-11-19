// SPDX-License-Identifier: AGPL-3.0-only
#include "reader_plugin_profile.hpp"
#include "../image-common/uint16_store_reader.hpp"
#include "ome_zarr_internal.hpp"

namespace {
struct Ngff05Source {
    static constexpr auto provider_id = "org.qupathxx.ngff05";
    static constexpr auto name = "OME-Zarr NGFF 0.5";
    static constexpr auto version = QPX_NGFF05_VERSION;
    static constexpr auto contract = QPX_NGFF05_CONTRACT;
    static constexpr auto primary_key = "zarr.json";

    static std::shared_ptr<qpx::ImageServer> open(const std::string& path, std::size_t work,
                                                const std::atomic<bool>* cancelled) {
        qpx::OmeZarrOpenOptions options;
        options.work_bytes = work;
        return qpx::detail::open_ngff05_image(path, options, cancelled);
    }
};

using Reader = qpx_image_plugin::TypedStoreReader<Ngff05Source, qpx::SampleType::UInt8,
                                                qpx::SampleType::Int8, qpx::SampleType::Int16,
                                                qpx::SampleType::UInt32,
                                                qpx::SampleType::Int32, qpx::SampleType::UInt64,
                                                qpx::SampleType::Int64, qpx::SampleType::Float32,
                                                qpx::SampleType::Float64>;
} // namespace

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL
qpx_reader_entry_v1(std::uint32_t requested_abi, qpx_reader_api_v1* out,
                    qpx_reader_text_v1* error) {
    return Reader::entry(requested_abi, out, error);
}

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL
qpx_reader_store_entry_v1(std::uint32_t requested_abi, qpx_reader_store_api_v1* out,
                          qpx_reader_text_v1* error) {
    return Reader::store_entry(requested_abi, out, error);
}

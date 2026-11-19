// SPDX-License-Identifier: AGPL-3.0-only
#include "reader_plugin_profile.hpp"
#include "../image-common/uint16_store_reader.hpp"
#include "n5_source.hpp"

namespace {
struct N5Source {
    static constexpr auto provider_id = "org.qupathxx.n5";
    static constexpr auto name = "N5 TensorStore UInt16";
    static constexpr auto version = "1.0.0";
    static constexpr auto contract = "n5-tensorstore-uint16/1";
    static constexpr auto primary_key = "attributes.json";

    static std::shared_ptr<qpx::ImageServer> open(const std::string& path, std::size_t work,
                                                const std::atomic<bool>* cancelled) {
        return qpx::detail::open_n5_image(path, work, cancelled);
    }
};

using Reader = qpx_image_plugin::UInt16StoreReader<N5Source>;
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

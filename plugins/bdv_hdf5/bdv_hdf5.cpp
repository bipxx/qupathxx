// SPDX-License-Identifier: AGPL-3.0-only
#include "reader_plugin_profile.hpp"
#include "../image-common/uint16_store_reader.hpp"
#include "bdv_hdf5_source.hpp"

namespace {
struct BdvHdf5Source {
    static constexpr auto provider_id = "org.qupathxx.bdv-hdf5";
    static constexpr auto name = "BigDataViewer HDF5 UInt16";
    static constexpr auto version = "1.4.0";
    static constexpr auto contract = "bdv-hdf5-uint16/5";
    static constexpr auto primary_key = "dataset.xml";

    static std::shared_ptr<qpx::ImageServer> open(const std::string& path, std::size_t work,
                                                const std::atomic<bool>* cancelled) {
        return qpx::detail::open_bdv_hdf5_image(path, work, cancelled);
    }
};

using Reader = qpx_image_plugin::UInt16StoreReader<BdvHdf5Source>;
}

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL
qpx_reader_entry_v1(std::uint32_t abi, qpx_reader_api_v1* out, qpx_reader_text_v1* error) {
    return Reader::entry(abi, out, error);
}

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL
qpx_reader_store_entry_v1(std::uint32_t abi, qpx_reader_store_api_v1* out, qpx_reader_text_v1* error) {
    return Reader::store_entry(abi, out, error);
}

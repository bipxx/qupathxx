// SPDX-License-Identifier: AGPL-3.0-only
#include "reader_plugin_profile.hpp"
#include <qupathxx/reader_plugin.h>
#include <blosc.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>
namespace {
constexpr std::uint64_t limit = UINT64_C(256) * 1024 * 1024;
struct Failure {
    qpx_reader_status_v1 status;
    const char* message;
};
struct Handle {
    std::vector<std::uint8_t> bytes;
};
bool text(qpx_reader_text_v1& o, const char* s) {
    o.size = std::strlen(s);
    if (o.capacity < o.size || (o.size && !o.data))
        return false;
    if (o.size)
        std::memcpy(o.data, s, o.size);
    return true;
}
qpx_reader_status_v1 fail(qpx_reader_text_v1* e, qpx_reader_status_v1 s, const char* m) {
    return e && !text(*e, m) ? QPX_READER_BUFFER_TOO_SMALL : s;
}
template <class F> qpx_reader_status_v1 boundary(qpx_reader_text_v1* e, F f) noexcept {
    if (e)
        e->size = 0;
    try {
        return f();
    } catch (const Failure& x) {
        return fail(e, x.status, x.message);
    } catch (const std::exception&) {
        return fail(e, QPX_READER_IO_ERROR, "Blosc byte-array operation failed");
    } catch (...) {
        return fail(e, QPX_READER_INTERNAL_ERROR, "Blosc byte-array internal error");
    }
}
void require(bool b, const char* m) {
    if (!b)
        throw Failure{QPX_READER_INVALID_REQUEST, m};
}
void checkpoint(qpx_reader_cancel_v1 c) {
    if (c.is_cancelled && c.is_cancelled(c.context))
        throw Failure{QPX_READER_CANCELLED, "Blosc byte-array operation cancelled"};
}
const Handle& handle(void* p) {
    require(p != nullptr, "Blosc handle is null");
    return *static_cast<Handle*>(p);
}
qpx_reader_status_v1 QPX_READER_CALL describe(qpx_reader_info_v1* o,
                                              qpx_reader_text_v1* e) noexcept {
    return boundary(e, [&] {
        require(o && o->struct_size >= sizeof(*o), "Blosc info structure is too small");
        o->abi_version = QPX_READER_ABI_V1;
        o->capabilities = QPX_READER_CAP_RGBA | QPX_READER_CAP_RAW;
        bool ok = text(o->provider_id, "org.qupathxx.blosc-bytes");
        ok = text(o->display_name, "Blosc decoded byte array") && ok;
        ok = text(o->version, "1.0.0") && ok;
        ok = text(o->contract, "blosc1-lossless-flat-bytes/1") && ok;
        return ok ? QPX_READER_OK
                  : fail(e, QPX_READER_BUFFER_TOO_SMALL, "Blosc descriptor buffer is too small");
    });
}
qpx_reader_status_v1 QPX_READER_CALL open(const qpx_reader_open_v1* r, void** o,
                                          qpx_reader_text_v1* e) noexcept {
    return boundary(e, [&] {
        require(o != nullptr, "Blosc output handle is null");
        *o = nullptr;
        require(r && r->struct_size >= sizeof(*r) && !r->reserved && !r->options.size,
                "Invalid Blosc open structure/options");
        require(r->work_bytes && r->work_bytes <= limit, "Blosc work budget must be1..256MiB");
        require(r->path.data && r->path.size && r->path.size <= 32768, "Invalid Blosc path");
        const std::string path(r->path.data, std::size_t(r->path.size));
        require(path.find('\0') == std::string::npos, "Blosc path contains NUL");
        const auto p = std::filesystem::u8path(path);
        require(p.is_absolute(), "Blosc path must be absolute");
        checkpoint(r->cancel);
        const auto size = std::filesystem::file_size(p);
        require(size >= BLOSC_MIN_HEADER_LENGTH && size <= 64u * 1024 * 1024 &&
                    size < r->work_bytes,
                "Blosc encoded input exceeds its bounded profile");
        std::ifstream in(p, std::ios::binary);
        require(bool(in), "Blosc input cannot be opened");
        std::vector<std::uint8_t> encoded(std::size_t(size), 0);
        in.read(reinterpret_cast<char*>(encoded.data()), std::streamsize(size));
        require(in.gcount() == std::streamsize(size) && in.peek() == EOF,
                "Blosc input changed/truncated while opening");
        std::size_t n = 0, block = 0, cbytes = 0, header_n = 0, typesize = 0;
        int flags = 0;
        require(blosc_cbuffer_validate(encoded.data(), encoded.size(), &n) == 0 && n &&
                    n <= 64u * 1024 * 1024,
                "Invalid/oversized Blosc frame");
        blosc_cbuffer_sizes(encoded.data(), &header_n, &cbytes, &block);
        blosc_cbuffer_metainfo(encoded.data(), &typesize, &flags);
        require(cbytes == size && header_n == n && block && block <= n && typesize &&
                    typesize <= BLOSC_MAX_TYPESIZE,
                "Inconsistent Blosc frame/header");
        require(size + n + 3 * block + 4 * typesize + 256u * 1024 + 8 * path.size() <=
                    r->work_bytes,
                "Blosc decoded data and scratch exceed work budget");
        const char* codec = blosc_cbuffer_complib(encoded.data());
        require(codec && (std::strcmp(codec, "LZ4") == 0 || std::strcmp(codec, "BloscLZ") == 0),
                "Blosc codec is outside the admitted LZ4/BloscLZ profile");
        auto h = std::make_unique<Handle>();
        h->bytes.resize(n);
        checkpoint(r->cancel);
        const auto written =
            blosc_decompress_ctx(encoded.data(), h->bytes.data(), h->bytes.size(), 1);
        checkpoint(r->cancel);
        require(written > 0 && std::size_t(written) == n, "Corrupt Blosc compressed payload");
        *o = h.release();
        return QPX_READER_OK;
    });
}
void QPX_READER_CALL destroy(void* p) noexcept {
    delete static_cast<Handle*>(p);
}
qpx_reader_status_v1 QPX_READER_CALL metadata(void* p, qpx_reader_metadata_v1* o,
                                              qpx_reader_cancel_v1 c,
                                              qpx_reader_text_v1* e) noexcept {
    return boundary(e, [&] {
        const auto& h = handle(p);
        require(o && o->struct_size >= sizeof(*o), "Blosc metadata structure is too small");
        checkpoint(c);
        o->level_count = 1;
        o->width = h.bytes.size();
        o->height = 1;
        o->has_raw = 1;
        o->channels = 1;
        o->sample_type = QPX_READER_UINT8;
        o->z_planes = o->timepoints = o->series_count = 1;
        o->series = 0;
        o->calibration_flags = 0;
        o->pixel_width_um = o->pixel_height_um = o->z_spacing_um = o->time_interval_seconds = 0;
        bool ok = text(o->vendor, "Blosc decoded bytes; outer container owns axes and dtype");
        ok = text(o->dimension_order, "XYCZT") && ok;
        return ok ? QPX_READER_OK
                  : fail(e, QPX_READER_BUFFER_TOO_SMALL, "Blosc metadata buffer is too small");
    });
}
qpx_reader_status_v1 QPX_READER_CALL level(void* p, std::uint32_t i, qpx_reader_level_v1* o,
                                           qpx_reader_text_v1* e) noexcept {
    return boundary(e, [&] {
        require(o && o->struct_size >= sizeof(*o) && !i, "Invalid Blosc level");
        o->reserved = 0;
        o->width = handle(p).bytes.size();
        o->height = 1;
        o->downsample = 1;
        return QPX_READER_OK;
    });
}
qpx_reader_status_v1 QPX_READER_CALL channel_name(void* p, std::uint32_t i, qpx_reader_text_v1* o,
                                                  qpx_reader_text_v1* e) noexcept {
    return boundary(e, [&] {
        handle(p);
        require(o && !i, "Invalid Blosc byte channel");
        return text(*o, "Decoded byte")
                   ? QPX_READER_OK
                   : fail(e, QPX_READER_BUFFER_TOO_SMALL, "Blosc channel buffer is too small");
    });
}
qpx_reader_status_v1 read(void* p, const qpx_reader_region_v1* r, qpx_reader_pixels_v1* o,
                          qpx_reader_cancel_v1 c, qpx_reader_text_v1* e, bool raw) noexcept {
    return boundary(e, [&] {
        const auto& h = handle(p);
        require(r && r->struct_size >= sizeof(*r) && o && o->struct_size >= sizeof(*o),
                "Invalid Blosc read structure");
        o->written = o->validity_written = 0;
        require(!r->level && !r->z && !r->t && !r->first_channel && r->channels == (raw ? 1u : 0u),
                "Invalid Blosc byte coordinates");
        const auto stride = raw ? 1u : 4u;
        require(r->width > 0 && r->height > 0 && std::uint64_t(r->width) <= limit / stride &&
                    std::uint64_t(r->height) <= limit / (stride * std::uint64_t(r->width)),
                "Blosc output exceeds byte limit");
        require(r->x <= INT64_MAX - r->width && r->y <= INT64_MAX - r->height,
                "Blosc coordinate overflow");
        const auto count = std::uint64_t(r->width) * r->height;
        require(o->sample_type == QPX_READER_UINT8 && o->width == r->width &&
                    o->height == r->height && o->channels == stride && !o->reserved &&
                    o->row_stride == std::uint64_t(r->width) * stride &&
                    o->pixel_stride == stride && o->channel_stride == 1 && o->data &&
                    o->capacity >= count * stride,
                "Invalid Blosc output layout/buffer");
        require(raw ? (o->validity && o->validity_capacity >= count && count <= limit - count)
                    : (!o->validity && !o->validity_capacity),
                "Invalid Blosc validity buffer/limit");
        checkpoint(c);
        std::fill_n(o->data, std::size_t(count * stride), std::uint8_t(0));
        if (raw)
            std::fill_n(o->validity, std::size_t(count), std::uint8_t(0));
        for (std::int64_t y = 0; y < r->height; ++y) {
            checkpoint(c);
            if (r->y + y != 0)
                continue;
            for (std::int64_t x = 0; x < r->width; ++x) {
                if ((x & 65535) == 0)
                    checkpoint(c);
                const auto from = r->x + x;
                if (from < 0 || std::uint64_t(from) >= h.bytes.size())
                    continue;
                const auto to = std::uint64_t(y) * r->width + x;
                const auto b = h.bytes[std::size_t(from)];
                o->data[to * stride] = b;
                if (raw)
                    o->validity[to] = 1;
                else {
                    o->data[to * 4 + 1] = o->data[to * 4 + 2] = b;
                    o->data[to * 4 + 3] = 255;
                }
            }
        }
        checkpoint(c);
        o->written = count * stride;
        o->validity_written = raw ? count : 0;
        return QPX_READER_OK;
    });
}
qpx_reader_status_v1 QPX_READER_CALL read_rgba(void* p, const qpx_reader_region_v1* r,
                                               qpx_reader_pixels_v1* o,
                                               qpx_reader_text_v1* e) noexcept {
    return read(p, r, o, {}, e, false);
}
qpx_reader_status_v1 QPX_READER_CALL read_raw(void* p, const qpx_reader_region_v1* r,
                                              qpx_reader_pixels_v1* o, qpx_reader_cancel_v1 c,
                                              qpx_reader_text_v1* e) noexcept {
    return read(p, r, o, c, e, true);
}
} // namespace
extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL
qpx_reader_entry_v1(std::uint32_t abi, qpx_reader_api_v1* o, qpx_reader_text_v1* e) {
    return boundary(e, [&] {
        require(abi == QPX_READER_ABI_V1 && o && o->struct_size >= sizeof(*o),
                "Unsupported Blosc reader ABI/structure");
        *o = {sizeof(*o), QPX_READER_ABI_V1, describe,  open,    destroy, metadata,
              level,      channel_name,      read_rgba, read_raw};
        return QPX_READER_OK;
    });
}

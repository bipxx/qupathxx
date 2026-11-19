// SPDX-License-Identifier: AGPL-3.0-only
#include "../image-common/rgba8_reader.hpp"

namespace {
using namespace qpx_image_plugin;

class Cursor {
    const std::uint8_t* data_;
    std::size_t size_;
public:
    Cursor(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}
    const std::uint8_t* take(std::size_t count) {
        valid(count <= size_, "GIF input is truncated");
        auto* result = data_;
        data_ += count;
        size_ -= count;
        return result;
    }
    unsigned byte() { return *take(1); }
    unsigned word() {
        const auto* bytes = take(2);
        return unsigned(bytes[0]) | (unsigned(bytes[1]) << 8);
    }
    bool empty() const { return !size_; }
    void skip_blocks(qpx_reader_cancel_v1 cancel) {
        for (auto length = byte(); length; length = byte()) {
            checkpoint(cancel);
            take(length);
        }
    }
};

// GIF packs variable-width codes least-significant bit first across sub-blocks.
class Codes {
    Cursor& stream_;
    unsigned remaining_ = 0, bits_ = 0, buffer_ = 0;
public:
    explicit Codes(Cursor& stream) : stream_(stream) {}
    unsigned read(unsigned width) {
        while (bits_ < width) {
            if (!remaining_) {
                remaining_ = stream_.byte();
                valid(remaining_, "GIF LZW stream ended before the end code");
            }
            buffer_ |= stream_.byte() << bits_;
            bits_ += 8;
            --remaining_;
        }
        const auto value = buffer_ & ((1u << width) - 1);
        buffer_ >>= width;
        bits_ -= width;
        return value;
    }
    void finish() {
        // Bits in the final byte are pad bits. Whole bytes after EOI are refused.
        valid(!remaining_ && stream_.byte() == 0, "Extra GIF LZW data after the end code");
    }
};

using Palette = std::array<std::array<std::uint8_t, 3>, 256>;
unsigned read_palette(Cursor& stream, unsigned flags, Palette& palette) {
    if (!(flags & 128)) return 0;
    const auto count = 2u << (flags & 7);
    const auto* triples = stream.take(3 * count);
    for (unsigned i = 0; i < count; ++i)
        std::memcpy(palette[i].data(), triples + 3 * i, 3);
    return count;
}

struct Raster {
    std::uint32_t width = 0, height = 0;
    const char* vendor = "GIF";
    Palette palette{};
    int transparent = -1;
    std::unique_ptr<std::uint8_t[]> raw;
    unsigned channels() const { return 1; }
    unsigned sample_bytes() const { return 1; }
    std::uint32_t sample_type() const { return QPX_READER_UINT8; }
    const char* channel_name(unsigned) const { return "Palette index"; }
    void rgba(const std::uint8_t* source, std::uint8_t* output) const {
        std::memcpy(output, palette[*source].data(), 3);
        output[3] = *source == transparent ? 0 : 255;
    }
};

class IndexOutput {
    static constexpr unsigned first_row_[4] = {0, 4, 2, 1};
    static constexpr unsigned stride_[4] = {8, 8, 4, 2};
    Raster& raster_;
    unsigned colors_, x_ = 0, y_ = 0, pass_ = 0;
    std::size_t written_ = 0;
    bool interlaced_;
    qpx_reader_cancel_v1 cancel_;
public:
    IndexOutput(Raster& raster, unsigned colors, bool interlaced, qpx_reader_cancel_v1 cancel)
        : raster_(raster), colors_(colors), interlaced_(interlaced), cancel_(cancel) {}
    void put(std::uint8_t index) {
        valid(index < colors_, "GIF palette index is outside its color table");
        valid(y_ < raster_.height, "GIF LZW produced too many indices");
        if (!(written_ & 4095)) checkpoint(cancel_);
        raster_.raw[std::size_t(y_) * raster_.width + x_] = index;
        ++written_;
        if (++x_ != raster_.width) return;
        x_ = 0;
        y_ += interlaced_ ? stride_[pass_] : 1;
        if (interlaced_)
            while (y_ >= raster_.height && pass_ < 3) y_ = first_row_[++pass_];
    }
    void finish() const {
        valid(written_ == std::size_t(raster_.width) * raster_.height, "GIF LZW produced too few indices");
    }
};

void decode_lzw(Cursor& stream, IndexOutput& output, qpx_reader_cancel_v1 cancel) {
    const auto minimum = stream.byte();
    valid(minimum >= 2 && minimum <= 8, "Invalid GIF LZW minimum code size");
    const auto clear = 1u << minimum, end = clear + 1;
    constexpr unsigned capacity = 4096;
    std::array<std::uint16_t, capacity> prefix{};
    std::array<std::uint8_t, capacity> tail{}, stack{};
    Codes codes(stream);
    auto width = minimum + 1, next = end + 1, previous = capacity;
    unsigned first = 0;
    valid(codes.read(width) == clear, "GIF LZW requires an initial clear code");
    for (std::size_t iteration = 0;; ++iteration) {
        if (!(iteration & 1023)) checkpoint(cancel);
        const auto code = codes.read(width);
        if (code == clear) {
            width = minimum + 1;
            next = end + 1;
            previous = capacity;
            continue;
        }
        if (code == end) break;
        valid(code <= next && (previous != capacity || code < clear), "Invalid GIF LZW code");
        unsigned length = 0, current = code;
        if (current == next) {
            stack[length++] = std::uint8_t(first);
            current = previous;
        }
        while (current >= clear) {
            valid(current > end && current < next && length < capacity, "Invalid GIF LZW dictionary chain");
            stack[length++] = tail[current];
            current = prefix[current];
        }
        first = current;
        output.put(std::uint8_t(first));
        while (length) output.put(stack[--length]);
        if (previous != capacity && next < capacity) {
            prefix[next] = std::uint16_t(previous);
            tail[next++] = std::uint8_t(first);
            if (next == (1u << width) && width < 12) ++width;
        }
        previous = code;
    }
    codes.finish();
    output.finish();
}

struct Codec {
    static constexpr const char* provider_id = "org.qupathxx.gif";
    static constexpr const char* name = "GIF";
    static constexpr const char* version = QPX_GIF_VERSION;
    static constexpr const char* contract = "gif-static-indexed/1";

    static Raster decode(std::uint8_t* bytes, std::size_t size, Budget& budget, qpx_reader_cancel_v1 cancel) {
        Raster raster;
        Cursor stream(bytes, size);
        const std::string_view signature(reinterpret_cast<const char*>(stream.take(6)), 6);
        const bool version89 = signature == "GIF89a";
        valid(version89 || signature == "GIF87a", "Invalid GIF signature");
        raster.width = stream.word();
        raster.height = stream.word();
        valid(raster.width && raster.height, "GIF logical screen dimensions must be positive");
        const auto flags = stream.byte(), background = stream.byte(), aspect = stream.byte();
        valid(!aspect || aspect == 49, "Unsupported GIF pixel aspect ratio");
        auto colors = read_palette(stream, flags, raster.palette);
        valid(!colors || background < colors, "GIF background is outside its color table");
        bool control = false, image = false;
        for (;;) {
            checkpoint(cancel);
            const auto block = stream.byte();
            if (block == 0x3b) {
                valid(image && stream.empty(), "GIF requires one image and a final trailer");
                return raster;
            }
            if (block == 0x21) {
                valid(version89, "GIF extensions require GIF89a");
                const auto label = stream.byte();
                if (label == 0xfe) {
                    stream.skip_blocks(cancel);
                    continue;
                }
                if (label != 0xf9)
                    fail(QPX_READER_UNSUPPORTED, "GIF application and plain text extensions are outside this profile");
                valid(!control && !image && stream.byte() == 4, "Invalid or repeated GIF graphic control");
                control = true;
                const auto graphic_flags = stream.byte(), delay = stream.word(), index = stream.byte();
                valid(!(graphic_flags & 0xe2) && ((graphic_flags >> 2) & 7) <= 1 && !delay,
                      "GIF profile requires an untimed image without disposal");
                valid(!stream.byte(), "GIF graphic control terminator is missing");
                if (graphic_flags & 1) raster.transparent = int(index);
                continue;
            }
            valid(block == 0x2c && !image, "GIF profile requires exactly one image");
            const auto left = stream.word(), top = stream.word();
            const auto width = stream.word(), height = stream.word();
            valid(!left && !top && width == raster.width && height == raster.height,
                  "GIF image must cover the entire logical screen");
            const auto image_flags = stream.byte();
            valid(!(image_flags & 0x18), "Invalid GIF image descriptor flags");
            if (image_flags & 128) colors = read_palette(stream, image_flags, raster.palette);
            valid(colors && raster.transparent < int(colors), "GIF requires a valid color table and transparency index");
            raster.raw = budget.array(std::uint64_t(width) * height);
            IndexOutput indices(raster, colors, bool(image_flags & 64), cancel);
            decode_lzw(stream, indices, cancel);
            image = true;
        }
    }
};
using Reader = PackedImageReader<Codec, Raster>;
}

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_entry_v1(
    std::uint32_t version, qpx_reader_api_v1* out, qpx_reader_text_v1* error) {
    return Reader::entry(version, out, error);
}

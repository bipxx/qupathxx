#pragma once
#include "qupathxx/label_raster.hpp"
#include "qupathxx/raw_image.hpp"
#include <variant>

namespace qpx {
// Exact mathematical integer value, independent of source sample width/sign.
// Zero is canonical (never negative); negative magnitudes fit Int64.
class LabelCode {
  public:
    LabelCode();
    LabelCode(bool negative, std::uint64_t magnitude);
    static LabelCode from_signed(std::int64_t);
    static LabelCode from_unsigned(std::uint64_t);
    // Canonical exact interchange: no plus, leading zeros, negative zero,
    // whitespace or values outside the signed-negative/unsigned-positive domain.
    static LabelCode from_decimal(const std::string&);
    bool negative() const;
    std::uint64_t magnitude() const;
    std::string decimal() const;
    // FNV-1a over sign + eight little-endian magnitude bytes. This is a stable
    // value hash, not a cryptographic identity or an alternative to exact equality.
    std::uint64_t stable_hash() const;
    friend bool operator==(const LabelCode&, const LabelCode&);
    friend bool operator!=(const LabelCode&, const LabelCode&);
    friend bool operator<(const LabelCode&, const LabelCode&);
    friend bool operator>(const LabelCode&, const LabelCode&);
    friend bool operator<=(const LabelCode&, const LabelCode&);
    friend bool operator>=(const LabelCode&, const LabelCode&);

  private:
    bool negative_;
    std::uint64_t magnitude_;
};

// Codes are retained as exact native integral values. Raw 8/16-bit inputs widen
// losslessly to 32 bits; 64-bit inputs retain their signed/unsigned 64-bit type.
using LabelCodeStorage =
    std::variant<std::vector<std::uint32_t>, std::vector<std::int32_t>,
                 std::vector<std::uint64_t>, std::vector<std::int64_t>>;
class LabelCodeMap {
  public:
    // Entries must be strictly increasing mathematical integers, excluding zero.
    explicit LabelCodeMap(LabelCodeStorage = std::vector<std::uint32_t>{},
                          const std::atomic<bool>* cancelled = nullptr);
    std::size_t size() const;
    LabelCode at(std::size_t) const;
    const LabelCodeStorage& storage() const;
    std::size_t payload_bytes() const;
    // Legacy UInt8/16/32 normalization produces UInt32 storage, moved without a
    // second mapping allocation. Other signed/64-bit storage is never narrowed.
    std::vector<std::uint32_t> release_uint32() &&;

  private:
    LabelCodeStorage codes_;
};
struct NormalizedLabelCodes {
    LabelRaster raster;
    // Dense raster label i+1 maps to source_codes.at(i); zero is background.
    LabelCodeMap source_codes;
};
// One complete valid single-channel native lattice, independent of any ROI or
// connectivity. All eight integral RawPixelBuffer types are admitted exactly;
// floats, missing samples and malformed storage are rejected. Dense IDs follow
// increasing mathematical source-code order, including negative foreground codes.
// The budget includes borrowed raw storage, sorting/compaction overlap, retained
// mapping, dense raster/regions and bounded host scratch. No raw values mutate.
NormalizedLabelCodes normalize_label_codes(const RawPixelBuffer&, LabelRasterLimits = {},
                                           const std::atomic<bool>* cancelled = nullptr);
} // namespace qpx

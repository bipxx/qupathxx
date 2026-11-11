#include "ppm_decoder.hpp"
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace qpx_ppm {
namespace {
std::size_t pixel_count(int w, int h) {
    if (w <= 0 || h <= 0 || static_cast<std::size_t>(w) > std::numeric_limits<std::size_t>::max() / 3 / static_cast<std::size_t>(h))
        throw std::invalid_argument("invalid image dimensions");
    return static_cast<std::size_t>(w) * h * 3;
}
std::string token(std::istream& in) {
    std::string result;
    while (in >> std::ws && in.peek() == '#') in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    if (!(in >> result)) throw std::runtime_error("incomplete PPM header");
    return result;
}
int integer_token(std::istream& in) {
    auto s = token(in); std::size_t used = 0;
    int value = std::stoi(s, &used);
    if (used != s.size()) throw std::invalid_argument("invalid PPM integer");
    return value;
}
}
DecodedImage read_ppm(const std::string& path) {
    std::ifstream in(std::filesystem::u8path(path), std::ios::binary);
    if (!in) throw std::runtime_error("cannot open image: " + path);
    if (token(in) != "P6") throw std::invalid_argument("only binary P6 PPM is supported");
    int w = integer_token(in), h = integer_token(in), max = integer_token(in);
    if (max != 255) throw std::invalid_argument("PPM maximum must be 255");
    char separator = 0; in.get(separator);
    if (separator == '\r' && in.peek() == '\n') in.get();
    else if (separator != ' ' && separator != '\n' && separator != '\t' && separator != '\r')
        throw std::invalid_argument("missing PPM raster separator");
    auto count = pixel_count(w, h);
    auto start = in.tellg(); in.seekg(0, std::ios::end); auto end = in.tellg();
    if (start < 0 || end < start || static_cast<std::uint64_t>(end - start) < count)
        throw std::runtime_error("truncated PPM raster");
    in.seekg(start);
    std::vector<std::uint8_t> data(count);
    if (!in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(count))) throw std::runtime_error("truncated PPM raster");
    return {w, h, std::move(data)};
}
}

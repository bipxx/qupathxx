#include "reader_provenance.hpp"
#include "qupathxx/core.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>

std::int64_t number(const char* text) {
    std::size_t end = 0; auto value = std::stoll(text, &end);
    if (text[end]) throw std::invalid_argument("invalid integer argument");
    return value;
}
int main(int argc, char** argv) {
    if (argc != 8) {
        std::cerr << "Usage: qupathxx_region IMAGE X Y LEVEL WIDTH HEIGHT OUTPUT.pam\n"
                     "Level-zero X/Y; output WIDTH/HEIGHT at LEVEL. PPM uses dense reader.\n";
        return 2;
    }
    try {
        const std::string path = argv[1];
        if (std::filesystem::weakly_canonical(std::filesystem::u8path(path)) ==
            std::filesystem::weakly_canonical(std::filesystem::u8path(argv[7])))
            throw std::invalid_argument("output must not overwrite the source image");
        auto server = path.size() >= 4 && path.substr(path.size()-4) == ".ppm"
            ? qpx::ppm_image_server(path) : qpx::open_slide(path);
        auto level = number(argv[4]);
        if (level < 0 || level > std::numeric_limits<std::int32_t>::max()) throw std::out_of_range("invalid level");
        auto region = server->read_region({number(argv[2]), number(argv[3]), static_cast<std::int32_t>(level), number(argv[5]), number(argv[6])});
        std::ofstream out(std::filesystem::u8path(argv[7]), std::ios::binary);
        out << "P7\nWIDTH " << region.width() << "\nHEIGHT " << region.height()
            << "\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n";
        out.write(reinterpret_cast<const char*>(region.bytes().data()), static_cast<std::streamsize>(region.bytes().size()));
        out.close(); if (!out) throw std::runtime_error("failed to write output region");
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}

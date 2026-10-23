#include "reader_provenance.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/thumbnail.hpp"
#include "qupathxx/raw_image.hpp"
#include "raw_cli.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <vector>
#include <limits>
#include <optional>
#include <stdexcept>

std::int64_t number(const char* text) {
    std::size_t end = 0; auto value = std::stoll(text, &end);
    if (text[end]) throw std::invalid_argument("invalid integer argument");
    return value;
}
static int launch(int argc, char** argv) {
    if(argc>1 && std::string(argv[1])=="--raw") return raw_region_cli(argc,argv);
    const bool thumbnail=argc>1 && std::string(argv[1])=="--thumbnail";
    if ((thumbnail && !(argc==6 || (argc==8 && std::string(argv[6])=="--source-pixels"))) || (!thumbnail && argc!=8)) {
        std::cerr << "Usage: qupathxx_region IMAGE X Y LEVEL WIDTH HEIGHT OUTPUT.pam\n"
                     "       qupathxx_region --thumbnail IMAGE MAX_WIDTH MAX_HEIGHT OUTPUT.pam [--source-pixels PIXELS]\n"
                     "Level-zero X/Y; output WIDTH/HEIGHT at LEVEL. PPM uses dense reader.\n";
        return 2;
    }
    try {
        const std::string path = argv[thumbnail?2:1];
        const auto output=argv[thumbnail?5:7];
        if (std::filesystem::weakly_canonical(std::filesystem::u8path(path)) ==
            std::filesystem::weakly_canonical(std::filesystem::u8path(output)))
            throw std::invalid_argument("output must not overwrite the source image");
        auto server = path.size() >= 4 && path.substr(path.size()-4) == ".ppm"
            ? qpx::ppm_image_server(path) : qpx::has_tiff() && qpx::is_ome_tiff_path(path)?qpx::open_tiff(path):qpx::open_slide(path);
        std::shared_ptr<const qpx::ThumbnailResult> preview;
        std::optional<qpx::PixelBuffer> explicit_region;
        if(thumbnail) {
            const auto w=number(argv[3]),h=number(argv[4]);
            if(w<1 || h<1 || w>4096 || h>4096) throw std::invalid_argument("thumbnail bounds must be 1..4096 pixels");
            qpx::ThumbnailOptions options;options.max_width=static_cast<std::int32_t>(w);options.max_height=static_cast<std::int32_t>(h);
            if(argc==8) {const auto limit=number(argv[7]);if(limit<1) throw std::invalid_argument("thumbnail source pixel limit must be positive");options.source_pixel_limit=static_cast<std::uint64_t>(limit);}
            options.validate();
            preview=qpx::read_thumbnail(*server,options);
        } else {
            auto level=number(argv[4]);
            if(level<0 || level>std::numeric_limits<std::int32_t>::max()) throw std::out_of_range("invalid level");
            explicit_region.emplace(server->read_region({number(argv[2]),number(argv[3]),static_cast<std::int32_t>(level),number(argv[5]),number(argv[6])}));
        }
        const auto& region=thumbnail?preview->pixels:*explicit_region;
        std::ofstream out(std::filesystem::u8path(output), std::ios::binary);
        out << "P7\nWIDTH " << region.width() << "\nHEIGHT " << region.height()
            << "\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n";
        out.write(reinterpret_cast<const char*>(region.bytes().data()), static_cast<std::streamsize>(region.bytes().size()));
        out.close(); if (!out) throw std::runtime_error("failed to write output region");
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}

#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
    try {std::vector<std::string> text;for(int i=0;i<argc;++i) text.push_back(std::filesystem::path(argv[i]).u8string());std::vector<char*> pointers;for(auto& item:text) pointers.push_back(item.data());pointers.push_back(nullptr);return launch(argc,pointers.data());}
    catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 2;}
}
#else
int main(int argc,char** argv) {return launch(argc,argv);}
#endif

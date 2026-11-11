#include "reader_registry.hpp"
#include "format_family_catalog.hpp"
#include "reader_plugin.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/thumbnail.hpp"
#include "qupathxx/resampling.hpp"
#include "qupathxx/raw_image.hpp"
#include "raw_cli.hpp"
#include "decimal_quantity.hpp"
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
    std::vector<char*> plugin_arguments;
    if(argc>1 && std::string(argv[1])=="--reader-plugins") {
        if(argc<4){std::cerr<<"Usage: qupathxx_region --reader-plugins ABS_MANIFEST [--readers | display region arguments]\n";return 2;}
        try {qpx::detail::configure_reader_plugins(argv[2]);}
        catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
        plugin_arguments.push_back(argv[0]);for(int i=3;i<argc;++i)plugin_arguments.push_back(argv[i]);
        plugin_arguments.push_back(nullptr);argc-=2;argv=plugin_arguments.data();
    }
    if(argc>1 && std::string(argv[1])=="--formats") {
        if(argc!=2) {std::cerr<<"Usage: qupathxx_region --formats\n";return 2;}
        std::cout<<qpx::detail::format_family_catalog_json();return 0;
    }
    if(argc>1 && std::string(argv[1])=="--readers") {
        if(argc!=2) {std::cerr<<"Usage: qupathxx_region --readers\n";return 2;}
        std::cout<<qpx::detail::reader_catalog_json(qpx::detail::reader_features());return 0;
    }
    std::string requested_reader="auto";
    std::vector<char*> display_arguments;
    const bool explicit_reader=argc>1 && std::string(argv[1])=="--reader";
    if(explicit_reader) {
        if(argc<4) {std::cerr<<"Usage: qupathxx_region --reader ID [display region arguments]\n";return 2;}
        requested_reader=argv[2];display_arguments.push_back(argv[0]);
        for(int i=3;i<argc;++i)display_arguments.push_back(argv[i]);
        display_arguments.push_back(nullptr);argc-=2;argv=display_arguments.data();
    }
    if(argc>1 && std::string(argv[1])=="--raw") {
        if(explicit_reader) {std::cerr<<"--reader applies only to display region modes; --raw uses TIFF explicitly\n";return 2;}
        return raw_region_cli(argc,argv);
    }
    const bool resample=argc>1 && std::string(argv[1])=="--resample";
    const bool thumbnail=argc>1 && std::string(argv[1])=="--thumbnail";
    const bool nominal=argc>1 && std::string(argv[1])=="--rgb-nominal";
    if ((thumbnail && !(argc==6 || (argc==8 && std::string(argv[6])=="--source-pixels"))) || (resample && argc!=11) ||
        (nominal && !(argc==9 || (argc==11 && std::string(argv[9])=="--sampling"))) || (!thumbnail && !resample && !nominal && argc!=8)) {
        std::cerr << "Usage: qupathxx_region IMAGE X Y LEVEL WIDTH HEIGHT OUTPUT.pam\n"
                     "       qupathxx_region --thumbnail IMAGE MAX_WIDTH MAX_HEIGHT OUTPUT.pam [--source-pixels PIXELS]\n"
                     "       qupathxx_region --resample IMAGE X Y LEVEL SOURCE_WIDTH SOURCE_HEIGHT WIDTH HEIGHT OUTPUT.pam\n"
                     "       qupathxx_region --rgb-nominal IMAGE X Y SOURCE_WIDTH SOURCE_HEIGHT DOWNSAMPLE OUTPUT.pam [--sampling integer-origin|affine-inverse]\n"
                     "       qupathxx_region --readers\n"
                     "       qupathxx_region --formats\n"
                     "       qupathxx_region --reader ID [any display region mode above]\n"
                     "       qupathxx_region --reader-plugins ABS_MANIFEST [--readers | display region arguments]\n"
                     "Level-zero X/Y; output WIDTH/HEIGHT at LEVEL. PPM uses dense reader.\n";
        return 2;
    }
    try {
        const std::string path = argv[(thumbnail || resample || nominal)?2:1];
        const auto output=argv[thumbnail?5:resample?10:nominal?8:7];
        if (std::filesystem::weakly_canonical(std::filesystem::u8path(path)) ==
            std::filesystem::weakly_canonical(std::filesystem::u8path(output)))
            throw std::invalid_argument("output must not overwrite the source image");
        std::optional<qpx::RgbNominalRequest> nominal_request;
        if(nominal) {
            const auto step=qpx::detail::positive_decimal_quantity(argv[7]);
            if(!step) throw std::invalid_argument("RGB nominal downsample must be a positive finite decimal");
            auto sampling=qpx::RgbNominalSampling::IntegerOrigin;
            if(argc==11) {
                const std::string value=argv[10];
                if(value=="affine-inverse") sampling=qpx::RgbNominalSampling::AffineInverse;
                else if(value!="integer-origin") throw std::invalid_argument("nominal sampling must be integer-origin or affine-inverse");
            }
            nominal_request.emplace(number(argv[3]),number(argv[4]),number(argv[5]),number(argv[6]),*step,sampling);
        }
        const auto selection=qpx::detail::select_reader(path,requested_reader,
            qpx::detail::ReaderPolicy::RegionCli,qpx::detail::reader_features());
        auto server=qpx::detail::open_reader(path,selection,{},nullptr);
        std::shared_ptr<const qpx::ThumbnailResult> preview;
        std::optional<qpx::PixelBuffer> explicit_region;
        if(nominal) {
            explicit_region.emplace(qpx::read_rgb_nominal_region(*server,*nominal_request));
        } else if(thumbnail) {
            const auto w=number(argv[3]),h=number(argv[4]);
            if(w<1 || h<1 || w>4096 || h>4096) throw std::invalid_argument("thumbnail bounds must be 1..4096 pixels");
            qpx::ThumbnailOptions options;options.max_width=static_cast<std::int32_t>(w);options.max_height=static_cast<std::int32_t>(h);
            if(argc==8) {const auto limit=number(argv[7]);if(limit<1) throw std::invalid_argument("thumbnail source pixel limit must be positive");options.source_pixel_limit=static_cast<std::uint64_t>(limit);}
            options.validate();
            preview=qpx::read_thumbnail(*server,options);
        } else {
            const auto offset=resample?1:0;auto level=number(argv[4+offset]);
            if(level<0 || level>std::numeric_limits<std::int32_t>::max()) throw std::out_of_range("invalid level");
            qpx::RegionRequest request{number(argv[2+offset]),number(argv[3+offset]),static_cast<std::int32_t>(level),number(argv[5+offset]),number(argv[6+offset])};
            if(resample) explicit_region.emplace(qpx::read_resampled_region(*server,request,number(argv[8]),number(argv[9])));
            else explicit_region.emplace(server->read_region(request));
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

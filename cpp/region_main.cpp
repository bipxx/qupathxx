#include "reader_registry.hpp"
#include "format_family_catalog.hpp"
#include "qupathxx/reader_plugins.hpp"
#include "reader_plugin_link.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/thumbnail.hpp"
#include "qupathxx/resampling.hpp"
#include "qupathxx/raw_image.hpp"
#include "qupathxx/region_export.hpp"
#include "raw_cli.hpp"
#include "decimal_quantity.hpp"
#include <charconv>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <vector>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>

std::int64_t number(const char* text) {
    std::size_t end = 0; auto value = std::stoll(text, &end);
    if (text[end]) throw std::invalid_argument("invalid integer argument");
    return value;
}
static int launch(int argc, char** argv) {
    if(argc>1 && std::string_view(argv[1])=="--verify-raw-bundle")
        return raw_region_bundle_cli(argc,argv,false);
    if(argc>1 && std::string_view(argv[1])=="--extract-raw-bundle")
        return raw_region_bundle_cli(argc,argv,true);
    std::vector<char*> plugin_arguments;
    if(argc>1 && std::string(argv[1])=="--reader-plugins") {
        std::vector<std::string> manifests;
        int first=1;
        while(first+2<argc && std::string_view(argv[first])=="--reader-plugins") {
            manifests.emplace_back(argv[first+1]);
            first+=2;
        }
        if(std::string_view(argv[first])=="--reader-plugins"){std::cerr<<"Usage: qupathxx_region [--reader-plugins ABS_MANIFEST]... [--readers | display region arguments]\n";return 2;}
        try {qpx::configure_reader_plugin_manifests(manifests);}
        catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
        plugin_arguments.push_back(argv[0]);
        plugin_arguments.insert(plugin_arguments.end(),argv+first,argv+argc);
        plugin_arguments.push_back(nullptr);argc-=first-1;argv=plugin_arguments.data();
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
    std::optional<std::string> plugin_options;
    std::vector<char*> display_arguments;
    const bool explicit_reader=argc>1 && std::string(argv[1])=="--reader";
    if(explicit_reader) {
        if(argc<4) {std::cerr<<"Usage: qupathxx_region --reader ID [--reader-options JSON_OBJECT] MODE ...\n";return 2;}
        requested_reader=argv[2];
        int first_mode_argument=3;
        if(std::string_view(argv[3])=="--reader-options") {
            if(argc<6 || !argv[4][0] || !qpx::detail::valid_reader_plugin_id(requested_reader)) {
                std::cerr<<"--reader-options requires an explicit native reader plugin, a JSON object and a mode\n";
                return 2;
            }
            plugin_options=argv[4];
            first_mode_argument=5;
        }
        display_arguments.push_back(argv[0]);
        for(int i=first_mode_argument;i<argc;++i)display_arguments.push_back(argv[i]);
        display_arguments.push_back(nullptr);
        argc=static_cast<int>(display_arguments.size())-1;
        argv=display_arguments.data();
    }
    if(argc>1 && std::string_view(argv[1])=="--reader-options") {
        std::cerr<<"Specify --reader-options immediately after an explicit --reader ID\n";
        return 2;
    }
    if(argc>1 && std::string_view(argv[1])=="--raw-project") {
        if(explicit_reader) {
            std::cerr<<"--raw-project uses the project's saved reader and options\n";
            return 2;
        }
        return raw_region_cli(argc,argv);
    }
    if(argc>1 && std::string_view(argv[1])=="--project-region") {
        const bool receipt=argc==11 && std::string_view(argv[10])=="--receipt";
        if((argc!=10 && !receipt) || explicit_reader) {
            std::cerr<<"Usage: qupathxx_region --project-region INPUT_PROJECT IMAGE_ID X Y LEVEL WIDTH HEIGHT OUTPUT.pam [--receipt]\n";
            return 2;
        }
        try {
            const std::string_view id=argv[3];
            std::uint64_t image=0;
            const auto parsed=std::from_chars(id.data(),id.data()+id.size(),image);
            if(parsed.ec!=std::errc{} || parsed.ptr!=id.data()+id.size() || !image)
                throw std::invalid_argument("IMAGE_ID must be a positive native image ID");
            const auto level=number(argv[6]);
            if(level<0 || level>std::numeric_limits<std::int32_t>::max())
                throw std::out_of_range("invalid level");
            const qpx::RegionRequest request{number(argv[4]),number(argv[5]),static_cast<std::int32_t>(level),
                                             number(argv[7]),number(argv[8])};
            if(receipt)
                std::cout<<qpx::export_project_display_region_with_receipt(argv[2],image,request,argv[9]);
            else
                qpx::export_project_display_region(argv[2],image,request,argv[9]);
            return 0;
        }catch(const std::exception& error) {
            std::cerr<<error.what()<<'\n';
            return 1;
        }
    }
    if(argc>1 && std::string(argv[1])=="--raw") {
        return raw_region_cli(argc,argv,explicit_reader?requested_reader:"tiff",plugin_options);
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
                     "       qupathxx_region --verify-raw-bundle ARCHIVE.zip\n"
                     "       qupathxx_region --extract-raw-bundle ARCHIVE.zip OUTPUT.npy\n"
                     "       qupathxx_region --raw-project INPUT_PROJECT IMAGE_ID OUTPUT.npy|OUTPUT.zip [raw region options]\n"
                     "       qupathxx_region --project-region INPUT_PROJECT IMAGE_ID X Y LEVEL WIDTH HEIGHT OUTPUT.pam [--receipt]\n"
                     "       qupathxx_region --reader ID [--reader-options JSON_OBJECT] [display region mode or --raw IMAGE OUTPUT.npy OPTIONS]\n"
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
        auto server=plugin_options
            ? qpx::detail::open_reader_with_plugin_options(path,selection,*plugin_options)
            : qpx::detail::open_reader(path,selection);
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
        qpx::save_region_pam(region,output,server->reader_source());
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

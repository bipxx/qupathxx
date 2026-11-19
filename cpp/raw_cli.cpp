#include "raw_cli.hpp"
#include "qupathxx/region_export.hpp"
#include <charconv>
#include <csignal>
#include <iostream>
#include <string_view>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace {
std::atomic<bool> cancelled{false};
void stop(int) {
    cancelled.store(true);
}
#ifdef _WIN32
BOOL WINAPI interrupt(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT) {
        cancelled.store(true);
        return TRUE;
    }
    return FALSE;
}
#endif
std::int64_t integer(const std::string& text) {
    std::int64_t n = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), n);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || text.empty())
        throw std::invalid_argument("invalid raw region integer");
    return n;
}
std::uint32_t index(const std::string& text) {
    const auto n = integer(text);
    if (n < 0 || n > 65535)
        throw std::out_of_range("raw axis index must be 0..65535");
    return static_cast<std::uint32_t>(n);
}
void prepare_cancellation() {
    cancelled.store(false);
    std::signal(SIGINT,stop);
    std::signal(SIGTERM,stop);
#ifdef _WIN32
    SetConsoleCtrlHandler(interrupt,TRUE);
#endif
}
} // namespace
int raw_region_bundle_cli(int argc,char** argv,bool extract) {
    try {
        if(argc!=(extract?4:3))
            throw std::invalid_argument(extract?"Usage: qupathxx_region --extract-raw-bundle ARCHIVE.zip OUTPUT.npy":
                                                "Usage: qupathxx_region --verify-raw-bundle ARCHIVE.zip");
        prepare_cancellation();
        std::cout<<(extract?qpx::extract_raw_region_bundle(argv[2],argv[3],&cancelled):
                           qpx::verify_raw_region_bundle(argv[2],&cancelled));
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<e.what()<<'\n';
        return cancelled.load()?130:2;
    }
}
int raw_region_cli(int argc, char** argv, const std::string& reader,
                   const std::optional<std::string>& plugin_options) {
    try {
        const bool project=std::string_view(argv[1])=="--raw-project";
        const int option_start=project?5:4;
        if (argc < option_start)
            throw std::invalid_argument(
                project?"Usage: qupathxx_region --raw-project INPUT_PROJECT IMAGE_ID OUTPUT.npy|OUTPUT.zip [raw region options]":
                "Usage: qupathxx_region --raw IMAGE OUTPUT.npy|OUTPUT.zip [--series N --z N --time N "
                "--channel N --channels N --x N --y N --level N --width N --height N "
                "--decode-bytes N --cache-bytes N --with-validity --level-coordinates --bundle]");
        std::uint64_t image=0;
        if(project) {
            const std::string_view value=argv[3];
            const auto parsed=std::from_chars(value.data(),value.data()+value.size(),image);
            if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() || !image)
                throw std::invalid_argument("IMAGE_ID must be a positive native image ID");
        }
        qpx::RawRegionExportOptions export_options;
        export_options.reader = reader;
        export_options.reader_plugin_options = plugin_options;
        auto& options = export_options.tiff;
        auto& request = export_options.request;
        bool tiff_option = false, follow_z=true, follow_time=true;
        for (int i = option_start; i < argc; ++i) {
            const std::string flag = argv[i];
            if (flag == "--bundle") {
                if (export_options.bundle)
                    throw std::invalid_argument("Duplicate --bundle");
                export_options.bundle = true;
                continue;
            }
            if (flag == "--level-coordinates") {
                if (export_options.level_coordinates)
                    throw std::invalid_argument("Duplicate --level-coordinates");
                export_options.level_coordinates = true;
                continue;
            }
            if (flag == "--with-validity") {
                if (export_options.with_validity)
                    throw std::invalid_argument("Duplicate --with-validity");
                export_options.with_validity = true;
                continue;
            }
            if (++i >= argc)
                throw std::invalid_argument("missing raw option value");
            const std::string value = argv[i];
            if (flag == "--series") {
                options.view.series = index(value);
                tiff_option = true;
            } else if (flag == "--z") {
                request.z = index(value);
                follow_z=false;
            } else if (flag == "--time") {
                request.t = index(value);
                follow_time=false;
            } else if (flag == "--channel")
                request.first_channel = index(value);
            else if (flag == "--channels") {
                const auto n = integer(value);
                if (n < 1 || n > 65536)
                    throw std::out_of_range("raw channel count must be 1..65536");
                request.channels = std::uint32_t(n);
            } else if (flag == "--x")
                request.region.x = integer(value);
            else if (flag == "--y")
                request.region.y = integer(value);
            else if (flag == "--width") {
                request.region.width = integer(value);
                export_options.full_width = false;
            } else if (flag == "--height") {
                request.region.height = integer(value);
                export_options.full_height = false;
            } else if (flag == "--level") {
                const auto n = integer(value);
                if (n < 0 || n > 65535)
                    throw std::out_of_range("invalid raw pyramid level");
                request.region.level = std::int32_t(n);
            } else if (flag == "--decode-bytes" || flag == "--cache-bytes") {
                const auto n = integer(value);
                if (n < 0 || n > 256 * 1024 * 1024)
                    throw std::out_of_range("invalid TIFF memory budget");
                (flag == "--decode-bytes" ? options.decode_bytes : options.cache_bytes) =
                    std::size_t(n);
                tiff_option = true;
            } else
                throw std::invalid_argument("unknown raw region option: " + flag);
        }
        if (project && tiff_option)
            throw std::invalid_argument("--raw-project uses the saved reader; TIFF opening options are unavailable");
        if (reader != "tiff" && tiff_option)
            throw std::invalid_argument(
                "--series/--decode-bytes/--cache-bytes are TIFF-only options");
        prepare_cancellation();
        const auto report = project
            ? qpx::export_project_raw_region(argv[2],image,argv[4],{export_options,follow_z,follow_time},&cancelled)
            : qpx::export_raw_region(argv[2],argv[3],export_options,&cancelled);
        std::cout << qpx::raw_region_export_report_json(report);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return cancelled.load() ? 130 : 2;
    }
}

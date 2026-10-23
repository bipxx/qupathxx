#include "raw_cli.hpp"
#include "qupathxx/raw_image.hpp"
#include "qupathxx/source_identity.hpp"
#include "reader_file_guard.hpp"
#include "atomic_export.hpp"
#include <charconv>
#include <algorithm>
#include <csignal>
#include <iostream>
#include <filesystem>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace {
std::atomic<bool> cancelled{false};
void stop(int) {cancelled.store(true);}
#ifdef _WIN32
BOOL WINAPI interrupt(DWORD event) {if(event==CTRL_C_EVENT || event==CTRL_BREAK_EVENT) {cancelled.store(true);return TRUE;}return FALSE;}
#endif
std::int64_t integer(const std::string& text) {std::int64_t n=0;const auto result=std::from_chars(text.data(),text.data()+text.size(),n);if(result.ec!=std::errc{} || result.ptr!=text.data()+text.size() || text.empty()) throw std::invalid_argument("invalid raw region integer");return n;}
std::uint32_t index(const std::string& text) {const auto n=integer(text);if(n<0 || n>65535) throw std::out_of_range("raw axis index must be 0..65535");return static_cast<std::uint32_t>(n);}
}
int raw_region_cli(int argc,char** argv) {
    try {
        if(argc<4) throw std::invalid_argument("Usage: qupathxx_region --raw IMAGE OUTPUT.npy [--series N --z N --time N --channel N --channels N --x N --y N --level N --width N --height N --decode-bytes N --cache-bytes N]");
        qpx::TiffOpenOptions options;qpx::RawRegionRequest request;bool width=false,height=false;
        for(int i=4;i<argc;++i) {const std::string flag=argv[i];if(++i>=argc) throw std::invalid_argument("missing raw option value");const std::string value=argv[i];
            if(flag=="--series") options.view.series=index(value);else if(flag=="--z") request.z=index(value);else if(flag=="--time") request.t=index(value);else if(flag=="--channel") request.first_channel=index(value);
            else if(flag=="--channels") {const auto n=integer(value);if(n<1 || n>65536) throw std::out_of_range("raw channel count must be 1..65536");request.channels=std::uint32_t(n);}
            else if(flag=="--x") request.region.x=integer(value);else if(flag=="--y") request.region.y=integer(value);else if(flag=="--width") {request.region.width=integer(value);width=true;}else if(flag=="--height") {request.region.height=integer(value);height=true;}
            else if(flag=="--level") {const auto n=integer(value);if(n<0 || n>65535) throw std::out_of_range("invalid raw pyramid level");request.region.level=std::int32_t(n);}
            else if(flag=="--decode-bytes" || flag=="--cache-bytes") {const auto n=integer(value);if(n<0 || n>256*1024*1024) throw std::out_of_range("invalid TIFF memory budget");(flag=="--decode-bytes"?options.decode_bytes:options.cache_bytes)=std::size_t(n);}
            else throw std::invalid_argument("unknown raw region option: "+flag);
        }
        cancelled.store(false);std::signal(SIGINT,stop);std::signal(SIGTERM,stop);
#ifdef _WIN32
        SetConsoleCtrlHandler(interrupt,TRUE);
#endif
        const auto output=qpx::detail::export_path(argv[3]);auto server=qpx::open_tiff(argv[2],options,&cancelled);const auto metadata=server->raw_metadata(&cancelled);
        if(request.region.level<0 || std::size_t(request.region.level)>=metadata->levels.size()) throw std::out_of_range("invalid raw pyramid level");
        if(!width) request.region.width=metadata->levels[request.region.level].width;if(!height) request.region.height=metadata->levels[request.region.level].height;
        qpx::detail::guard_file_output(server->source_locator(),server->reader_source()->primary->identity,output);const auto raw=server->read_raw(request,&cancelled);
        qpx::detail::guard_file_output(server->source_locator(),server->reader_source()->primary->identity,output);qpx::save_raw_numpy(raw,output,false,&cancelled);
        const auto valid=std::count(raw.valid().begin(),raw.valid().end(),std::uint8_t(1));
        std::cout<<"{\"schema\":\"qpx.raw-region/1\",\"dtype\":\""<<qpx::sample_name(raw.sample_type())<<"\",\"height\":"<<raw.height()<<",\"width\":"<<raw.width()<<",\"channels\":"<<raw.channels()<<",\"series\":"<<options.view.series<<",\"z\":"<<request.z<<",\"time\":"<<request.t<<",\"first_channel\":"<<request.first_channel<<",\"x\":"<<request.region.x<<",\"y\":"<<request.region.y<<",\"level\":"<<request.region.level<<",\"valid_pixels\":"<<valid<<",\"raw_bytes\":"<<raw.payload_bytes()<<"}\n";return 0;
    }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return cancelled.load()?130:2;}
}

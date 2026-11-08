#include "qupathxx/spatial_counts.hpp"
#include "qupathxx/workflow.hpp"
#include <charconv>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <limits>

namespace {
std::atomic<bool> cancelled{false};
void interrupt(int) {cancelled.store(true);}
std::uint64_t integer(const std::string& text) {
    std::uint64_t value=0;const auto p=std::from_chars(text.data(),text.data()+text.size(),value);
    if(text.empty() || p.ec!=std::errc{} || p.ptr!=text.data()+text.size())throw std::invalid_argument("Expected unsigned integer");
    return value;
}
int run(const std::vector<std::string>& args) {
    try {
        if(args.size()==1 && args[0]=="--help") {
            std::cout<<"Usage: qupathxx_spatial_counts INPUT_PROJECT NEW_PROJECT IMAGE_ID COUNTS.mtx FEATURES.tsv POSITIONS.tsv --gene ID [--gene ID ...] [--z N --time N --file-bytes N --work-bytes N]\n";
            return 0;
        }
        if(args.size()<8)throw std::invalid_argument("Use --help for the spatial-count input profile");
        qpx::SpatialCountsOptions options;
        for(std::size_t i=6;i<args.size();++i) {
            const auto& flag=args[i];
            if(i+1==args.size())throw std::invalid_argument("Missing option value");
            const auto& value=args[++i];
            if(flag=="--gene")options.features.push_back(value);
            else {
                const auto n=integer(value);
                if(flag=="--z" || flag=="--time") {
                    if(n>=65536)throw std::invalid_argument("Plane index exceeds65535");
                    if(flag=="--z")options.z=static_cast<std::uint32_t>(n);else options.t=static_cast<std::uint32_t>(n);
                } else if(flag=="--file-bytes" || flag=="--work-bytes") {
                    if(n>std::numeric_limits<std::size_t>::max())throw std::invalid_argument("Byte budget exceeds platform range");
                    if(flag=="--file-bytes")options.file_bytes=static_cast<std::size_t>(n);else options.work_bytes=static_cast<std::size_t>(n);
                } else throw std::invalid_argument("Unknown spatial-count option: "+flag);
            }
        }
        std::cout<<qpx::import_spatial_counts(args[0],args[1],integer(args[2]),args[3],args[4],args[5],options,&cancelled)<<'\n';
        return 0;
    } catch(const qpx::WorkflowCancelled& error) {std::cerr<<error.what()<<'\n';return 130;}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return cancelled.load()?130:2;}
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
    std::signal(SIGINT,interrupt);std::signal(SIGTERM,interrupt);
    std::vector<std::string> args;
    for(int i=1;i<argc;++i)args.push_back(std::filesystem::path(argv[i]).u8string());
    return run(args);
}
#else
int main(int argc,char** argv) {
    std::signal(SIGINT,interrupt);std::signal(SIGTERM,interrupt);return run({argv+1,argv+argc});
}
#endif

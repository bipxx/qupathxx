#include "qupathxx/classifier.hpp"
#include "atomic_export.hpp"
#include "reader_file_guard.hpp"
#include "utf8_text.hpp"
#include <fstream>

namespace qpx {
ForestModel load_forest_model(const std::string& input,const std::atomic<bool>* cancelled) {
    detail::export_checkpoint(cancelled);detail::bounded_utf8(input,32768);
    const auto path=detail::export_path(input);detail::ReaderFileGuard observed(path);
    const auto size=observed.version()->bytes;
    if(size>64u*1024*1024) throw std::length_error("classifier model file exceeds 64 MiB");
    std::ifstream file(std::filesystem::u8path(path),std::ios::binary);
    if(!file) throw std::runtime_error("cannot open classifier model file");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    for(std::size_t at=0;at<bytes.size();) {
        detail::export_checkpoint(cancelled);const auto count=std::min<std::size_t>(65536,bytes.size()-at);
        file.read(reinterpret_cast<char*>(bytes.data()+at),static_cast<std::streamsize>(count));
        if(file.gcount()!=static_cast<std::streamsize>(count)) throw std::runtime_error("classifier model file changed or read failed");
        at+=count;
    }
    if(file.peek()!=std::char_traits<char>::eof()) throw std::runtime_error("classifier model file grew during read");
    if(file.bad()) throw std::runtime_error("classifier model file read failed");
    observed.verify();detail::export_checkpoint(cancelled);
    auto model=ForestModel::from_bytes(bytes);observed.verify();detail::export_checkpoint(cancelled);return model;
}
void save_forest_model(const ForestModel& model,const std::string& input,bool replace,const std::atomic<bool>* cancelled) {
    detail::export_checkpoint(cancelled);detail::bounded_utf8(input,32768);const auto path=detail::export_path(input);
    std::unique_ptr<detail::ReaderFileGuard> observed;
    if(replace && std::filesystem::exists(std::filesystem::u8path(path))) {
        observed=std::make_unique<detail::ReaderFileGuard>(path);
        (void)load_forest_model(path,cancelled);observed->verify();
    }
    const auto bytes=model.bytes();detail::export_checkpoint(cancelled);
    detail::AtomicExportFile output(path,64u*1024*1024,cancelled);
    output.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    const bool replacing=bool(observed);
    // Windows cannot replace a destination while our validation handle is
    // open, even with delete sharing. Keep write/delete denied through the
    // final version check, then release immediately before the atomic rename.
    if(observed) {observed->verify();observed.reset();}
    output.commit(replacing);
}
}

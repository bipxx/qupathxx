#include "tma_case_export.hpp"
#include "tma_internal.hpp"
#include "atomic_export.hpp"
#include "reader_file_guard.hpp"
#include "utf8_text.hpp"
#include <array>
#include <fstream>

namespace qpx::detail {
namespace {
std::string file_path(const std::string& value) {
    auto path=export_path(value);
#ifdef _WIN32
    auto wide=std::filesystem::u8path(path).native();
    if(wide.rfind(L"\\\\?\\",0)!=0) {
        wide=wide.rfind(L"\\\\",0)==0?L"\\\\?\\UNC\\"+wide.substr(2):L"\\\\?\\"+wide;
        path=std::filesystem::path(wide).u8string();
    }
#endif
    return path;
}
template<class ProduceCsv> std::size_t save_case_file(ProduceCsv&& produce_csv,const std::string& output_path,
    const std::string& input_path,const std::string& value,const std::string& weight,
    std::size_t limit,const std::atomic<bool>* cancelled,const ReaderFileGuard* snapshot_source) {
    export_checkpoint(cancelled);
    if(!limit || limit>256*1024*1024) throw std::invalid_argument("invalid TMA CSV byte limit");
    bounded_utf8(value,128);bounded_utf8(weight,128);
    if(input_path.empty()!=value.empty() || (input_path.empty() && !weight.empty()))
        throw std::invalid_argument("TMA case measurements require an input file and value column together");
    const auto output=file_path(output_path),input=input_path.empty()?"":file_path(input_path);
    if(snapshot_source) snapshot_source->verify();
    std::string table;std::unique_ptr<ReaderFileGuard> observed;
    if(!input.empty()) {
        observed=std::make_unique<ReaderFileGuard>(input);
        guard_file_output(input,observed->version()->identity,output);
        const auto input_bytes=observed->version()->bytes;
        if(input_bytes>limit) throw std::length_error("TMA measurement CSV exceeds byte limit");
        table.reserve(static_cast<std::size_t>(input_bytes));
        std::ifstream stream(std::filesystem::u8path(input),std::ios::binary);
        if(!stream) throw std::runtime_error("cannot open TMA measurement CSV");
        std::array<char,65536> chunk;
        while(stream) {
            export_checkpoint(cancelled);stream.read(chunk.data(),chunk.size());
            const auto n=static_cast<std::size_t>(stream.gcount());
            if(n>limit-table.size()) throw std::length_error("TMA measurement CSV exceeds byte limit");
            table.append(chunk.data(),n);
        }
        if(!stream.eof() || stream.bad()) throw std::runtime_error("cannot read TMA measurement CSV");
        observed->verify();
        if(table.empty()) throw std::invalid_argument("TMA measurement CSV is empty");
    }
    const auto csv=produce_csv(table);
    export_checkpoint(cancelled);
    AtomicExportFile destination(output,limit,cancelled);
    destination.write(csv.data(),csv.size());
    if(observed) {observed->verify();guard_file_output(input,observed->version()->identity,output);}
    if(snapshot_source) snapshot_source->verify();
    destination.commit(false);
    return csv.size();
}
}
std::size_t save_tma_case_summary_file(const TmaGrid& snapshot,const std::string& output,
    const std::string& input,const std::string& value,const std::string& weight,
    std::size_t limit,bool unversioned,const std::atomic<bool>* cancelled,const ReaderFileGuard* source) {
    return save_case_file([&](const std::string& table) {
        return tma_case_summary_csv(snapshot,table,value,weight,limit,unversioned,cancelled);
    },output,input,value,weight,limit,cancelled,source);
}
std::size_t save_tma_cohort_case_summary_file(const TmaCohort& snapshot,const std::string& output,
    const std::string& input,const std::string& value,const std::string& weight,
    std::size_t limit,bool unversioned,const std::atomic<bool>* cancelled,const ReaderFileGuard* source) {
    validate_tma_cohort(snapshot,limit,cancelled);
    return save_case_file([&](const std::string& table) {
        return tma_cohort_case_summary_csv(snapshot,table,value,weight,limit,unversioned,cancelled);
    },output,input,value,weight,limit,cancelled,source);
}
}
namespace qpx {
std::size_t save_tma_case_summary_csv(const TmaGrid& snapshot,const std::string& output,
    const std::string& input,const std::string& value,const std::string& weight,
    std::size_t limit,bool unversioned,const std::atomic<bool>* cancelled) {
    return detail::save_tma_case_summary_file(snapshot,output,input,value,weight,limit,unversioned,cancelled);
}
std::size_t save_tma_cohort_case_summary_csv(const TmaCohort& snapshot,const std::string& output,
    const std::string& input,const std::string& value,const std::string& weight,
    std::size_t limit,bool unversioned,const std::atomic<bool>* cancelled) {
    return detail::save_tma_cohort_case_summary_file(snapshot,output,input,value,weight,limit,unversioned,cancelled);
}
}

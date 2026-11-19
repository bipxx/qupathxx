#include "tma_case_export.hpp"
#include "atomic_export.hpp"
#include "project_database_output.hpp"
#include "utf8_text.hpp"
#include <array>
#include <fstream>
#include <set>

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
    const std::vector<std::string>& input_paths,const std::string& value,const std::string& weight,
    std::size_t limit,const std::atomic<bool>* cancelled,const ReaderFileGuard* snapshot_source) {
    export_checkpoint(cancelled);
    if(!limit || limit>256*1024*1024) throw std::invalid_argument("invalid TMA CSV byte limit");
    bounded_utf8(value,128);bounded_utf8(weight,128);
    if(input_paths.empty()!=value.empty() || (input_paths.empty() && !weight.empty()))
        throw std::invalid_argument("TMA case measurements require an input file and value column together");
    if(input_paths.size()>tma_measurement_file_limit) throw std::length_error("too many TMA measurement files");
    const auto output=file_path(output_path);
    if(snapshot_source) {
        snapshot_source->verify();
        guard_project_database_output(*snapshot_source,output);
    }
    std::vector<std::unique_ptr<ReaderFileGuard>> observations;
    std::set<std::pair<std::string,std::string>> identities;
    auto remaining=limit;
    for(const auto& path:input_paths) {
        export_checkpoint(cancelled);
        auto observed=std::make_unique<ReaderFileGuard>(file_path(path));
        const auto version=observed->version();
        guard_file_output(version->path,version->identity,output);
        if(!identities.emplace(version->identity_scheme,version->identity).second)
            throw std::invalid_argument("duplicate TMA measurement file");
        if(!version->bytes) throw std::invalid_argument("TMA measurement CSV is empty");
        if(version->bytes>remaining) throw std::length_error("TMA measurement CSV inputs exceed byte limit");
        remaining-=static_cast<std::size_t>(version->bytes);
        observations.push_back(std::move(observed));
    }
    std::vector<std::string> storage(observations.size());
    std::vector<std::string_view> tables;
    tables.reserve(storage.size());
    std::array<char,65536> chunk;
    for(std::size_t i=0;i<observations.size();++i) {
        const auto& observed=*observations[i];
        const auto version=observed.version();
        const auto input_bytes=version->bytes;
        auto& table=storage[i];
        table.reserve(static_cast<std::size_t>(input_bytes));
        std::ifstream stream(std::filesystem::u8path(version->path),std::ios::binary);
        if(!stream) throw std::runtime_error("cannot open TMA measurement CSV");
        while(stream) {
            export_checkpoint(cancelled);stream.read(chunk.data(),chunk.size());
            const auto n=static_cast<std::size_t>(stream.gcount());
            if(n>input_bytes-table.size()) throw std::runtime_error("TMA measurement CSV grew while reading");
            table.append(chunk.data(),n);
        }
        if(!stream.eof() || stream.bad() || table.size()!=input_bytes) throw std::runtime_error("cannot read complete TMA measurement CSV");
        observed.verify();
        tables.push_back(table);
    }
    const auto csv=produce_csv(tables);
    export_checkpoint(cancelled);
    AtomicExportFile destination(output,limit,cancelled);
    destination.write(csv.data(),csv.size());
    for(const auto& observed:observations) {
        export_checkpoint(cancelled);observed->verify();
        const auto version=observed->version();guard_file_output(version->path,version->identity,output);
    }
    if(snapshot_source) {
        snapshot_source->verify();
        guard_project_database_output(*snapshot_source,output);
    }
    destination.commit(false);
    return csv.size();
}
}
std::size_t save_tma_case_summary_file(const TmaGrid& snapshot,const std::string& output,
    const std::vector<std::string>& inputs,const std::string& value,const std::string& weight,
    std::size_t limit,bool unversioned,const std::atomic<bool>* cancelled,const ReaderFileGuard* source,TmaCaseInputScope scope) {
    return save_case_file([&](const auto& tables) {
        return tma_scoped_case_summary_csv(snapshot,tables,value,weight,limit,unversioned,cancelled,scope);
    },output,inputs,value,weight,limit,cancelled,source);
}
std::size_t save_tma_cohort_case_summary_file(const TmaCohort& snapshot,const std::string& output,
    const std::vector<std::string>& inputs,const std::string& value,const std::string& weight,
    std::size_t limit,bool unversioned,const std::atomic<bool>* cancelled,const ReaderFileGuard* source,TmaCaseInputScope scope) {
    validate_tma_cohort(snapshot,limit,cancelled);
    return save_case_file([&](const auto& tables) {
        return tma_scoped_case_summary_csv(snapshot,tables,value,weight,limit,unversioned,cancelled,scope);
    },output,inputs,value,weight,limit,cancelled,source);
}
}
namespace qpx {
std::size_t save_tma_case_summary_csv(const TmaGrid& snapshot,const std::string& output,
    const std::string& input,const std::string& value,const std::string& weight,
    std::size_t limit,bool unversioned,const std::atomic<bool>* cancelled) {
    return detail::save_tma_case_summary_file(snapshot,output,input.empty()?std::vector<std::string>{}:std::vector<std::string>{input},value,weight,limit,unversioned,cancelled);
}
std::size_t save_tma_cohort_case_summary_csv(const TmaCohort& snapshot,const std::string& output,
    const std::string& input,const std::string& value,const std::string& weight,
    std::size_t limit,bool unversioned,const std::atomic<bool>* cancelled) {
    return detail::save_tma_cohort_case_summary_file(snapshot,output,input.empty()?std::vector<std::string>{}:std::vector<std::string>{input},value,weight,limit,unversioned,cancelled);
}
}

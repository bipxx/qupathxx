#pragma once
#include "tma.hpp"
namespace qpx {
struct QptmaCore {
    std::string name,case_id;
    Rect bounds{0,0,1,1};bool missing=false;
};
struct QptmaData {
    std::string source_hint,image_name;
    std::size_t rows=1,columns=1;
    std::vector<QptmaCore> cores; // complete row-major grid, including missing cores
    void validate() const;
};
struct QptmaExport {
    QptmaData data;
    std::vector<std::string> omitted_metadata;
};
QptmaData parse_qptma(const std::string&,std::size_t byte_limit=8*1024*1024,const std::atomic<bool>* cancelled=nullptr);
std::string qptma_text(const QptmaData&,std::size_t byte_limit=8*1024*1024,const std::atomic<bool>* cancelled=nullptr);
QptmaData load_qptma(const std::string& path,std::size_t byte_limit=8*1024*1024,const std::atomic<bool>* cancelled=nullptr);
// Atomic new-file publication. Existing files (including an imported source) survive.
std::size_t save_qptma(const QptmaData&,const std::string& path,std::size_t byte_limit=8*1024*1024,const std::atomic<bool>* cancelled=nullptr);
// Conversion reports native-only fields omitted by the upstream grid format.
QptmaExport qptma_from_grid(const TmaGrid&,std::string source_hint="",std::string image_name="");
std::vector<ObjectEdit> qptma_grid_edits(const QptmaData&,std::string grid_name="Imported TMA",
    const std::vector<std::string>& row_labels={},const std::vector<std::string>& column_labels={});
// Single-owner mutation. Geometry validation and the entire undo command are atomic.
std::vector<std::uint64_t> import_qptma(Document&,const QptmaData&,std::string grid_name="Imported TMA",
    const std::vector<std::string>& row_labels={},const std::vector<std::string>& column_labels={});
}

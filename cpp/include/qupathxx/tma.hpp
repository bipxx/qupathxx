#pragma once
#include "qupathxx/core.hpp"
#include <atomic>

namespace qpx {
// Level-zero coordinates. The two step vectors permit rotated/skewed lattices;
// each core is an axis-aligned analytic ellipse.
struct TmaGridOptions {
    std::string name="TMA";
    std::size_t rows=3,columns=4;
    Point origin{50,50},row_step{0,100},column_step{100,0};
    double width=80,height=80;
    std::vector<std::string> row_labels,column_labels;
    void validate() const;
};
class TmaGridInfo : public std::enable_shared_from_this<TmaGridInfo> {
public:
    const std::string& name() const {return name_;}
    const std::vector<std::string>& row_labels() const {return rows_;}
    const std::vector<std::string>& column_labels() const {return columns_;}
    std::size_t rows() const {return rows_.size();}
    std::size_t columns() const {return columns_.size();}
    std::size_t payload_bytes() const;
    bool same_layout(const TmaGridInfo&) const;
    void validate() const;
private:
    TmaGridInfo()=default;
    std::string name_;
    std::vector<std::string> rows_,columns_;
    friend class Document;friend struct TmaCodec;
};
class TmaCoreInfo : public std::enable_shared_from_this<TmaCoreInfo> {
public:
    TmaCoreInfo(const TmaCoreInfo&)=default;
    const std::shared_ptr<const TmaGridInfo>& grid_info() const {return grid_;}
    const std::string& grid_name() const {return grid_->name();}
    const std::string& row_label() const {return grid_->row_labels()[row_];}
    const std::string& column_label() const {return grid_->column_labels()[column_];}
    std::size_t row() const {return row_;}std::size_t column() const {return column_;}
    bool missing() const {return missing_;}
    const std::string& case_id() const {return case_;}
    std::size_t payload_bytes() const;
    bool same_metadata(const TmaCoreInfo&) const;
    void validate() const;
    std::shared_ptr<const TmaCoreInfo> with_missing(bool) const;
    std::shared_ptr<const TmaCoreInfo> with_case_id(std::string) const;
private:
    TmaCoreInfo()=default;
    std::shared_ptr<const TmaGridInfo> grid_;
    std::size_t row_=0,column_=0;
    bool missing_=false;
    std::string case_;
    friend class Document;friend struct TmaCodec;
};
// Explicit immutable snapshot. A vacant slot differs from a retained missing
// core; generic object deletion leaves a vacant slot while other cores remain.
class TmaGrid : public std::enable_shared_from_this<TmaGrid> {
public:
    const std::shared_ptr<const TmaGridInfo>& info() const {return info_;}
    const std::vector<Annotation>& cores() const {return cores_;}
    std::optional<Annotation> core(std::size_t row,std::size_t column) const;
    std::size_t payload_bytes() const {return bytes_;}
    std::uint64_t document_revision() const {return revision_;}
private:
    TmaGrid()=default;
    std::shared_ptr<const TmaGridInfo> info_;
    std::vector<Annotation> cores_;
    std::size_t bytes_=0;
    std::uint64_t revision_=0;
    friend class Document;
};
// Explicit snapshot exports. Context joins require matching core revisions by
// default; callers may opt out for tables without a revision column.
std::string tma_metadata_csv(const TmaGrid&,std::size_t byte_limit=64*1024*1024);
std::string tma_context_csv(const TmaGrid&,const std::string& table,std::size_t byte_limit=64*1024*1024,bool require_revision=true);
// Replicate cores share a case ID; unassigned cores remain separate rows.
// Missing cores contribute inventory counts but no measurement values.
// Existing revision selectors are always checked. Legacy tables lacking one
// require allow_unversioned; an absent measurement table exports inventory only.
std::string tma_case_summary_csv(const TmaGrid&,const std::string& table="",const std::string& value_column="",
    const std::string& weight_column="",std::size_t byte_limit=64*1024*1024,bool allow_unversioned=false,
    const std::atomic<bool>* cancelled=nullptr);
// Read an optional bounded measurement file and atomically create a new CSV.
// Existing destinations are preserved; the input is verified before publication.
std::size_t save_tma_case_summary_csv(const TmaGrid&,const std::string& output_path,
    const std::string& input_path="",const std::string& value_column="",const std::string& weight_column="",
    std::size_t byte_limit=64*1024*1024,bool allow_unversioned=false,const std::atomic<bool>* cancelled=nullptr);
// Caller-owned positive image namespaces bind immutable grid captures. Core IDs
// are local to each image. Equal nonempty case IDs pool raw measurement rows;
// unassigned cores remain isolated by (image ID, core ID).
using TmaCohort=std::map<std::uint64_t,std::shared_ptr<const TmaGrid>>;
// Canonical source inventory binds each full metadata CSV by SHA-256. At most
// 4096 grids and byte_limit bytes of total captured payload are admitted.
std::string tma_cohort_manifest_csv(const TmaCohort&,std::size_t byte_limit=64*1024*1024,
    const std::atomic<bool>* cancelled=nullptr);
// Cohort measurement tables require image_id (or source_image_id), in addition
// to the existing core/revision selectors. Input, output and captured-payload
// budgets apply independently. Single-grid summary schema/bytes are unchanged.
std::string tma_cohort_case_summary_csv(const TmaCohort&,const std::string& table="",const std::string& value_column="",
    const std::string& weight_column="",std::size_t byte_limit=64*1024*1024,bool allow_unversioned=false,
    const std::atomic<bool>* cancelled=nullptr);
// Shared bounded loader and atomic new-file publication for cohort summaries.
std::size_t save_tma_cohort_case_summary_csv(const TmaCohort&,const std::string& output_path,
    const std::string& input_path="",const std::string& value_column="",const std::string& weight_column="",
    std::size_t byte_limit=64*1024*1024,bool allow_unversioned=false,const std::atomic<bool>* cancelled=nullptr);
std::vector<ObjectEdit> tma_grid_edits(const TmaGridOptions&);
}

#pragma once
#include "qupathxx/expression_dataset.hpp"
#include <array>
#include <optional>

namespace qpx {
enum class GeneDisplayTransform { Linear, Log1p };
enum class GeneDisplayRange { MinMax, Percentile, Manual };
enum class GeneDisplayPalette { BlueYellow, Gray };
struct GeneDisplayOptions {
    GeneDisplayTransform transform=GeneDisplayTransform::Linear;
    GeneDisplayRange range=GeneDisplayRange::MinMax;
    GeneDisplayPalette palette=GeneDisplayPalette::BlueYellow;
    double lower=0,upper=1,lower_percentile=2,upper_percentile=98,opacity=1;
    std::array<std::uint8_t,4> missing_color{145,145,145,255};
    std::array<std::uint8_t,4> zero_color{20,20,24,255};
    void validate() const;
};
struct GeneDisplayLegend {
    ExpressionValueDomain domain=ExpressionValueDomain::Counts;
    GeneDisplayTransform transform=GeneDisplayTransform::Linear;
    GeneDisplayRange range=GeneDisplayRange::MinMax;
    // Limits are in the explicitly transformed display domain, not normalized data.
    double lower=0,upper=0;
    std::size_t measured=0,missing=0,zeros=0,clipped_lower=0,clipped_upper=0;
};
class GeneDisplayRequest {
public:
    // Null rows selects all observations; an explicitly empty cohort stays empty.
    GeneDisplayRequest(ExpressionDataset,std::string feature,GeneDisplayOptions={},
        std::optional<std::vector<std::size_t>> rows=std::nullopt,
        std::size_t work_bytes=64u*1024*1024);
    const ExpressionDataset& dataset() const;
    const std::string& feature() const;
    const GeneDisplayOptions& options() const;
    const std::vector<std::size_t>& rows() const;
    std::size_t feature_index() const;
    std::size_t work_bytes() const;
    const std::string& sha256() const;
private:
    struct Data;
    std::shared_ptr<const Data> data_;
};
struct GeneDisplayField {
    ExpressionDataset dataset;
    std::string feature,request_sha256;
    GeneDisplayLegend legend;
    std::vector<std::size_t> rows;
    // Presentation only. Tooltips get exact raw counts from dataset.counts().
    std::vector<double> transformed_values;
    std::vector<std::uint8_t> valid;
    std::vector<std::array<std::uint8_t,4>> colors;
    std::size_t payload_bytes=0;
};
std::shared_ptr<const GeneDisplayField> gene_display_field(const GeneDisplayRequest&,
    const std::atomic<bool>* cancelled=nullptr);
// Serial controller-thread cache. Fields/snapshots are immutable and may be read
// by workers. A changed dataset/cohort/options identity cannot return an old field.
class GeneDisplayCache {
public:
    explicit GeneDisplayCache(std::size_t max_bytes=64u*1024*1024,std::size_t max_entries=8);
    ~GeneDisplayCache();
    GeneDisplayCache(const GeneDisplayCache&)=delete;
    std::shared_ptr<const GeneDisplayField> get(const GeneDisplayRequest&,
        const std::atomic<bool>* cancelled=nullptr);
    void clear();
    std::size_t retained_bytes() const;
    std::size_t entries() const;
private:
    struct Data;
    std::unique_ptr<Data> data_;
};
}

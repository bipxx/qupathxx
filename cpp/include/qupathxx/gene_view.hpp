#pragma once
#include "qupathxx/gene_display.hpp"

namespace qpx {
struct GeneViewOptions {
    std::array<std::string,2> genes;
    GeneDisplayOptions display;
    bool enabled=false,compare=false,shared_range=true;
    double radius=4;
    void validate() const;
};
// One versioned line for local workspace preferences, never project/count data.
std::string gene_view_preference(const GeneViewOptions&);
GeneViewOptions parse_gene_view_preference(const std::string&);
struct GeneWorkspacePreference {GeneViewOptions view;std::string source_id;};
std::string gene_workspace_preference(const GeneViewOptions&,const std::string& source_id={});
GeneWorkspacePreference parse_gene_workspace_preference(const std::string&);
struct GeneFeaturePage {std::vector<std::size_t> indices;std::size_t matches=0,page=0;};
GeneFeaturePage gene_feature_page(const ExpressionDataset&,const std::string& query,
    std::size_t page=0,std::size_t page_size=32);
// Serial owner-thread controller with one worker and latest-request coalescing.
// Worker code receives only immutable native snapshots, never a Document/UI.
class GeneViewSession {
public:
    explicit GeneViewSession(std::size_t work_bytes=64u*1024*1024);
    ~GeneViewSession();GeneViewSession(const GeneViewSession&)=delete;
    void bind(ExpressionDataset);
    void clear();
    void configure(GeneViewOptions);
    void refresh();void cancel();void poll();void wait();
    bool busy() const;
    const GeneViewOptions& options() const;
    const std::optional<ExpressionDataset>& dataset() const;
    std::shared_ptr<const GeneDisplayPair> result() const;
    const std::string& error() const;
    std::uint64_t generation() const;
private:
    struct Data;std::unique_ptr<Data> data_;
};
}

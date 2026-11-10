#pragma once
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>
namespace qpx {
class Document;
enum class MarkerTruth { Negative=0, Positive=1, Unknown=2 };
enum class MarkerCompartment { Nucleus=0, Cell=1, Cytoplasm=2 };
enum class MarkerStatistic { Mean=0, Sum=1, Minimum=2, Maximum=3, SampleSD=4 };
enum class MarkerComparison { Greater=0, GreaterEqual=1, Less=2, LessEqual=3 };
enum class MarkerUnits { SourceSample=0, SourceSampleSum=1 };
enum class MarkerOperation { Gate=0, And=1, Or=2 };
struct MarkerGate {
    std::string name;
    std::uint32_t channel=0;
    MarkerCompartment compartment=MarkerCompartment::Nucleus;
    MarkerStatistic statistic=MarkerStatistic::Mean;
    MarkerComparison comparison=MarkerComparison::Greater;
    MarkerUnits units=MarkerUnits::SourceSample;
    double threshold=0;
    std::uint64_t minimum_finite_samples=1;
};
struct MarkerNode {
    MarkerOperation operation=MarkerOperation::Gate;
    std::uint32_t gate=0;
    std::vector<std::uint32_t> inputs;
};
struct MarkerRule {
    std::string name="Marker rule",domain="original-channel-samples";
    std::string positive_class="Marker positive",negative_class="Marker negative",unknown_class="Marker unknown";
    std::vector<MarkerGate> gates;
    // Topological DAG; the last node is the root. All nodes/gates must be used.
    std::vector<MarkerNode> nodes;
    // Required only for the unmixed domain, fixing component order/meaning.
    std::string unmix_record;
    void validate() const;
    std::string sha256() const;
};
struct MarkerObservation {std::optional<double> value;std::uint64_t finite_samples=0;};
struct MarkerDecision {std::vector<MarkerTruth> gates;MarkerTruth phenotype=MarkerTruth::Unknown;};
std::string marker_measurement_key(const MarkerGate&);
MarkerTruth evaluate_marker_gate(const MarkerGate&,MarkerObservation);
// Pure value evaluation supplies no document publication authority.
MarkerDecision evaluate_marker_rule(const MarkerRule&,const std::vector<MarkerObservation>&);
struct MarkerCounts {std::size_t positive=0,negative=0,unknown=0;};
class MarkerRuleRequest {
public:
    // Captures intact native fluorescence cell observations, not image pixels.
    // Initial authority reuses the open quantitative source's native capture.
    MarkerRuleRequest(const Document&,const std::vector<std::uint64_t>&,MarkerRule,
        std::size_t work_bytes=64u*1024*1024,const std::atomic<bool>* cancelled=nullptr);
    bool matches(const Document&) const;
    const MarkerRule& rule() const;
    const std::string& input_sha256() const;
    std::size_t rows() const;
    std::size_t payload_bytes() const;
private:
    struct Data;std::shared_ptr<const Data> data_;
    friend struct MarkerRuleKernel;
    friend class MarkerRuleResult;
};
struct MarkerRow {std::uint64_t object_id=0;MarkerTruth phenotype=MarkerTruth::Unknown;};
class MarkerRuleResult {
public:
    const std::vector<MarkerRow>& rows() const{return rows_;}
    const MarkerCounts& counts() const{return counts_;}
    const std::vector<MarkerCounts>& gate_counts() const{return gate_counts_;}
    // Row-major [row, gate], owned contiguous storage for batched consumers.
    const std::vector<MarkerTruth>& gate_states() const{return gate_states_;}
    const std::string& sha256() const{return sha_;}
    const std::string& input_sha256() const;
    std::vector<std::uint64_t> cohort(MarkerTruth) const;
    std::vector<std::uint64_t> gate_cohort(std::size_t,MarkerTruth) const;
    std::size_t payload_bytes() const;
private:
    MarkerRuleResult()=default;
    std::shared_ptr<const MarkerRuleRequest::Data> snapshot_;
    std::vector<MarkerRow> rows_;MarkerCounts counts_;std::vector<MarkerCounts> gate_counts_;std::vector<MarkerTruth> gate_states_;std::string sha_;
    friend struct MarkerRuleKernel;
};
std::shared_ptr<const MarkerRuleResult> preview_marker_rule(const MarkerRuleRequest&,const std::atomic<bool>* cancelled=nullptr);
// Existing compound classification command: one undo, geometry/raw facts intact,
// class probabilities/explicit object colors cleared by existing class policy.
// Requires the exact live request and result ticket.
std::vector<std::uint64_t> apply_marker_rule(Document&,const MarkerRuleRequest&,const MarkerRuleResult&,const std::atomic<bool>* cancelled=nullptr);
// Historical publication check: unchanged saved observations and expected class
// labels, permitting the revisions changed by Apply. Does not authorize Apply.
bool verify_marker_rule_publication(const Document&,const MarkerRuleResult&,const std::atomic<bool>* cancelled=nullptr);
// Select original-domain native fluorescence cells across planes. Corrupt
// matching publications remain in the cohort so capture refuses, not hides them.
std::vector<std::uint64_t> marker_cell_ids(const Document&,const std::atomic<bool>* cancelled=nullptr);
// Explicit domain cohorts keep original and corrected observations separate.
std::vector<std::uint64_t> marker_cell_ids_for_domain(const Document&,const std::string& domain,
    const std::atomic<bool>* cancelled=nullptr);
// Unmixed rules select one recipe; malformed publications remain for refusal.
std::vector<std::uint64_t> marker_cell_ids_for_rule(const Document&,const MarkerRule&,const std::atomic<bool>* cancelled=nullptr,
    std::size_t work_bytes=64u*1024*1024);
// Exact typed rule storage uses existing image properties. Keep/remove are
// separate undoable actions and do not change observed cells or class labels.
std::string keep_marker_rule(Document&,const MarkerRule&);
std::vector<std::string> marker_rule_ids(const Document&);
MarkerRule load_marker_rule(const Document&,const std::string&);
void remove_marker_rule(Document&,const std::string&);
bool marker_rules_reference_unmix(const Document&,const std::string&);
}

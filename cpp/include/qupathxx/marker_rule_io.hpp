#pragma once
#include "qupathxx/marker_rules.hpp"
#include <functional>
namespace qpx {
// Project/JSON capability is optional; no Python parser/runtime is used.
bool has_marker_rule_io();
MarkerRule marker_rule_from_json(const std::string&);
std::string marker_rule_to_json(const MarkerRule&);
MarkerRule load_marker_rule_file(const std::string&,const std::atomic<bool>* cancelled=nullptr);
void save_marker_rule_file(const MarkerRule&,const std::string&,const std::atomic<bool>* cancelled=nullptr,
    const std::function<void()>& publication_guard={});
// Pure report formatting, not live-document publication authority. Cohorts are
// IDs grouped by root truth; gate counts are reported without repeating each rule.
std::string marker_result_json(const MarkerRuleRequest&,const MarkerRuleResult&);
}

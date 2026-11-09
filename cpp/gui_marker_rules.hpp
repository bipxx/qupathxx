#pragma once
#include "qupathxx/core.hpp"
#include "qupathxx/marker_rules.hpp"
#include <memory>
namespace qpx::detail {
class GuiMarkerRules {
public:
    enum class Action {Preview,Cancel,Apply,Keep,LoadFile,LoadSaved,Remove};
    GuiMarkerRules();
    ~GuiMarkerRules();
    void open();
    void poll(Document&);
    void draw(Document&,std::uint64_t selected,bool blocked=false);
    void set_rule(MarkerRule);
    void set_file(std::string);
    void set_selected_only(bool);
    Rect bounds(Action) const;
    bool busy() const;
    const std::string& error() const;
    const std::string& state() const;
    std::shared_ptr<const MarkerRuleResult> result() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}

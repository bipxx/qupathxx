#pragma once
#include "qupathxx/core.hpp"
namespace qpx::detail {
// Reversible field transaction. Geometry and image/reader ownership never
// enter this history record. Each row alternates its string with the live one.
struct ClassificationEdit {
    struct Row {std::uint64_t id;std::string alternate;};
    std::vector<Row> rows;
    std::vector<std::string> count_keys,class_keys;
    std::vector<std::size_t> expected_counts;
    std::vector<bool> expected_classes;
    std::map<std::string,std::size_t> counts;
    std::map<std::string,ObjectClass> classes;
};
}

#pragma once
#include "qupathxx/linear_unmixing.hpp"
#include <functional>
namespace qpx {
bool has_linear_mixing_io();
LinearMixingMatrix linear_mixing_matrix_from_json(const std::string&);
std::string linear_mixing_matrix_to_json(const LinearMixingMatrix&);
LinearMixingMatrix load_linear_mixing_matrix_file(const std::string&,const std::atomic<bool>* cancelled=nullptr);
void save_linear_mixing_matrix_file(const LinearMixingMatrix&,const std::string&,
    const std::atomic<bool>* cancelled=nullptr,const std::function<void()>& publication_guard={});
}

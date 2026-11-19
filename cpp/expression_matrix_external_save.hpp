// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "qupathxx/expression_matrix_project.hpp"

namespace qpx::detail {
enum class ExternalMatrixSave { Relink, Own };
struct SavedExternalMatrix {
    std::string project_path,matrix_path;
    ExpressionMatrixAssociation association;
};

SavedExternalMatrix save_external_expression_matrix(const std::string& input_project,
    std::uint64_t image,const std::string& association_id,const std::string& replacement,
    const std::string& output_project,ExternalMatrixSave mode,const std::atomic<bool>* cancelled);
}

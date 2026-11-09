#pragma once
#include "qupathxx/expression_matrix.hpp"
namespace qpx {
class Project;
struct ExpressionMatrixAssociation {
    // External locators are absolute in memory and relative on disk where possible.
    // Persisted owned locators are .qpx-expression/<file digest>.qmx. An unsaved
    // project may retain an absolute origin until its first save copies the bytes.
    std::string path,file_sha256,source_sha256;
    std::uint64_t features=0,observations=0,entries=0;
    bool owned=false;
    void validate() const;
    void require_source(const ExpressionMatrixSource&) const;
};
// One undoable image-data command. Owned mode copies exact bytes beside a saved
// project, or defers the copy until an unsaved project's first atomic save.
// No original inputs, observation objects or coordinates are inferred.
std::string keep_expression_matrix(Project&,std::uint64_t image,
    std::shared_ptr<const ExpressionMatrixSource>,bool owned=false,
    const std::atomic<bool>* cancelled=nullptr);
std::vector<std::string> expression_matrix_ids(const Project&,std::uint64_t image);
ExpressionMatrixAssociation load_expression_matrix_association(const Project&,
    std::uint64_t image,const std::string& id);
std::shared_ptr<const ExpressionMatrixSource> reopen_expression_matrix(const Project&,
    std::uint64_t image,const std::string& id,std::size_t work_bytes=64u*1024*1024,
    const std::atomic<bool>* cancelled=nullptr);
// Only exact content may replace the locator; scientific source identity is stable.
// Owned relink restores the managed copy; external relink changes only its locator.
void relink_expression_matrix(Project&,std::uint64_t image,const std::string& id,
    std::shared_ptr<const ExpressionMatrixSource>,const std::atomic<bool>* cancelled=nullptr);
// Upgrade an existing external association to owned storage using exact content.
// This preserves shared point references and is one undoable metadata command.
void own_expression_matrix(Project&,std::uint64_t image,const std::string& id,
    std::shared_ptr<const ExpressionMatrixSource>,const std::atomic<bool>* cancelled=nullptr);
// Removing/Undo changes metadata only. Never delete a shared managed sidecar here.
void remove_expression_matrix(Project&,std::uint64_t image,const std::string& id);
}

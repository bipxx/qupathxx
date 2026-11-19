#pragma once

#include "qupathxx/format_import.hpp"
#include "qupathxx/image_server.hpp"
#include "qupathxx/workflow.hpp"
#include "format_import_frame.hpp"
#include "project_source_internal.hpp"
#include "reader_file_guard.hpp"
#include "spatial_output_guard.hpp"
#include <functional>

namespace qpx::detail {

class FormatImportImageGuard {
public:
    FormatImportImageGuard(std::shared_ptr<ImageServer> server, const std::atomic<bool>* cancelled,
                          const SourceFileAssociation* verified_source=nullptr)
        : server_(std::move(server)) {
        workflow_checkpoint(cancelled);
        if (!server_ || server_->closed()) throw std::invalid_argument("Import image is closed");
        auto source = server_->reader_source();
        if (source && source->scope == "unopened") {
            server_->read_region({0, 0, 0, 1, 1});
            workflow_checkpoint(cancelled);
            source = server_->reader_source();
        }
        if (!source) throw std::invalid_argument("Import image has no stable source identity");
        source->validate();
        if (verified_source) require_verified_project_source(*verified_source,*source,cancelled);
        if (source->scope == "directory_store") directory_ = source->locator;
        if (!source->primary) {
            (void)dense_image(*server_);
            return;
        }
        files_.reserve(1 + source->companions.size());
        auto guarded = *source;
        files_.push_back(std::make_unique<ReaderFileGuard>(guarded.primary->path));
        guarded.primary = files_.back()->version();
        for (auto& file : guarded.companions) {
            workflow_checkpoint(cancelled);
            files_.push_back(std::make_unique<ReaderFileGuard>(file.path));
            file = *files_.back()->version();
        }
        if (!same_reader_files(*source, guarded))
            throw WorkflowInputChanged("Import image files changed after the reader opened");
    }

    void verify(const std::atomic<bool>* cancelled) const {
        workflow_checkpoint(cancelled);
        if (!directory_.empty()) server_->require_raw_current(cancelled);
        for (const auto& file : files_) {
            workflow_checkpoint(cancelled);
            file->verify();
        }
    }

    void protect(const std::string& output) const {
        if (!directory_.empty()) guard_directory_output(directory_, output);
        for (const auto& file : files_) {
            const auto version = file->version();
            protect_spatial_path(version->path, version->identity, output);
        }
    }

private:
    std::shared_ptr<ImageServer> server_;
    std::string directory_;
    std::vector<std::unique_ptr<ReaderFileGuard>> files_;
};

ExpressionMatrixImportResult import_matrix_file_guarded(const std::string& provider,
    const std::string& input, const std::string& positions, const std::string& output,
    const MatrixPluginImportOptions&, const std::optional<std::string>&,
    const std::function<void()>& verify_target, const std::atomic<bool>*);
ExpressionMatrixImportResult import_matrix_store_guarded(const std::string& provider,
    const std::string& input, const std::string& positions, const std::string& output,
    const MatrixStorePluginImportOptions&, const std::optional<std::string>&,
    const std::function<void()>& verify_target, const std::atomic<bool>*);

}

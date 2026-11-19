// SPDX-License-Identifier: AGPL-3.0-only
#include "qupathxx/expression_matrix_project.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/registration.hpp"
#include "qupathxx/workflow.hpp"
#include "spatial_output_guard.hpp"
#include "expression_matrix_project_internal.hpp"
#include "expression_matrix_external_save.hpp"
#include "format_import_internal.hpp"
#include "project_database_output.hpp"
#include "workflow_input.hpp"

namespace qpx {

IndexedMatrixAttachment attach_indexed_expression_matrix(const std::string& input_project,
        std::uint64_t image, const std::string& qmx, const std::string& expected_file_sha256,
        const std::string& new_project, const std::atomic<bool>* cancelled) {
    namespace fs = std::filesystem;
    workflow_checkpoint(cancelled);
    if (expected_file_sha256.size() != 64 ||
        expected_file_sha256.find_first_not_of("0123456789abcdef") != std::string::npos)
        throw std::invalid_argument("Matrix attachment requires the exact lowercase QMX SHA-256");
    const auto input = detail::export_path(input_project);
    const auto output = detail::export_path(new_project);
    const auto matrix = detail::export_path(qmx);
    detail::ReaderFileGuard input_guard(input), matrix_guard(matrix);
    const auto protect = [&] {
        detail::protect_spatial_path(input, input_guard.version()->identity, output);
        detail::protect_spatial_path(matrix, matrix_guard.version()->identity, output);
        for (const auto* project : {&input, &output}) {
            const auto folder = fs::u8path(*project).parent_path() / ".qpx-expression";
            detail::protect_spatial_path(folder.u8string(), "", output);
        }
    };
    protect();
    const auto target = fs::u8path(output);
    std::error_code ec;
    const auto status = fs::symlink_status(target, ec);
    if (ec && ec != std::errc::no_such_file_or_directory)
        throw fs::filesystem_error("Inspect matrix attachment destination", target, ec);
    if (status.type() != fs::file_type::not_found)
        throw WorkflowOutputExists("Matrix attachment requires a new project destination");

    const auto source = open_expression_matrix(matrix, expected_file_sha256, 64u * 1024 * 1024,
                                               64ull * 1024 * 1024 * 1024, cancelled);
    const auto& frame = source->metadata().spatial_frame;
    if (!frame || !frame->image_level_zero || frame->unit != CoordinateUnit::Pixel)
        throw std::invalid_argument("Matrix attachment requires a stored level-zero image pixel frame");
    const auto frame_sha256 = frame->sha256();
    const auto project = Project::open_without_journal_recovery(input);
    const auto document = project->document(image);
    if (std::string_view(detail::indexed_matrix_frame_id(*document)) == detail::reader_bound_matrix_frame_id &&
        frame->id != detail::reader_bound_matrix_frame_id)
        throw std::invalid_argument("Plugin image attachment requires a reader-bound QMX frame. Reindex against the selected image.");
    std::optional<detail::FormatImportImageGuard> verified_image;
    if (const auto verified=project->source_verification(image))
        verified_image.emplace(document->server(),cancelled,verified.get());
    const auto verify = [&] {
        workflow_checkpoint(cancelled);
        input_guard.verify();
        matrix_guard.verify();
        source->require_current(cancelled);
        if (verified_image)verified_image->verify(cancelled);
        if (image_coordinate_frame(*document, frame->id, frame->plane, cancelled).sha256() != frame_sha256)
            throw WorkflowInputChanged("The selected project image does not match the stored QMX frame");
    };
    verify();
    const auto id = detail::prepare_expression_matrix_attachment(*project, image, source, cancelled);
    // Revalidate source and target before the existing atomic save publishes.
    project->save_new(output, 512ull * 1024 * 1024, 1024ull * 1024 * 1024, cancelled, [&] {
        protect();
        verify();
    });
    const auto reopened = Project::open_without_journal_recovery(output);
    const auto association = load_expression_matrix_association(*reopened, image, id);
    const auto retained = reopen_expression_matrix(*reopened, image, id, 64u * 1024 * 1024);
    association.require_source(*source);
    association.require_source(*retained);
    if (!association.owned || retained->file_sha256() != expected_file_sha256)
        throw std::logic_error("The saved project did not retain the exact owned QMX");
    return {reopened->path(), association, frame_sha256};
}

namespace {
void guard_sqlite_destination_companions(const std::string& destination,const char* error) {
    if(detail::has_sqlite_companion(destination))throw WorkflowOutputExists(error);
}
}

detail::SavedExternalMatrix detail::save_external_expression_matrix(
        const std::string& input_project,std::uint64_t image,const std::string& association_id,
        const std::string& replacement,const std::string& output_project,ExternalMatrixSave mode,
        const std::atomic<bool>* cancelled) {
    namespace fs=std::filesystem;
    const bool own=mode==ExternalMatrixSave::Own;
    const WorkflowInput input(input_project,1024ull*1024*1024,true,cancelled);
    const auto project=Project::open_without_journal_recovery(input.path());
    const auto original=load_expression_matrix_association(*project,image,association_id);
    if(original.owned)throw std::invalid_argument(own?"Matrix ownership conversion requires an external project association":
                                                     "Matrix relink requires an external project association");
    const auto source=open_expression_matrix(replacement,original.file_sha256,64u*1024*1024,
                                              64ull*1024*1024*1024,cancelled);
    original.require_source(*source);
    const auto destination=export_path(output_project);
    const auto managed=(fs::u8path(destination).parent_path()/".qpx-expression"/
                        fs::u8path(original.file_sha256+".qmx")).u8string();
    const auto protect=[&] {
        input.verify(false,cancelled);
        source->require_current(cancelled);
        for(const auto* protected_path:{&input.path(),&original.path,&source->path()}) {
            for(const auto* suffix:{"","-journal","-wal","-shm"})
                guard_file_output(*protected_path+suffix,"",destination);
        }
        guard_sqlite_destination_companions(destination,own?"New ownership project destination has a SQLite companion":
                                                          "New relink project destination has a SQLite companion");
        if(own)guard_sqlite_destination_companions(managed,"Managed expression destination has a SQLite companion");
    };
    protect();
    if(own) {
        for(const auto* suffix:{"","-journal","-wal","-shm"})
            guard_file_output(input.path()+suffix,"",managed);
        if(!fs::exists(fs::u8path(managed)))guard_file_output(original.path,"",managed);
        prepare_expression_matrix_attachment(*project,image,source,cancelled,ExpressionAttachmentMode::ReuseExact);
    } else {
        relink_expression_matrix(*project,image,association_id,source,cancelled);
    }
    project->save_new(destination,512ull*1024*1024,1024ull*1024*1024,cancelled,protect);
    const auto reopened=Project::open_without_journal_recovery(destination);
    auto saved=expression_matrix_open_request(*reopened,image,association_id);
    saved.association.require_source(*source);
    if(saved.association.owned!=own)throw std::logic_error("Saved matrix ownership differs");
    return {reopened->path(),std::move(saved.path),std::move(saved.association)};
}

}

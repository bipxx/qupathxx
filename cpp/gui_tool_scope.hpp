#pragma once
#include "qupathxx/core.hpp"
#include "qupathxx/source_identity.hpp"
#include "reader_file_guard.hpp"
#include <string_view>

namespace qpx::detail {
// Uses the existing GUI friend access; no native interface or identity is added.
std::weak_ptr<const char> tool_document_identity(const Document&);
struct ToolScope {
    std::uint64_t image_id=0;
    std::string source_generation;
    ObjectPlane plane{};
    std::uint64_t document_revision=0;
    // Image IDs are project-local; an expired document can never own new state.
    std::weak_ptr<const char> document_identity;
    std::shared_ptr<const ReaderSource> observed_source;
};
struct ScopeChange {bool image=false,source=false,plane=false,document=false;};
inline ScopeChange update(ToolScope& scope,const Document& doc,std::uint64_t image_id) {
    const auto source=doc.server()->reader_source();
    const std::string_view generation=source?std::string_view(source->generation):std::string_view{};
    const auto identity=tool_document_identity(doc);
    const bool generation_changed=scope.source_generation!=generation;
    // Readers may give an immutable display projection a new generation. Use
    // the same native file-set authority as RawAnalysisRequest at that seam.
    const bool source_changed=generation_changed && (!scope.observed_source || !source || !same_reader_files(*scope.observed_source,*source));
    const ScopeChange changed{scope.image_id!=image_id || scope.document_identity.owner_before(identity) || identity.owner_before(scope.document_identity),
        source_changed,scope.plane!=doc.active_plane(),scope.document_revision!=doc.revision()};
    if(changed.image) {scope.image_id=image_id;scope.document_identity=identity;}
    if(generation_changed) {scope.source_generation.assign(generation);scope.observed_source=source;}
    if(changed.plane)scope.plane=doc.active_plane();
    if(changed.document)scope.document_revision=doc.revision();
    return changed;
}
}

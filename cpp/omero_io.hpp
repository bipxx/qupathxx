#pragma once
#include "export_output.hpp"

namespace qpx::detail {
inline void save_new_omero_file(const std::string& output, const void* bytes, std::size_t size,
                                const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);
    require_new_export(output);
    AtomicExportFile file(output, size, cancelled);
    file.write(static_cast<const char*>(bytes), size);
    commit_export_output(file, output, ExportWritePolicy::CreateNew);
}
} // namespace qpx::detail

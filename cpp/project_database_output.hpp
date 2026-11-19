// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "reader_file_guard.hpp"
#include "filesystem_access.hpp"

namespace qpx::detail {
// Protect the snapshot and the names SQLite reserves beside it, even when
// there is no journal or writer at the time of the command.
inline void guard_project_companion_output(const std::string& path,const std::string& destination) {
    for(const auto* suffix:{"-journal","-wal","-shm"})
        guard_file_output(path+suffix,"",destination);
}
inline void guard_project_database_output(const std::string& path,const std::string& identity,
                                         const std::string& destination) {
    guard_file_output(path,identity,destination);
    guard_project_companion_output(path,destination);
}
inline void guard_project_database_output(const ReaderFileGuard& snapshot,const std::string& destination) {
    const auto& version=snapshot.version();
    guard_project_database_output(version->path,version->identity,destination);
}
inline bool has_sqlite_companion(const std::string& path) {
    namespace fs=std::filesystem;
    for(const auto* suffix:{"-journal","-wal","-shm"}) {
        const auto companion=access_path(fs::u8path(path+suffix));
        std::error_code ec;
        const auto status=fs::symlink_status(companion,ec);
        if(ec && ec!=std::errc::no_such_file_or_directory)
            throw fs::filesystem_error("Inspect database companion",companion,ec);
        if(status.type()!=fs::file_type::not_found)return true;
    }
    return false;
}
}

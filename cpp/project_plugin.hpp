#pragma once
// Loader-independent saved plugin links and their native lazy project server.
#include "reader_plugin_link.hpp"
namespace qpx::detail {
std::shared_ptr<const ReaderPluginLink> plugin_link_of(const ImageServer&);
std::shared_ptr<ImageServer> project_plugin_server(ImageMetadata metadata,std::string path,
    std::shared_ptr<const ReaderPluginLink> link,std::shared_ptr<ImageServer> verified={});
// Includes the saved contract, cached metadata/source and one encoded payload.
// Zero for legacy servers; never loads or freezes the plugin catalog.
std::size_t project_plugin_snapshot_bytes(const ImageServer&);
void guard_project_plugin_output(const ImageServer&,const std::string& absolute_destination);
}

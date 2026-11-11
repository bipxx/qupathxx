#include "reader_plugin.hpp"
#include "reader_file_guard.hpp"
#include "qupathxx/workflow.hpp"
#include <stdexcept>
namespace qpx::detail {
std::shared_ptr<ImageServer> reopen_reader_plugin(const std::string& path,const ReaderPluginLink& saved,
    ReaderPluginOpenPolicy policy,const std::atomic<bool>* cancelled) {
    saved.validate();workflow_checkpoint(cancelled);
    const auto& catalog=reader_plugin_catalog();
    for(const auto& plugin:catalog.readers)if(plugin->info.provider_id==saved.implementation.provider_id) {
        const auto current=reader_plugin_implementation(plugin->info,saved.implementation.options);
        if(policy==ReaderPluginOpenPolicy::Exact && current.sha256()!=saved.implementation.sha256())
            throw WorkflowInputChanged("Reader plugin implementation changed; review and adopt explicitly");
        auto server=open_plugin_image_server(path,plugin,cancelled,saved.implementation.options);
        const auto actual=capture_native_reader_plugin_link(*server);
        if(!actual)throw std::runtime_error("Reader plugin did not provide a native source contract");
        if(policy==ReaderPluginOpenPolicy::Exact) {
            if(!same_reader_plugin_metadata(saved,*actual))throw WorkflowInputChanged("Reader plugin metadata changed; review explicitly");
            if(!same_reader_files(saved.observed,actual->observed))throw WorkflowInputChanged("Reader plugin source files changed; review explicitly");
        }
        return server;
    }
    throw std::runtime_error("Reader plugin provider unavailable: "+saved.implementation.provider_id+"; configure its admitted manifest before reading source pixels");
}
bool reader_plugin_cached_available(const ReaderPluginLink& saved) noexcept {
    try {
        const auto* catalog=cached_reader_plugin_catalog();if(!catalog)return false;
        for(const auto& plugin:catalog->readers)if(plugin->info.provider_id==saved.implementation.provider_id)
            return reader_plugin_implementation(plugin->info,saved.implementation.options).sha256()==saved.implementation.sha256();
    }catch(...){}
    return false;
}
}

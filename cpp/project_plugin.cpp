#include "project_plugin.hpp"
#include "reader_plugin.hpp"
#include "reader_provenance.hpp"
#include "reader_file_guard.hpp"
#include "utf8_text.hpp"
#include "qupathxx/workflow.hpp"
#include <atomic>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace qpx::detail {
namespace {
class PluginLinkedServer final : public ImageServer {
    ImageMetadata metadata_;
    std::string path_;
    std::shared_ptr<const ReaderPluginLink> link_;
    mutable std::shared_ptr<ImageServer> server_;
    mutable std::shared_ptr<const ReaderSource> source_;
    mutable std::mutex mutex_;
    std::atomic<bool> closed_{false};
    void retain_verified(std::shared_ptr<ImageServer> candidate) const {
        if(!candidate || candidate->closed())throw std::invalid_argument("Plugin project link requires an open native reader");
        const auto actual=capture_native_reader_plugin_link(*candidate);
        if(!actual || actual->implementation.sha256()!=link_->implementation.sha256() ||
            !same_reader_plugin_metadata(*actual,*link_) || actual->observed.reader!=link_->observed.reader ||
            !same_reader_files(actual->observed,link_->observed))
            throw std::invalid_argument("Plugin project source does not match its saved contract");
        const auto observed=candidate->reader_source();
        if(!observed || !observed->primary)throw std::invalid_argument("Plugin project source is not observed");
        auto next=std::make_shared<ReaderSource>(*observed);
        next->generation=std::atomic_load(&source_)->generation;next->validate();
        std::shared_ptr<const ReaderSource> published=std::move(next);
        std::atomic_store(&source_,std::move(published));server_=std::move(candidate);
    }
    void open_locked(const std::atomic<bool>* cancelled=nullptr) const {
        if(closed_)throw std::runtime_error("image server is closed");
        if(!server_)retain_verified(reopen_reader_plugin(path_,*link_,ReaderPluginOpenPolicy::Exact,cancelled));
    }
public:
    PluginLinkedServer(ImageMetadata metadata,std::string path,std::shared_ptr<const ReaderPluginLink> link,std::shared_ptr<ImageServer> verified)
        :metadata_(std::move(metadata)),path_(std::move(path)),link_(std::move(link)) {
        if(!link_)throw std::invalid_argument("Plugin project link is missing");
        link_->validate();bounded_utf8(path_,32768);
        if(path_.empty() || !std::filesystem::u8path(path_).is_absolute())throw std::invalid_argument("Plugin project path must be absolute");
        auto expected=*link_;expected.metadata=metadata_;
        if(!same_reader_plugin_metadata(expected,*link_))throw std::invalid_argument("Plugin project metadata differs from its saved contract");
        source_=make_reader_source(reader_plugin_binding(link_->implementation),path_,"unopened");
        if(verified)retain_verified(std::move(verified));
    }
    const ImageMetadata& metadata() const override {return metadata_;}
    std::string reader_id() const override {return link_->implementation.provider_id;}
    std::string source_locator() const override {return path_;}
    std::shared_ptr<const ReaderSource> reader_source() const override {return std::atomic_load(&source_);}
    std::shared_ptr<const ReaderPluginLink> link() const {return link_;}
    PixelBuffer read_region(const RegionRequest& request) const override {
        std::lock_guard<std::mutex> lock(mutex_);open_locked();return server_->read_region(request);
    }
    std::shared_ptr<const RawImageMetadata> raw_metadata(const std::atomic<bool>* cancelled=nullptr) const override {
        workflow_checkpoint(cancelled);
        if(closed_)throw std::runtime_error("image server is closed");
        // Saved domain facts remain readable when the decoder is unavailable.
        // Pixel reads/currentness still require the exact implementation.
        return link_->raw;
    }
    RawPixelBuffer read_raw(const RawRegionRequest& request,const std::atomic<bool>* cancelled=nullptr) const override {
        if(!link_->raw)throw std::invalid_argument("image reader has no raw quantitative samples");
        request.validate(*link_->raw);
        std::lock_guard<std::mutex> lock(mutex_);open_locked(cancelled);return server_->read_raw(request,cancelled);
    }
    void require_raw_current(const std::atomic<bool>* cancelled=nullptr) const override {
        std::lock_guard<std::mutex> lock(mutex_);open_locked(cancelled);server_->require_raw_current(cancelled);
    }
    bool closed() const override {return closed_.load();}
    void close() override {
        std::lock_guard<std::mutex> lock(mutex_);if(server_)server_->close();closed_=true;
    }
};
}
std::shared_ptr<const ReaderPluginLink> plugin_link_of(const ImageServer& server) {
    if(const auto* linked=dynamic_cast<const PluginLinkedServer*>(&server))return linked->link();
    return capture_native_reader_plugin_link(server);
}
std::shared_ptr<ImageServer> project_plugin_server(ImageMetadata metadata,std::string path,
    std::shared_ptr<const ReaderPluginLink> link,std::shared_ptr<ImageServer> verified) {
    return std::make_shared<PluginLinkedServer>(std::move(metadata),std::move(path),std::move(link),std::move(verified));
}
std::size_t project_plugin_snapshot_bytes(const ImageServer& server) {
    const auto link=plugin_link_of(server);if(!link)return 0;
    const auto source=server.reader_source();
    const auto& metadata=server.metadata();
    const auto path=server.source_locator();const auto encoded=encode_reader_plugin_link(*link);
    return sizeof(PluginLinkedServer)+metadata.levels.capacity()*sizeof(ImageLevel)+metadata.vendor.capacity()+
        path.capacity()+link->payload_bytes()+encoded.capacity()+128+
        (source?source->payload_bytes():0);
}
void guard_project_plugin_output(const ImageServer& server,const std::string& destination) {
    const auto link=plugin_link_of(server);if(!link)return;
    guard_file_output(server.source_locator(),"",destination);
    const auto& observed=link->observed;
    if(observed.primary)guard_file_output(observed.primary->path,observed.primary->identity,destination);
    for(const auto& file:observed.companions)guard_file_output(file.path,file.identity,destination);
}
}

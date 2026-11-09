#include "region_statistics_codec.hpp"
#include "utf8_text.hpp"
#include "class_registry.hpp"
#include "image_axis_calibration_codec.hpp"
#include "image_analysis_settings_codec.hpp"
#include "image_data_properties_codec.hpp"
#include "expression_matrix_project_internal.hpp"
#include "object_properties_codec.hpp"
#include "reader_file_guard.hpp"
#include "project_source_internal.hpp"
#include "project_zarr.hpp"
#include "filesystem_access.hpp"
#include "atomic_export.hpp"
#include "ascii_case.hpp"
#include "tma_internal.hpp"
#include "qupathxx/raw_image.hpp"
#include "raw_analysis_internal.hpp"
#include "provenance_codec.hpp"
#include "batch_checkpoint.hpp"
#include "reader_provenance.hpp"
#include "object_snapshot.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/source_identity.hpp"
#include "tissue_archive.hpp"
#include "cell_archive.hpp"
#include "geometry_archive.hpp"
#include "object_classifier_archive.hpp"
#include "pixel_classifier_archive.hpp"
#include "raw_pixel_archive.hpp"
#include "pixel_stream_record.hpp"
#include "raw_pixel_stream_record.hpp"
#include "project_schema.hpp"
#include "spatial_archive.hpp"
#include "neighborhood_archive.hpp"
#include "density_archive.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <array>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>
#include <mutex>
#include <random>
#include <stdexcept>
#ifdef QUPATHXX_HAS_PROJECT
#include <sqlite3.h>
#endif

namespace qpx {
namespace {
namespace fs=std::filesystem;
void text(const std::string& value,std::size_t limit=4096) {
    if(value.size()>limit || value.find('\0')!=std::string::npos) throw std::invalid_argument("invalid project text or path");
}
std::size_t project_class_bytes(const std::map<std::string,ObjectClass>& classes,const std::vector<std::string>& defaults) {
    std::size_t bytes=0;
    for(const auto& [name,value]:classes)bytes+=sizeof(ObjectClass)+128+name.size()+value.name.size()+value.parent.size();
    for(const auto& name:defaults)bytes+=sizeof(std::string)+name.size();return bytes;
}
void validate_project_classes(const std::map<std::string,ObjectClass>& classes,const std::vector<std::string>& defaults) {
    if(classes.size()>65536 || defaults.size()>65536 || project_class_bytes(classes,defaults)>128ull*1024*1024)throw std::length_error("project class registry exceeds admission limits");
    for(const auto& name:defaults) {detail::bounded_utf8(name,4096);if(!classes.count(name))throw std::invalid_argument("project default class is not registered");}
}
std::size_t string_metadata_bytes(const std::map<std::string,std::string>& metadata) {
    std::size_t bytes=sizeof(metadata);for(const auto& [key,value]:metadata)bytes+=128+key.size()+value.size();return bytes;
}
std::size_t validate_string_metadata_item(const std::string& key,const std::string& value) {
    detail::bounded_utf8(key,4096);detail::bounded_utf8(value,4096);return 128+key.size()+value.size();
}
void validate_string_metadata(const std::map<std::string,std::string>& metadata) {
    if(metadata.size()>4096)throw std::invalid_argument("metadata exceeds 4096 entries");
    std::size_t bytes=sizeof(metadata);
    for(const auto& [key,value]:metadata) {bytes+=validate_string_metadata_item(key,value);if(bytes>1024*1024)throw std::invalid_argument("metadata exceeds 1MiB payload");}
}
std::vector<std::string> normalize_image_tags(std::vector<std::string> tags) {
    if(tags.size()>4096)throw std::invalid_argument("image tags exceed 4096 inputs");
    std::set<std::string> seen;std::vector<std::string> result;std::size_t bytes=0;
    for(auto& tag:tags) {
        detail::bounded_utf8(tag,4096);
        if(seen.insert(tag).second) {
            bytes+=64+tag.size();if(bytes>1024*1024)throw std::invalid_argument("image tags exceed 1MiB payload");
            result.push_back(std::move(tag));
        }
    }
    return result;
}
std::string absolute(const std::string& path) {
    text(path,32768); if(path.empty()) throw std::invalid_argument("project/image path must not be empty");
    return fs::absolute(fs::u8path(path)).lexically_normal().u8string();
}
std::string reader_name(const std::string& path,std::string reader) {
    if(reader=="auto") {
        auto ext=fs::u8path(path).extension().u8string();
        detail::ascii_lower_in_place(ext);
        const auto ome=is_ome_tiff_path(path);
        reader=ext==".zarr"?"ome-zarr":ext==".ppm"?"ppm":has_tiff() && ome?"tiff":"openslide";
    }
    if(reader!="ppm" && reader!="openslide" && reader!="tiff" && reader!="ome-zarr") throw std::invalid_argument("image reader must be auto, ppm, openslide, tiff or ome-zarr");
    return reader;
}
std::shared_ptr<ImageServer> read_image(const std::string& path,const std::string& reader,std::shared_ptr<const TiffView> view={},const std::atomic<bool>* cancelled=nullptr,std::shared_ptr<const TiffOpenOptions> budgets={},std::shared_ptr<const detail::ZarrLink> zarr={}) {
    if(reader=="ome-zarr"){auto options=zarr?zarr->options:OmeZarrOpenOptions{};if(view)options.view=*view;auto server=open_ome_zarr(path,options,cancelled);if(zarr && !zarr->expected.empty() && server->reader_source()->directory_sha256!=zarr->expected)throw std::runtime_error("Zarr directory changed; relink/review explicitly");return server;}
    if(reader=="tiff") {TiffOpenOptions options;if(budgets) options=*budgets;if(view) options.view=*view;return open_tiff(path,options,cancelled);}return reader=="ppm"?ppm_image_server(path):open_slide(path);
}
void validate_metadata(const ImageMetadata& m) {
    if(m.pixel_type!=PixelType::UInt8 || m.layout!=PixelLayout::RGBA || m.width<=0 || m.height<=0 || m.levels.empty() || m.levels.size()>65536 ||
        m.levels.front().width!=m.width || m.levels.front().height!=m.height || m.levels.front().downsample!=1)
        throw std::invalid_argument("invalid project image metadata");
    text(m.vendor);
    for(const auto& l:m.levels) if(l.width<=0 || l.height<=0 || !std::isfinite(l.downsample) || l.downsample<1)
        throw std::invalid_argument("invalid project pyramid level");
    for(auto c:{m.pixel_width_um,m.pixel_height_um}) if(c && (!std::isfinite(*c) || *c<=0))
        throw std::invalid_argument("invalid project image calibration");
}
void compatible(const ImageMetadata& expected,const ImageMetadata& actual) {
    if(expected.width!=actual.width || expected.height!=actual.height || expected.levels.size()!=actual.levels.size() ||
        expected.pixel_type!=actual.pixel_type || expected.layout!=actual.layout)
        throw std::invalid_argument("relinked image dimensions or pyramid do not match project");
    for(std::size_t i=0;i<expected.levels.size();++i) {
        const auto& a=expected.levels[i]; const auto& b=actual.levels[i];
        if(a.width!=b.width || a.height!=b.height || a.downsample!=b.downsample)
            throw std::invalid_argument("relinked image pyramid does not match project");
    }
}
// Source reviews bind file facts, not a frozen object set. Check current planes
// once at publication so an edit/undo after review cannot strand annotations.
void compatible_object_planes(const Document& document,const ImageServer& candidate) {
    std::int32_t channel=-1;std::uint32_t z=0,t=0;bool nondefault=false;
    for(const auto& [id,object]:document.objects()) {
        nondefault|=object.plane!=ObjectPlane{};
        channel=std::max(channel,object.plane.c);z=std::max(z,object.plane.z);t=std::max(t,object.plane.t);
    }
    // Ordinary single-plane projects keep the existing lazy/cached reader path.
    if(!nondefault)return;
    const auto raw=candidate.raw_metadata();
    if(!raw || z>=raw->z_planes || t>=raw->timepoints || (channel>=0 && std::uint32_t(channel)>=raw->channels))
        throw std::invalid_argument("replacement source does not contain every retained object plane");
}
class LinkedServer final : public ImageServer {
    ImageMetadata metadata_;
    std::string path_,reader_;
    std::shared_ptr<const TiffView> tiff_view_;std::shared_ptr<const TiffOpenOptions> tiff_options_;std::shared_ptr<const detail::ZarrLink> zarr_;
    mutable std::shared_ptr<ImageServer> server_;
    mutable std::shared_ptr<const ReaderSource> source_;
    mutable std::mutex mutex_;
    std::atomic<bool> closed_{false};
    void retain_verified(std::shared_ptr<ImageServer> candidate) const {
        compatible(metadata_,candidate->metadata());
        const auto actual=candidate->reader_source();
        if(actual) {
            auto next=std::make_shared<ReaderSource>(*actual);
            next->generation=std::atomic_load(&source_)->generation;next->validate();
            std::shared_ptr<const ReaderSource> published=std::move(next);
            std::atomic_store(&source_,std::move(published));
        }
        server_=std::move(candidate);
    }
public:
    std::string reader_id() const override {return reader_;}
    std::string source_locator() const override {return path_;}
    std::shared_ptr<const ReaderSource> reader_source() const override {return std::atomic_load(&source_);}
    LinkedServer(ImageMetadata m,std::string path,std::string reader,std::shared_ptr<const TiffView> view={},std::shared_ptr<const TiffOpenOptions> budgets={},std::shared_ptr<ImageServer> verified={},std::shared_ptr<const detail::ZarrLink> zarr={}) : metadata_(std::move(m)),path_(std::move(path)),reader_(std::move(reader)),tiff_view_(std::move(view)),tiff_options_(std::move(budgets)),zarr_(std::move(zarr)) {
        if(reader_=="ome-zarr"){if(!zarr_ && verified)zarr_=detail::capture_zarr_link(*verified);if(!zarr_)throw std::invalid_argument("Zarr link requires saved view/directory identity");tiff_view_=std::make_shared<const TiffView>(zarr_->options.view);}
        validate_metadata(metadata_);if(reader_=="tiff" && tiff_view_ && !tiff_options_) {TiffOpenOptions options;options.view=*tiff_view_;tiff_options_=std::make_shared<const TiffOpenOptions>(options);}if((reader_=="tiff" || reader_=="ome-zarr")!=bool(tiff_view_)) throw std::invalid_argument("TIFF project link requires its captured view");
        auto identity=reader_;if(tiff_view_) {identity=reader_=="ome-zarr"?ome_zarr_view_encode(*tiff_view_):tiff_view_encode(*tiff_view_);identity.resize(identity.size()-64);identity=reader_+":"+identity;}source_=make_reader_source(identity,path_,"unopened");
        if(verified)retain_verified(std::move(verified));
    }
    const ImageMetadata& metadata() const override { return metadata_; }
    void open_locked(const std::atomic<bool>* cancelled=nullptr) const {
        if(closed_) throw std::runtime_error("image server is closed");
        if(!server_)retain_verified(read_image(path_,reader_,tiff_view_,cancelled,tiff_options_,zarr_));
    }
    PixelBuffer read_region(const RegionRequest& request) const override {
        std::lock_guard<std::mutex> lock(mutex_);open_locked();
        return server_->read_region(request);
    }
    std::shared_ptr<const TiffView> tiff_view() const override {return reader_=="tiff"?tiff_view_:nullptr;}
    std::shared_ptr<const TiffView> raw_view()const override{return tiff_view_;}
    std::shared_ptr<const detail::ZarrLink> zarr_link()const{return zarr_;}
    std::shared_ptr<const OmeZarrOpenOptions> ome_zarr_options()const override{return zarr_?std::shared_ptr<const OmeZarrOpenOptions>(zarr_,&zarr_->options):nullptr;}
    void require_raw_current(const std::atomic<bool>* c=nullptr)const override{std::lock_guard<std::mutex> lock(mutex_);open_locked(c);server_->require_raw_current(c);}
    std::shared_ptr<ImageServer> with_raw_view(const TiffView& v,const std::atomic<bool>* c=nullptr)const override{std::lock_guard<std::mutex> lock(mutex_);open_locked(c);return server_->with_raw_view(v,c);}
    std::shared_ptr<const TiffOpenOptions> tiff_options() const override {return tiff_options_;}
    std::shared_ptr<ImageServer> with_tiff_view(const TiffView& view,const std::atomic<bool>* cancelled=nullptr) const override {std::lock_guard<std::mutex> lock(mutex_);open_locked(cancelled);return server_->with_tiff_view(view,cancelled);}
    std::shared_ptr<const RawImageMetadata> raw_metadata(const std::atomic<bool>* cancelled=nullptr) const override {if(reader_!="tiff" && reader_!="ome-zarr") return {};std::lock_guard<std::mutex> lock(mutex_);if(!server_) open_locked(cancelled);return server_->raw_metadata(cancelled);}
    RawPixelBuffer read_raw(const RawRegionRequest& request,const std::atomic<bool>* cancelled=nullptr) const override {if(reader_!="tiff" && reader_!="ome-zarr") throw std::invalid_argument("image reader has no raw quantitative samples");std::lock_guard<std::mutex> lock(mutex_);open_locked(cancelled);return server_->read_raw(request,cancelled);}
    std::shared_ptr<const TiffReadStats> tiff_read_stats() const override {if(reader_!="tiff" && reader_!="ome-zarr") return {};std::lock_guard<std::mutex> lock(mutex_);return server_?server_->tiff_read_stats():std::make_shared<const TiffReadStats>();}
    bool closed() const override { return closed_.load(); }
    void close() override { std::lock_guard<std::mutex> lock(mutex_); if(server_) server_->close(); closed_=true; }
};
std::shared_ptr<const detail::ZarrLink> zarr_link_of(const std::shared_ptr<ImageServer>& server){
    if(server->reader_id()!="ome-zarr")return {};
    if(const auto* linked=dynamic_cast<const LinkedServer*>(server.get()))return linked->zarr_link();
    return detail::capture_zarr_link(*server);
}
void advance(std::uint64_t value) { if(value==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("project identity/revision exhausted"); }
}
Project::Project() {
    std::random_device random; constexpr char hex[]="0123456789abcdef";
    for(int i=0;i<32;++i) identity_.push_back(hex[random()&15]);
    instance_identity_=identity_;
}
std::unique_ptr<Project> Project::snapshot_for_save(std::size_t budget,std::size_t& bytes,const std::set<std::uint64_t>* selected) const {
    bytes=0;
    auto add=[&](std::size_t count) { if(count>budget-bytes) throw std::length_error("project snapshot exceeds memory budget"); bytes+=count; };
    add(sizeof(Project)+path_.size()+identity_.size());
    add(string_metadata_bytes(metadata_)-sizeof(metadata_));
    add(project_class_bytes(class_registry_,class_defaults_));
    for(const auto& [id,e]:entries_) {
        if(selected && !selected->count(id)) continue;
        add(sizeof(Entry)+sizeof(Document)-e.document->inactive_neighborhood_metadata_bytes()-e.document->inactive_density_metadata_bytes()+128+e.info.name.size()+e.info.source_path.size()+e.info.reader.size());
        add(string_metadata_bytes(e.info.metadata)-sizeof(e.info.metadata));
        if(e.info.description)add(e.info.description->size());
        for(const auto& tag:e.info.tags)add(64+tag.size());
        if(e.info.embedded) add(e.document->image().pixels().size());
        else add(sizeof(LinkedServer)+e.document->metadata().levels.size()*sizeof(ImageLevel)+e.document->metadata().vendor.size()+e.info.source_path.size()+e.info.reader.size()+sizeof(ReaderSource)+256+e.info.source_path.size()+e.info.reader.size());
        if(e.document->server()->raw_view()) add(sizeof(TiffView)+1024);if(e.info.reader=="ome-zarr")add(sizeof(detail::ZarrLink)+4096);
        if(const auto v=source_verifications_.find(id);v!=source_verifications_.end()) {
            add(sizeof(std::uint64_t)+sizeof(SourceFileAssociation)+v->second->source.payload_bytes()+file_fingerprint_json(v->second->fingerprint).size());
        }
        add(e.document->object_bytes_);
        if(e.document->image_properties_)add(e.document->image_properties_->payload_bytes());
        if(e.document->image_axis_calibration_)add(e.document->image_axis_calibration_->timepoints_seconds().size()*sizeof(double));
        if(e.document->image_data_properties_)add(e.document->image_data_properties_->payload_bytes());
        if(e.document->image_analysis_settings_)add(e.document->image_analysis_settings_->payload_bytes()-sizeof(ImageAnalysisSettings));
        add(e.document->object_blocks_.size()*sizeof(std::shared_ptr<const Document::ObjectBlock>));
        for(const auto& [name,c]:e.document->classes_) add(sizeof(ObjectClass)+96+name.size()+c.name.size()+c.parent.size());
        add(e.document->analysis_bytes_);
        add(e.document->raw_analysis_bytes_);
        add(e.document->raw_analysis_blocks_.values.size()*sizeof(std::shared_ptr<const Document::RawAnalysisBlock>));
        add(e.document->tissue_bytes_);
        add(e.document->cells_bytes_);
        add(e.document->density_bytes_);
        add(e.document->density_records_.size()*sizeof(std::shared_ptr<const SpatialDensityRecord>));
        add(e.document->neighborhood_bytes_);
        add(e.document->neighborhood_records_.size()*sizeof(std::shared_ptr<const SpatialNeighborhoodRecord>));
        add(e.document->spatial_bytes_);
        add(e.document->spatial_records_.size()*sizeof(std::shared_ptr<const SpatialDistanceRecord>));
        add(e.document->raw_streams_bytes_);
        add(e.document->raw_streams_.size()*sizeof(std::shared_ptr<const RawPixelStreamRecord>));
        add(e.document->streams_bytes_);
        add(e.document->streams_.size()*sizeof(std::shared_ptr<const PixelStreamRecord>));
        add(e.document->raw_pixels_bytes_);
        add(e.document->raw_pixels_.size()*sizeof(std::shared_ptr<const RawPixelRecord>));
        add(e.document->pixels_bytes_);
        add(e.document->pixels_.size()*sizeof(std::shared_ptr<const PixelClassifierRecord>));
        add(e.document->classifiers_bytes_);
        add(e.document->classifiers_.size()*sizeof(std::shared_ptr<const ObjectClassifierRecord>));
        add(e.document->cells_.size()*sizeof(std::shared_ptr<const CellRecord>));
        add(e.document->tissue_blocks_.values.size()*(sizeof(Document::TissueBlock)+sizeof(std::shared_ptr<const Document::TissueBlock>)));
        add(e.document->analysis_blocks_.values.size()*(sizeof(Document::AnalysisBlock)+sizeof(std::shared_ptr<const Document::AnalysisBlock>)));
    }
    for(const auto& [id,v]:views_) if(!selected || selected->count(id)) add(sizeof(ProjectView)+64);
    auto snapshot=std::make_unique<Project>();
    snapshot->class_registry_=class_registry_;snapshot->class_defaults_=class_defaults_;
    snapshot->metadata_=metadata_;
    snapshot->identity_=identity_; snapshot->path_=path_; snapshot->generation_=generation_;
    for(const auto& [id,value]:source_verifications_)if(!selected || selected->count(id))snapshot->source_verifications_.emplace(id,value);
    snapshot->next_id_=next_id_; snapshot->active_image_=active_image_; snapshot->revision_=revision_;
    for(const auto& [id,v]:views_) if(!selected || selected->count(id)) snapshot->views_.emplace(id,v);
    if(selected && !selected->count(snapshot->active_image_)) snapshot->active_image_=selected->empty()?0:*selected->begin();
    for(const auto& [id,e]:entries_) {
        if(selected && !selected->count(id)) continue;
        auto doc=std::shared_ptr<Document>(new Document());
        // Save-only authority: no mutable indexes, selection or edit history enter
        // the worker. Immutable ROI storage and dense source pixels are shared.
        doc->server_=e.info.embedded?e.document->server_:std::make_shared<LinkedServer>(e.document->metadata(),e.info.source_path,e.info.reader,e.document->server()->raw_view(),e.document->server()->tiff_options(),nullptr,zarr_link_of(e.document->server()));
        doc->object_snapshot_.reserve(e.document->object_blocks_.size());
        for(const auto& [key,block]:e.document->object_blocks_) doc->object_snapshot_.push_back(block);
        doc->object_snapshot_count_=e.document->objects_.size();doc->object_bytes_=e.document->object_bytes_;doc->tma_index_=e.document->tma_index_;doc->tma_revision_=e.document->tma_revision_;
        doc->classes_=e.document->classes_;
        doc->image_properties_=e.document->image_properties_;
        doc->image_data_properties_=e.document->image_data_properties_;
        doc->image_axis_calibration_=e.document->image_axis_calibration_;
        doc->image_analysis_settings_=e.document->image_analysis_settings_;
        doc->next_id_=e.document->next_id_; doc->revision_=e.document->revision_;
        doc->pixel_width_um_=e.document->pixel_width_um_; doc->pixel_height_um_=e.document->pixel_height_um_;
        const auto& blocks=e.document->analysis_blocks_.values;
        doc->analysis_snapshot_.assign(blocks.begin(),blocks.end());
        for(const auto& block:blocks) if(!block->sealed) block->sealed=true;
        doc->analysis_snapshot_count_=e.document->analysis_.size();
        doc->analysis_bytes_=e.document->analysis_bytes_;
        const auto& raw=e.document->raw_analysis_blocks_.values;
        doc->raw_analysis_snapshot_.assign(raw.begin(),raw.end());
        for(const auto& block:raw) if(!block->sealed) block->sealed=true;
        doc->raw_analysis_snapshot_count_=e.document->raw_analysis_.size();
        doc->raw_analysis_bytes_=e.document->raw_analysis_bytes_;

        const auto& masks=e.document->tissue_blocks_.values;
        doc->tissue_snapshot_.assign(masks.begin(),masks.end());
        for(const auto& block:masks) block->sealed=true;
        doc->tissue_snapshot_count_=e.document->tissue_.size();
        doc->tissue_bytes_=e.document->tissue_bytes_;
        doc->cells_snapshot_.reserve(e.document->cells_.size());
        for(const auto& entry:e.document->cells_) doc->cells_snapshot_.push_back(entry.second.value);
        doc->cells_bytes_=e.document->cells_bytes_;
        doc->spatial_snapshot_.reserve(e.document->spatial_records_.size());
        for(const auto& entry:e.document->spatial_records_) doc->spatial_snapshot_.push_back(entry.second.value);
        doc->spatial_bytes_=e.document->spatial_bytes_;
        doc->neighborhood_snapshot_.reserve(e.document->neighborhood_records_.size());
        for(const auto& entry:e.document->neighborhood_records_) doc->neighborhood_snapshot_.push_back(entry.second.value);
        doc->neighborhood_bytes_=e.document->neighborhood_bytes_;
        doc->density_snapshot_.reserve(e.document->density_records_.size());
        for(const auto& entry:e.document->density_records_) doc->density_snapshot_.push_back(entry.second.value);
        doc->density_bytes_=e.document->density_bytes_;
        doc->classifiers_snapshot_.reserve(e.document->classifiers_.size());
        for(const auto& entry:e.document->classifiers_) doc->classifiers_snapshot_.push_back(entry.second);
        doc->classifiers_bytes_=e.document->classifiers_bytes_;
        doc->pixels_snapshot_.reserve(e.document->pixels_.size());
        for(const auto& entry:e.document->pixels_) doc->pixels_snapshot_.push_back(entry.second);
        doc->pixels_bytes_=e.document->pixels_bytes_;
        doc->raw_pixels_snapshot_.reserve(e.document->raw_pixels_.size());
        for(const auto& entry:e.document->raw_pixels_) doc->raw_pixels_snapshot_.push_back(entry.second);
        doc->raw_pixels_bytes_=e.document->raw_pixels_bytes_;
        doc->streams_snapshot_.reserve(e.document->streams_.size());
        for(const auto& entry:e.document->streams_) doc->streams_snapshot_.push_back(entry.second);
        doc->streams_bytes_=e.document->streams_bytes_;
        doc->raw_streams_snapshot_.reserve(e.document->raw_streams_.size());
        for(const auto& entry:e.document->raw_streams_) doc->raw_streams_snapshot_.push_back(entry.second);
        doc->raw_streams_bytes_=e.document->raw_streams_bytes_;
        snapshot->entries_.emplace(id,Entry{e.info,std::move(doc)});
    }
    return snapshot;
}
void Project::restore_recovery(const std::string& path,const std::string& base) {
    auto restored=recover(path,base);
    adopt_workspace(*restored);
}
void Project::adopt_workspace(Project& candidate) noexcept {
    class_registry_.swap(candidate.class_registry_);class_defaults_.swap(candidate.class_defaults_);
    metadata_.swap(candidate.metadata_);entries_.swap(candidate.entries_); source_verifications_.swap(candidate.source_verifications_); views_.swap(candidate.views_); saved_revisions_.swap(candidate.saved_revisions_);
    path_.swap(candidate.path_); identity_.swap(candidate.identity_); instance_identity_.swap(candidate.instance_identity_); recovery_source_.swap(candidate.recovery_source_);
    std::swap(next_id_,candidate.next_id_); std::swap(active_image_,candidate.active_image_); std::swap(generation_,candidate.generation_);
    std::swap(revision_,candidate.revision_); std::swap(saved_revision_,candidate.saved_revision_);
}
bool Project::can_retire_on_worker() const {
    return std::all_of(entries_.begin(),entries_.end(),[](const auto& value){return value.second.native_lifetime;});
}
const Project::Entry& Project::entry(std::uint64_t id) const {
    auto it=entries_.find(id); if(it==entries_.end()) throw std::out_of_range("project image does not exist"); return it->second;
}
std::shared_ptr<Document> Project::document(std::uint64_t id) const { return entry(id).document; }
std::vector<ProjectImage> Project::images() const {
    std::vector<ProjectImage> result; result.reserve(entries_.size());
    for(const auto& [id,e]:entries_) {
        auto info=e.info; std::error_code error;
        info.available=info.embedded || fs::is_regular_file(fs::u8path(info.source_path),error); result.push_back(std::move(info));
    }
    return result;
}
std::uint64_t Project::add_image(const std::string& path,std::string reader,std::string name) {
    auto source=absolute(path); reader=reader_name(source,std::move(reader));
    auto id=add_document(std::make_shared<Document>(read_image(source,reader)),std::move(name),source,reader);
    entries_.at(id).native_lifetime=true;
    return id;
}
std::uint64_t Project::add_tiff_image(const std::string& path,const TiffOpenOptions& options,std::string name) {
    return add_tiff_source(open_tiff(absolute(path),options),std::move(name));
}
std::uint64_t Project::add_tiff_source(std::shared_ptr<ImageServer> server,std::string name) {
    if(!server || server->reader_id()!="tiff" || !server->tiff_view() || server->source_locator().empty()) throw std::invalid_argument("project requires a native opened TIFF source");
    const auto source=server->source_locator();const auto id=add_document(std::make_shared<Document>(std::move(server)),std::move(name),source,"tiff");entries_.at(id).native_lifetime=true;return id;
}
void Project::set_tiff_view(std::uint64_t id,const TiffView& view) {document(id)->set_tiff_view(view);}
std::uint64_t Project::add_document(std::shared_ptr<Document> doc,std::string name,std::string source,std::string reader) {
    if(!doc) throw std::invalid_argument("project requires a document");
    for(const auto& [id,e]:entries_) if(e.document.get()==doc.get()) throw std::invalid_argument("document already belongs to this project");
    if(source.empty()) {
        const auto native_reader=doc->server()->reader_id();
        const auto locator=doc->server()->source_locator();
        if((native_reader=="openslide" || native_reader=="tiff" || native_reader=="ome-zarr") && !locator.empty()) {
            if(reader!="auto" && reader!=native_reader) throw std::invalid_argument("reader differs from the attached document's native source");
            source=locator;reader=native_reader;
        }
    }
    advance(next_id_); advance(revision_); validate_metadata(doc->metadata());
    const bool embedded=source.empty();
    if(embedded) {
        const auto& image=doc->image();
        if(image.pixels().size()>256*1024*1024) throw std::length_error("embedded image exceeds 256 MiB; use a file link");
        reader="embedded";
    } else {
        source=absolute(source); reader=reader_name(source,std::move(reader));
        const auto trusted_tiff=reader=="tiff" && doc->server()->reader_id()=="tiff" && doc->server()->source_locator()==source && bool(doc->server()->tiff_view());
        const auto trusted_zarr=reader=="ome-zarr" && doc->server()->reader_id()=="ome-zarr" && doc->server()->source_locator()==source && bool(doc->server()->raw_view());
        if((reader=="tiff" || reader=="ome-zarr") && !trusted_tiff && !trusted_zarr) throw std::invalid_argument("attach a native TIFF document with the same source, or use add_tiff_image");
        if(fs::exists(fs::u8path(source)) && !trusted_tiff && !trusted_zarr) compatible(doc->metadata(),read_image(source,reader,doc->server()->tiff_view())->metadata());
    }
    if(name.empty()) name=embedded?"Image":fs::u8path(source).filename().u8string(); text(name);
    auto id=next_id_;
    entries_.emplace(id,Entry{{id,std::move(name),std::move(source),std::move(reader),doc->metadata().width,doc->metadata().height,embedded,true},std::move(doc)});
    if(entries_.at(id).info.reader=="ome-zarr")entries_.at(id).native_lifetime=true;
    ++next_id_; ++revision_; if(!active_image_) active_image_=id; return id;
}
void Project::remove_image(std::uint64_t id) {
    entry(id); advance(revision_); entries_.erase(id); source_verifications_.erase(id); views_.erase(id); ++revision_;
    if(active_image_==id) active_image_=entries_.empty()?0:entries_.begin()->first;
}
void ProjectView::validate() const {
    static_assert(static_cast<int>(DisplayMode::RGB)==0 && static_cast<int>(DisplayMode::Gray)==1 &&
        static_cast<int>(DisplayMode::Red)==2 && static_cast<int>(DisplayMode::Green)==3 && static_cast<int>(DisplayMode::Blue)==4,
        "Persisted schema-2 display codes require an explicit migration before changing");
    display.validate();
    const double limit=std::numeric_limits<float>::max()/4;
    if(!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(zoom) || zoom<=0 ||
       !std::isfinite(8192./zoom) || zoom>limit || std::abs(center.x*zoom)>limit || std::abs(center.y*zoom)>limit)
        throw std::invalid_argument("project view exceeds finite screen coordinate range");
}
bool ProjectView::operator==(const ProjectView& b) const {
    return center.x==b.center.x && center.y==b.center.y && zoom==b.zoom && display==b.display;
}
namespace {
void validate_view(const ProjectView& view,const ImageMetadata& image) {
    view.validate();
    if(static_cast<double>(std::max(image.width,image.height))*view.zoom>std::numeric_limits<float>::max()/4)
        throw std::invalid_argument("project view exceeds image screen coordinate range");
}
}
std::optional<ProjectView> Project::view(std::uint64_t id) const {
    entry(id); auto it=views_.find(id); return it==views_.end()?std::nullopt:std::optional<ProjectView>(it->second);
}
void Project::set_view(std::uint64_t id,ProjectView view) {
    validate_view(view,entry(id).document->metadata());
    auto existing=views_.find(id); if(existing!=views_.end() && existing->second==view) return;
    advance(revision_); views_.insert_or_assign(id,std::move(view)); ++revision_;
}
void Project::clear_view(std::uint64_t id) {
    entry(id); if(!views_.count(id)) return;
    advance(revision_); views_.erase(id); ++revision_;
}
void Project::set_active_image(std::uint64_t id) {
    if(id) entry(id); if(active_image_==id) return;
    advance(revision_); active_image_=id; ++revision_;
}
void Project::rename_image(std::uint64_t id,std::string name) {
    const auto& old=entry(id); text(name);
    if(name.empty()) throw std::invalid_argument("image name must not be empty");
    if(old.info.name==name) return;
    advance(revision_); entries_.at(id).info.name.swap(name); ++revision_;
}
void Project::set_class_registry(const std::vector<ObjectClass>& definitions,std::vector<std::string> defaults) {
    if(definitions.size()>65536 || defaults.size()>65536)throw std::length_error("project class registry exceeds admission limits");
    auto candidate=detail::normalized_class_registry(definitions);validate_project_classes(candidate,defaults);
    if(candidate==class_registry_ && defaults==class_defaults_)return;
    advance(revision_);class_registry_.swap(candidate);class_defaults_.swap(defaults);++revision_;
}
void Project::apply_class_defaults(std::uint64_t image) {
    auto document=entry(image).document;if(class_defaults_.empty())return;auto candidate=document->classes();
    const std::set<std::string> selected(class_defaults_.begin(),class_defaults_.end());std::set<std::string> applied;
    for(const auto& name:selected) {
        const auto chain=detail::class_chain(class_registry_.at(name),[&](const std::string& key)->const ObjectClass* {const auto at=class_registry_.find(key);return at==class_registry_.end()?nullptr:&at->second;},false);
        for(const auto* value:chain) {
            if(!applied.insert(value->name).second)continue;
            const auto old=candidate.find(value->name);
            if(old!=candidate.end() && old->second.parent!=value->parent)throw std::invalid_argument("project default class identity conflicts with image registry");
            candidate.insert_or_assign(value->name,*value);
        }
    }
    std::vector<ObjectClass> definitions;definitions.reserve(candidate.size());for(const auto& [name,value]:candidate)definitions.push_back(value);
    document->replace_class_registry(definitions,"Apply project classes");
}
std::map<std::string,std::string> Project::metadata() const {return metadata_;}
void Project::set_metadata(std::map<std::string,std::string> metadata) {
    validate_string_metadata(metadata);if(metadata_==metadata)return;
    advance(revision_);metadata_.swap(metadata);++revision_;
}
std::map<std::string,std::string> Project::image_metadata(std::uint64_t id) const {return entry(id).info.metadata;}
void Project::set_image_metadata(std::uint64_t id,std::map<std::string,std::string> metadata) {
    const auto& old=entry(id);validate_string_metadata(metadata);if(old.info.metadata==metadata)return;
    advance(revision_);entries_.at(id).info.metadata.swap(metadata);++revision_;
}
std::optional<std::string> Project::image_description(std::uint64_t id) const {return entry(id).info.description;}
std::vector<std::string> Project::image_tags(std::uint64_t id) const {return entry(id).info.tags;}
void Project::set_image_details(std::uint64_t id,std::optional<std::string> description,std::vector<std::string> tags) {
    const auto& old=entry(id);if(description)detail::bounded_utf8(*description,1024*1024);tags=normalize_image_tags(std::move(tags));
    if(old.info.description==description && old.info.tags==tags)return;
    advance(revision_);auto& info=entries_.at(id).info;info.description.swap(description);info.tags.swap(tags);++revision_;
}
void Project::relink(std::uint64_t id,const std::string& path) {
    if(source_verification(id))throw std::invalid_argument("saved source verification requires reviewed source replacement");
    const auto& old=entry(id); if(old.info.embedded) throw std::invalid_argument("embedded images do not require relinking");
    auto source=absolute(path); auto server=read_image(source,old.info.reader,old.document->server()->raw_view(),nullptr,old.document->server()->tiff_options(),detail::unbound_zarr_link(zarr_link_of(old.document->server())));
    // Keep recorded source metadata and manual calibration separate from new
    // reader metadata. Existing external server references retain their source.
    // Publish the same verified reader. A lazy replacement would open the path
    // again, duplicate decoding and admit a different file before the first read.
    old.document->validate_axis_source(*server);
    compatible_object_planes(*old.document,*server);
    const auto view=server->raw_view();const auto budgets=server->tiff_options();
    auto linked=std::make_shared<LinkedServer>(old.document->metadata(),source,old.info.reader,view,budgets,std::move(server));
    advance(revision_);
    entries_.at(id).info.source_path.swap(source); entries_.at(id).document->server_=std::move(linked); ++revision_;
}
std::shared_ptr<const SourceFileAssociation> Project::source_verification(std::uint64_t id) const {
    entry(id);const auto found=source_verifications_.find(id);return found==source_verifications_.end()?nullptr:found->second;
}
void Project::clear_source_verification(std::uint64_t id) {
    entry(id);if(!source_verifications_.count(id))return;
    advance(revision_);source_verifications_.erase(id);++revision_;
}
std::shared_ptr<ProjectSourceReview> Project::review_source_verification(std::uint64_t id,const FileFingerprint& fingerprint) const {
    if(!has_project())throw std::runtime_error("source verification requires project storage");
    const auto& old=entry(id);if(old.info.embedded)throw std::invalid_argument("embedded images have no source file verification");
    fingerprint.validate();const auto path=old.info.reader=="ome-zarr"?absolute(fs::u8path(fingerprint.path).parent_path().u8string()):absolute(fingerprint.path);
    const auto previous=old.document->server()->reader_source();
    std::shared_ptr<ImageServer> linked;std::vector<std::unique_ptr<detail::ReaderFileGuard>> guards;
    bool matches_live=false;
    if(previous && previous->primary) {const auto status=associate_source_file(*previous,fingerprint).status;matches_live=status=="matching_metadata" || status=="matching_primary_metadata";}
    if(old.native_lifetime && !old.document->server()->closed() && path==old.info.source_path && matches_live) {
        try {guards=detail::lock_source_verification_files(associate_source_file(*previous,fingerprint));linked=old.document->server();}
        catch(const std::runtime_error&) { /* Changed/missing native inputs need a fresh candidate; opening errors propagate below. */ }
    }
    if(!linked) {
        auto candidate=read_image(path,old.info.reader,old.document->server()->raw_view(),nullptr,old.document->server()->tiff_options(),detail::unbound_zarr_link(zarr_link_of(old.document->server())));
        const auto view=candidate->raw_view();const auto budgets=candidate->tiff_options();
        linked=std::make_shared<LinkedServer>(old.document->metadata(),path,old.info.reader,view,budgets,std::move(candidate));
    }
    const auto observed=linked->reader_source();if(!observed || !observed->primary)throw std::invalid_argument("candidate has no observed native source");
    auto association=std::make_shared<const SourceFileAssociation>(associate_source_file(*observed,fingerprint));
    if(association->status!="matching_metadata" && association->status!="matching_primary_metadata")throw std::runtime_error("fingerprint no longer matches candidate; hash and review again");
    if(guards.empty())guards=detail::lock_source_verification_files(*association);
    auto review=std::make_shared<ProjectSourceReview::Impl>();review->image=id;review->instance=instance_identity_;
    review->previous_path=old.info.source_path;review->previous_generation=previous?previous->generation:std::string{};
    review->baseline=source_verification(id);review->association=std::move(association);review->candidate=std::move(linked);review->path=path;
    if(!review->baseline)review->comparison=previous && previous->primary && (!matches_live || !detail::same_reader_files(*previous,*observed))?"changed_observed_source":"unverified";
    else if(review->baseline->fingerprint.sha256!=fingerprint.sha256 || review->baseline->fingerprint.bytes!=fingerprint.bytes)review->comparison="changed_bytes";
    else if((review->baseline->source.scope=="compound_files" || observed->scope=="compound_files" || review->baseline->source.scope=="directory_store" || observed->scope=="directory_store") && !detail::same_reader_files(review->baseline->source,*observed))review->comparison="changed_dependencies";
    else review->comparison=observed->scope=="directory_store"?"matching_observed_directory":"matching_bytes";
    review->coverage=observed->scope=="directory_store"?"directory_primary_only":observed->scope=="compound_files"?"compound_primary_only":observed->scope;
    return std::shared_ptr<ProjectSourceReview>(new ProjectSourceReview(std::move(review)));
}
void Project::apply_source_review(const std::shared_ptr<ProjectSourceReview>& review,bool accept_changed) {
    if(!review)throw std::invalid_argument("source review is required");
    auto& r=*review->impl_;std::lock_guard<std::mutex> lock(r.mutex);
    if(r.consumed)throw std::invalid_argument("source review already consumed");
    const auto& old=entry(r.image);const auto current=old.document->server()->reader_source();
    if(instance_identity_!=r.instance || old.info.source_path!=r.previous_path || !current || current->generation!=r.previous_generation || source_verification(r.image)!=r.baseline)
        throw std::runtime_error("source review is stale; review the current image again");
    if(review->requires_acceptance() && !accept_changed)throw std::invalid_argument("source or dependency facts changed; explicit accept_changed is required");
    auto path=r.path;const auto guards=detail::lock_source_verification_files(*r.association);
    if((r.comparison=="matching_bytes" || r.comparison=="matching_observed_directory") && old.info.source_path==r.path && old.document->server()==r.candidate) {r.consumed=true;return;}
    if(old.document->server()!=r.candidate) {old.document->validate_axis_source(*r.candidate);compatible_object_planes(*old.document,*r.candidate);}
    advance(revision_);
    // A matching-byte relink retains the historical baseline. Replacement starts
    // a new baseline; old analysis provenance and previously returned facts stay immutable.
    source_verifications_.insert_or_assign(r.image,(r.comparison=="matching_bytes" || r.comparison=="matching_observed_directory")?r.baseline:r.association);
    auto& target=entries_.at(r.image);target.info.source_path.swap(path);target.document->server_=r.candidate;
    ++revision_;r.consumed=true;
}
bool Project::dirty() const {
    if(path_.empty() || revision_!=saved_revision_ || saved_revisions_.size()!=entries_.size()) return true;
    for(const auto& [id,e]:entries_) { auto old=saved_revisions_.find(id); if(old==saved_revisions_.end() || old->second!=e.document->revision()) return true; }
    return false;
}
bool has_project() {
#ifdef QUPATHXX_HAS_PROJECT
    return true;
#else
    return false;
#endif
}
std::string sqlite_version() {
#ifdef QUPATHXX_HAS_PROJECT
    return sqlite3_libversion();
#else
    return "disabled";
#endif
}
#ifndef QUPATHXX_HAS_PROJECT
void Project::save(const std::string&) { throw std::runtime_error("project storage requires QUPATHXX_BUILD_PROJECT=ON"); }
void Project::save_new(const std::string&,std::size_t,std::uint64_t,const std::atomic<bool>*,const std::function<void()>&) { throw std::runtime_error("project storage requires QUPATHXX_BUILD_PROJECT=ON"); }
void Project::save_impl(const std::string&,const std::string&,const std::atomic<bool>*,const RecoveryInfo*,const detail::BatchCheckpoint*,std::uint64_t) { throw std::runtime_error("project storage requires QUPATHXX_BUILD_PROJECT=ON"); }
void Project::save_recovery(const std::string&,const std::string&,const RecoveryInfo&,const std::atomic<bool>*) { throw std::runtime_error("project recovery requires QUPATHXX_BUILD_PROJECT=ON"); }
std::shared_ptr<Project> Project::open(const std::string&) { throw std::runtime_error("project storage requires QUPATHXX_BUILD_PROJECT=ON"); }
std::shared_ptr<Project> Project::open_without_journal_recovery(const std::string&) { throw std::runtime_error("project storage requires QUPATHXX_BUILD_PROJECT=ON"); }
std::shared_ptr<Project> Project::open_impl(const std::string&,bool,const std::string&,LoadProgress*,bool) { throw std::runtime_error("project recovery requires QUPATHXX_BUILD_PROJECT=ON"); }
std::shared_ptr<Project> Project::recover(const std::string&,const std::string&) { throw std::runtime_error("project recovery requires QUPATHXX_BUILD_PROJECT=ON"); }
std::pair<std::string,std::uint64_t> Project::read_save_identity(const std::string&) { throw std::runtime_error("project storage requires QUPATHXX_BUILD_PROJECT=ON"); }
std::optional<RecoveryInfo> Project::inspect_recovery_work(const std::string&,const std::atomic<bool>*) { throw std::runtime_error("project recovery requires QUPATHXX_BUILD_PROJECT=ON"); }
RecoveryInfo Project::inspect_recovery_header(const std::string&,const std::atomic<bool>*) { throw std::runtime_error("project recovery requires QUPATHXX_BUILD_PROJECT=ON"); }
RecoveryInfo Project::inspect_recovery(const std::string&) { throw std::runtime_error("project recovery requires QUPATHXX_BUILD_PROJECT=ON"); }
#else
namespace {
constexpr int application_id=1364219992;
constexpr int recovery_application_id=1364218456; // QPRX, distinct from ordinary QPXX projects.
struct Database {
    sqlite3* db=nullptr;
    Database(const std::string& path,bool write,const std::atomic<bool>* cancelled=nullptr,bool repair_journal=true) {
        // SQLite's ordinary Windows VFS does not add extended-length prefixes.
        // Adapt only physical access; source/base paths in metadata stay logical.
        const auto physical=detail::access_path(fs::u8path(path)).u8string();
        auto connect=[&](int flags) {
            auto code=sqlite3_open_v2(physical.c_str(),&db,flags|SQLITE_OPEN_NOMUTEX,nullptr);
            if(code!=SQLITE_OK) throw std::runtime_error(std::string("SQLite open: ")+(db?sqlite3_errmsg(db):"out of memory"));
            sqlite3_extended_result_codes(db,1); sqlite3_busy_timeout(db,1000);
            if(cancelled) {
                // Initial header reads can wait on another writer before the
                // usual progress handler is installed. Keep that wait cancellable.
                sqlite3_busy_handler(db,[](void* value,int retry) {
                    auto* flag=static_cast<const std::atomic<bool>*>(value);
                    if(retry>=200 || flag->load()) return 0;
                    sqlite3_sleep(5);
                    return flag->load()?0:1;
                },const_cast<std::atomic<bool>*>(cancelled));
                cancellation(cancelled);
            }
            sqlite3_limit(db,SQLITE_LIMIT_LENGTH,272*1024*1024);
        };
        try {
            connect(write?SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE:SQLITE_OPEN_READONLY);
            if(!write) {
                // A killed writer can leave a hot rollback journal. A read-only
                // connection cannot repair it. Retry only SQLite's precise
                // recovery-required condition, without creating a new file.
                auto code=sqlite3_exec(db,"PRAGMA user_version",nullptr,nullptr,nullptr);
                if(code!=SQLITE_OK && sqlite3_extended_errcode(db)==SQLITE_READONLY_ROLLBACK) {
                    if(!repair_journal)throw std::runtime_error("project requires rollback recovery; open it explicitly before source-preserving interchange");
                    sqlite3_close(db); db=nullptr;
                    connect(SQLITE_OPEN_READWRITE);
                    if(sqlite3_exec(db,"PRAGMA user_version",nullptr,nullptr,nullptr)!=SQLITE_OK)
                        throw std::runtime_error(std::string("SQLite rollback recovery: ")+sqlite3_errmsg(db));
                    sqlite3_close(db); db=nullptr;
                    connect(SQLITE_OPEN_READONLY);
                } else if(code!=SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
            }
        } catch(...) {if(db) sqlite3_close(db); db=nullptr; throw;}
    }
    ~Database() { if(db) sqlite3_close(db); }
    Database(const Database&)=delete;
    void cancellation(const std::atomic<bool>* flag) {
        if(flag) sqlite3_progress_handler(db,1000,[](void* value){return static_cast<const std::atomic<bool>*>(value)->load()?1:0;},const_cast<std::atomic<bool>*>(flag));
    }
    void exec(const char* sql) {
        char* error=nullptr; auto code=sqlite3_exec(db,sql,nullptr,nullptr,&error);
        if(code!=SQLITE_OK) { std::string message=error?error:sqlite3_errmsg(db); sqlite3_free(error); throw std::runtime_error("SQLite: "+message); }
    }
};
struct Statement {
    Database& owner; sqlite3_stmt* stmt=nullptr;
    Statement(Database& db,const char* sql):owner(db) { if(sqlite3_prepare_v2(db.db,sql,-1,&stmt,nullptr)!=SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db.db)); }
    ~Statement() { if(stmt) sqlite3_finalize(stmt); }
    void check(int code) { if(code!=SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(owner.db)); }
    void string(int i,const std::string& s) { check(sqlite3_bind_text64(stmt,i,s.data(),s.size(),SQLITE_TRANSIENT,SQLITE_UTF8)); }
    void null(int i) {check(sqlite3_bind_null(stmt,i));}
    void integer(int i,std::int64_t v) { check(sqlite3_bind_int64(stmt,i,v)); }
    void real(int i,double v) { check(sqlite3_bind_double(stmt,i,v)); }
    void optional(int i,std::optional<double> v) { if(v) real(i,*v); else check(sqlite3_bind_null(stmt,i)); }
    void blob(int i,const std::vector<std::uint8_t>& v) { check(sqlite3_bind_blob64(stmt,i,v.data(),v.size(),SQLITE_STATIC)); }
    // Borrowed bytes must remain alive through done()/reset; reject temporaries.
    void blob(int,const std::vector<std::uint8_t>&&)=delete;
    bool row() { auto code=sqlite3_step(stmt); if(code==SQLITE_ROW) return true; if(code==SQLITE_DONE) return false; throw std::runtime_error(sqlite3_errmsg(owner.db)); }
    void done() { if(row()) throw std::runtime_error("unexpected SQLite row"); check(sqlite3_reset(stmt)); check(sqlite3_clear_bindings(stmt)); }
    int type(int i) const { return sqlite3_column_type(stmt,i); }
    std::string string(int i,std::size_t maximum=32768) const {
        if(type(i)!=SQLITE_TEXT) throw std::invalid_argument("project text column has invalid type");
        auto bytes=sqlite3_column_bytes(stmt,i); if(bytes<0 || static_cast<std::size_t>(bytes)>maximum) throw std::length_error("project text too large");
        auto* p=sqlite3_column_text(stmt,i); return std::string(reinterpret_cast<const char*>(p),static_cast<std::size_t>(bytes));
    }
    std::int64_t integer(int i) const { if(type(i)!=SQLITE_INTEGER) throw std::invalid_argument("invalid project integer"); return sqlite3_column_int64(stmt,i); }
    double real(int i) const { if(type(i)!=SQLITE_FLOAT && type(i)!=SQLITE_INTEGER) throw std::invalid_argument("invalid project number"); auto v=sqlite3_column_double(stmt,i); if(!std::isfinite(v)) throw std::invalid_argument("nonfinite project number"); return v; }
    std::optional<double> optional(int i) const { return type(i)==SQLITE_NULL?std::nullopt:std::optional<double>(real(i)); }
};
struct Transaction {
    Database& db; bool committed=false;
    Transaction(Database& d,bool write):db(d) { db.exec(write?"BEGIN IMMEDIATE":"BEGIN"); }
    ~Transaction() { if(!committed) sqlite3_exec(db.db,"ROLLBACK",nullptr,nullptr,nullptr); }
    void commit() { db.exec("COMMIT"); committed=true; }
};
std::int64_t scalar(Database& db,const char* sql) { Statement s(db,sql); if(!s.row()) throw std::runtime_error("missing SQLite scalar"); return s.integer(0); }
std::uint64_t number(const std::string& text) {
    std::uint64_t value=0; auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || std::to_string(value)!=text) throw std::invalid_argument("invalid project unsigned identity");
    return value;
}
int schema(Database& db,bool allow_recovery=false) {
    auto version=scalar(db,"PRAGMA user_version");
    auto app=scalar(db,"PRAGMA application_id");
    if((app!=application_id && !(allow_recovery && app==recovery_application_id)) || (version<1 || version>detail::project_schema_version))
        throw std::invalid_argument("not a supported qupathxx project schema");
    return static_cast<int>(version);
}
void create_views(Database& db) {
    db.exec("CREATE TABLE views(image_id TEXT PRIMARY KEY NOT NULL REFERENCES images(id),center_x REAL NOT NULL,center_y REAL NOT NULL,zoom REAL NOT NULL,mode INTEGER NOT NULL,black REAL NOT NULL,white REAL NOT NULL,gamma REAL NOT NULL) STRICT; PRAGMA user_version=2;");
}
void create_analysis(Database& db) {
    db.exec("CREATE TABLE analysis_records(image_id TEXT NOT NULL REFERENCES images(id),id TEXT NOT NULL,object_id TEXT NOT NULL,object_revision TEXT NOT NULL,document_revision TEXT NOT NULL,algorithm TEXT NOT NULL,input_sha256 TEXT NOT NULL,kind INTEGER NOT NULL,nucleus INTEGER NOT NULL,tile_size INTEGER NOT NULL,samples TEXT NOT NULL,tiles TEXT NOT NULL,elapsed_ms REAL NOT NULL,mean_r REAL NOT NULL,mean_g REAL NOT NULL,mean_b REAL NOT NULL,white_r REAL NOT NULL,white_g REAL NOT NULL,white_b REAL NOT NULL,s00 REAL NOT NULL,s01 REAL NOT NULL,s02 REAL NOT NULL,s10 REAL NOT NULL,s11 REAL NOT NULL,s12 REAL NOT NULL,s20 REAL NOT NULL,s21 REAL NOT NULL,s22 REAL NOT NULL,geometry BLOB NOT NULL,PRIMARY KEY(image_id,id)) STRICT; PRAGMA user_version=3;");
}
void create_tissue(Database& db) {
    db.exec("CREATE TABLE tissue_records(image_id TEXT NOT NULL REFERENCES images(id),id TEXT NOT NULL,producer TEXT NOT NULL,geometry BLOB NOT NULL,mask BLOB NOT NULL,PRIMARY KEY(image_id,id)) STRICT; PRAGMA user_version=4;");
}
void create_cells(Database& db) {
    db.exec("CREATE TABLE cell_records(image_id TEXT NOT NULL REFERENCES images(id),id TEXT NOT NULL,producer TEXT NOT NULL,geometry BLOB NOT NULL,run BLOB NOT NULL,PRIMARY KEY(image_id,id)) STRICT; PRAGMA user_version=5;");
}
void create_cell_links(Database& db) {
    db.exec("CREATE TABLE cell_feature_links(image_id TEXT NOT NULL,object_id TEXT NOT NULL,record_id TEXT NOT NULL,label INTEGER NOT NULL,PRIMARY KEY(image_id,object_id),FOREIGN KEY(image_id,object_id) REFERENCES objects(image_id,id)) STRICT; PRAGMA user_version=6;");
}
void create_provenance(Database& db) {
    db.exec("CREATE TABLE analysis_provenance(image_id TEXT NOT NULL REFERENCES images(id),record_id TEXT NOT NULL,payload BLOB NOT NULL,PRIMARY KEY(image_id,record_id)) STRICT; PRAGMA user_version=7;");
}
void create_classifiers(Database& db) {
    db.exec("CREATE TABLE classifier_records(image_id TEXT NOT NULL REFERENCES images(id),id TEXT NOT NULL,payload BLOB NOT NULL,PRIMARY KEY(image_id,id)) STRICT; PRAGMA user_version=8;");
}
void create_pixel_classifiers(Database& db) {
    db.exec("CREATE TABLE pixel_classifier_records(image_id TEXT NOT NULL REFERENCES images(id),id TEXT NOT NULL,payload BLOB NOT NULL,PRIMARY KEY(image_id,id)) STRICT; PRAGMA user_version=9;");
}
void create_pixel_streams(Database& db) {
    db.exec("CREATE TABLE pixel_stream_records(image_id TEXT NOT NULL REFERENCES images(id),id TEXT NOT NULL,payload BLOB NOT NULL,PRIMARY KEY(image_id,id)) STRICT; PRAGMA user_version=10;");
}
void create_tma(Database& db) {
    db.exec("CREATE TABLE tma_grids(image_id TEXT PRIMARY KEY NOT NULL REFERENCES images(id),payload BLOB NOT NULL) STRICT; CREATE TABLE tma_cores(image_id TEXT NOT NULL,object_id TEXT NOT NULL,row_index INTEGER NOT NULL,column_index INTEGER NOT NULL,missing INTEGER NOT NULL,case_id TEXT NOT NULL,PRIMARY KEY(image_id,object_id),FOREIGN KEY(image_id,object_id) REFERENCES objects(image_id,id)) STRICT; PRAGMA user_version=11;");
}
void create_raw_analysis(Database& db) {
    db.exec("CREATE TABLE raw_analysis_records(image_id TEXT NOT NULL REFERENCES images(id),id TEXT NOT NULL,payload BLOB NOT NULL,PRIMARY KEY(image_id,id)) STRICT; PRAGMA user_version=13;");
}
void create_spatial(Database& db) {
    db.exec("CREATE TABLE spatial_distance_records(image_id TEXT NOT NULL REFERENCES images(id),id TEXT NOT NULL,payload BLOB NOT NULL,PRIMARY KEY(image_id,id)) STRICT; PRAGMA user_version=14;");
}
void create_densities(Database& db) {
    db.exec("CREATE TABLE spatial_density_records(image_id TEXT NOT NULL REFERENCES images(id),id TEXT NOT NULL,payload BLOB NOT NULL,PRIMARY KEY(image_id,id)) STRICT; PRAGMA user_version=29;");
}
void create_neighborhoods(Database& db) {
    db.exec("CREATE TABLE spatial_neighborhood_records(image_id TEXT NOT NULL REFERENCES images(id),id TEXT NOT NULL,payload BLOB NOT NULL,PRIMARY KEY(image_id,id)) STRICT; PRAGMA user_version=28;");
}
void create_image_analysis_settings(Database& db) {
    db.exec("CREATE TABLE image_analysis_settings(image_id TEXT PRIMARY KEY REFERENCES images(id),payload BLOB NOT NULL) STRICT; PRAGMA user_version=20;");
}
void create_analysis_statistics(Database& db) {
    db.exec("CREATE TABLE analysis_statistics(image_id TEXT NOT NULL,id TEXT NOT NULL,payload BLOB NOT NULL,PRIMARY KEY(image_id,id),FOREIGN KEY(image_id,id) REFERENCES analysis_records(image_id,id)) STRICT; PRAGMA user_version=27;");
}
void create_class_registries(Database& db) {
    db.exec("CREATE TABLE class_semantics(image_id TEXT NOT NULL,class_name TEXT NOT NULL,parent_name TEXT NOT NULL,color_argb INTEGER,PRIMARY KEY(image_id,class_name),FOREIGN KEY(image_id,class_name) REFERENCES classes(image_id,name)) STRICT; CREATE TABLE project_classes(name TEXT PRIMARY KEY NOT NULL,parent_name TEXT NOT NULL,color_argb INTEGER) STRICT; CREATE TABLE project_class_defaults(position INTEGER PRIMARY KEY NOT NULL CHECK(position>=0 AND position<65536),class_name TEXT NOT NULL REFERENCES project_classes(name)) STRICT; PRAGMA user_version=26;");
}
void create_project_metadata(Database& db) {
    db.exec("CREATE TABLE project_metadata(metadata_key TEXT PRIMARY KEY NOT NULL,metadata_value TEXT NOT NULL) STRICT; PRAGMA user_version=25;");
}
void create_image_details(Database& db) {
    db.exec("CREATE TABLE image_details(image_id TEXT PRIMARY KEY NOT NULL REFERENCES images(id),description TEXT) STRICT; CREATE TABLE image_tags(image_id TEXT NOT NULL REFERENCES image_details(image_id),position INTEGER NOT NULL CHECK(position>=0 AND position<4096),tag TEXT NOT NULL,PRIMARY KEY(image_id,position),UNIQUE(image_id,tag)) STRICT; PRAGMA user_version=24;");
}
void create_image_metadata(Database& db) {
    db.exec("CREATE TABLE image_metadata(image_id TEXT NOT NULL REFERENCES images(id),metadata_key TEXT NOT NULL,metadata_value TEXT NOT NULL,PRIMARY KEY(image_id,metadata_key)) STRICT; PRAGMA user_version=23;");
}
void create_image_axis_calibration(Database& db) {
    db.exec("CREATE TABLE image_axis_calibration(image_id TEXT PRIMARY KEY NOT NULL REFERENCES images(id),payload BLOB NOT NULL) STRICT; PRAGMA user_version=22;");
}
void create_image_data_properties(Database& db) {
    db.exec("CREATE TABLE image_data_properties(image_id TEXT PRIMARY KEY REFERENCES images(id),payload BLOB NOT NULL) STRICT; PRAGMA user_version=21;");
}
void create_image_properties(Database& db) {
    db.exec("CREATE TABLE image_properties(image_id TEXT PRIMARY KEY REFERENCES images(id),payload BLOB NOT NULL) STRICT; PRAGMA user_version=19;");
}
void create_object_planes(Database& db) {
    db.exec("CREATE TABLE object_planes(image_id TEXT NOT NULL,object_id TEXT NOT NULL,c INTEGER NOT NULL CHECK(c>=-1 AND c<65536),z INTEGER NOT NULL CHECK(z>=0 AND z<65536),t INTEGER NOT NULL CHECK(t>=0 AND t<65536),PRIMARY KEY(image_id,object_id),FOREIGN KEY(image_id,object_id) REFERENCES objects(image_id,id)) STRICT; PRAGMA user_version=18;");
}
void create_object_properties(Database& db) {
    db.exec("CREATE TABLE object_properties(image_id TEXT NOT NULL,object_id TEXT NOT NULL,payload BLOB NOT NULL,PRIMARY KEY(image_id,object_id),FOREIGN KEY(image_id,object_id) REFERENCES objects(image_id,id)) STRICT; PRAGMA user_version=16;");
}
void create_source_verifications(Database& db) {
    db.exec("CREATE TABLE source_verifications(image_id TEXT PRIMARY KEY NOT NULL REFERENCES images(id),payload BLOB NOT NULL) STRICT; PRAGMA user_version=15;");
}
void create_tiff_views(Database& db) {
    db.exec("CREATE TABLE tiff_views(image_id TEXT PRIMARY KEY NOT NULL REFERENCES images(id),payload BLOB NOT NULL) STRICT; PRAGMA user_version=12;");
}
bool recovery_table(Database& db) { return scalar(db,"SELECT count(*) FROM sqlite_schema WHERE type='table' AND name='recovery'")!=0; }
RecoveryInfo recovery_header(Database& db,const std::string& path) {
    if(scalar(db,"PRAGMA application_id")!=recovery_application_id || schema(db,true)<2 || !recovery_table(db)) throw std::invalid_argument("not a supported recovery snapshot");
    Statement row(db,"SELECT identity,session,base_path,base_generation,created_ms,image_count,object_count FROM recovery");
    if(!row.row()) throw std::invalid_argument("missing recovery metadata");
    RecoveryInfo info; info.path=path; info.identity=row.string(0); info.session=row.string(1); info.base_path=row.string(2);
    info.base_generation=number(row.string(3)); info.created_unix_ms=row.integer(4);
    auto images=row.integer(5),objects=row.integer(6);
    auto valid_id=[](const std::string& id){return id.size()==32 && id.find_first_not_of("0123456789abcdef")==std::string::npos;};
    text(info.base_path,32768);
    if(!valid_id(info.identity) || !valid_id(info.session) || info.created_unix_ms<=0 || images<0 || objects<0 || row.row() ||
       (info.base_path.empty()?info.base_generation!=0:(!fs::u8path(info.base_path).is_absolute() || !info.base_generation || info.base_generation>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))))
        throw std::invalid_argument("invalid recovery metadata");
    info.images=static_cast<std::size_t>(images); info.objects=static_cast<std::size_t>(objects);
    Statement meta(db,"SELECT identity FROM project");
    if(!meta.row() || meta.string(0)!=info.identity || meta.row() || scalar(db,"SELECT count(*) FROM images")!=images || scalar(db,"SELECT count(*) FROM objects")!=objects)
        throw std::invalid_argument("recovery metadata does not match snapshot");
    return info;
}
std::vector<std::uint8_t> encode(const Roi& roi) {return GeometryArchiveCodec::encode(roi);}
Roi decode(Statement& s,int column) {
    const auto size=sqlite3_column_bytes(s.stmt,column);
    if(s.type(column)!=SQLITE_BLOB || size<0) throw std::invalid_argument("invalid ROI blob");
    return GeometryArchiveCodec::decode(static_cast<const std::uint8_t*>(sqlite3_column_blob(s.stmt,column)),static_cast<std::size_t>(size));
}

}
void Project::save(const std::string& path) {
    save_impl(path,"");
}
void Project::save_new(const std::string& path,std::size_t budget,std::uint64_t file_bytes,const std::atomic<bool>* cancelled,const std::function<void()>& guard) {
    detail::export_checkpoint(cancelled);
    if(!budget || budget>4ull*1024*1024*1024 || !file_bytes || file_bytes>4ull*1024*1024*1024)
        throw std::invalid_argument("new project snapshot/file budgets must be 1 byte..4 GiB");
    auto destination=absolute(path);const auto target=fs::u8path(destination);std::error_code status_error;
    const auto status=fs::symlink_status(target,status_error);
    if(status_error && status_error!=std::errc::no_such_file_or_directory)throw fs::filesystem_error("cannot inspect new project target",target,status_error);
    if(status.type()!=fs::file_type::not_found)throw std::invalid_argument("new project destination already exists");
    const auto source_path=path_;const auto source_generation=generation_,source_revision=revision_;
    std::map<std::uint64_t,std::uint64_t> revisions;for(const auto& [id,e]:entries_)revisions.emplace(id,e.document->revision());
    const auto protected_images=images();
    auto protect=[&] {if(!source_path.empty())detail::guard_file_output(source_path,"",destination);
        for(const auto& image:protected_images) {
            if(!image.embedded && !image.source_path.empty())detail::guard_file_output(image.source_path,"",destination);
            detail::guard_expression_project_output(document(image.id)->image_data_properties(),fs::u8path(source_path).parent_path(),destination);
        }};
    protect();std::size_t captured=0;auto snapshot=snapshot_for_save(budget,captured);
    struct Stage {
        fs::path directory,file;
        explicit Stage(const fs::path& parent) {
            std::random_device random;constexpr char hex[]="0123456789abcdef";
            for(int attempt=0;attempt<16;++attempt) {
                std::string suffix=".qpx-save-new-";for(int i=0;i<32;++i)suffix+=hex[random()&15];
                auto candidate=parent/fs::u8path(suffix);std::error_code error;
                if(fs::create_directory(candidate,error)){directory=std::move(candidate);file=directory/"project.qpxproj";return;}
                if(error && error!=std::errc::file_exists)throw fs::filesystem_error("cannot create new project staging directory",candidate,error);
            }
            throw std::runtime_error("cannot allocate new project staging directory");
        }
        ~Stage() {std::error_code error;for(const char* suffix:{"","-journal","-wal","-shm"}) {auto path=file;path+=suffix;fs::remove(path,error);}fs::remove(directory,error);}
    } stage(target.parent_path());
    detail::export_checkpoint(cancelled);
    snapshot->save_impl(stage.file.u8string(),target.parent_path().u8string(),cancelled,nullptr,nullptr,file_bytes);
    const auto size=fs::file_size(stage.file);if(size>file_bytes || size>std::numeric_limits<std::size_t>::max())throw std::length_error("new project exceeds file budget");
    std::ifstream input(stage.file,std::ios::binary);if(!input)throw std::runtime_error("cannot read staged project");
    detail::AtomicExportFile output(destination,static_cast<std::size_t>(file_bytes),cancelled);std::array<char,65536> bytes;
    while(input) {detail::export_checkpoint(cancelled);input.read(bytes.data(),bytes.size());const auto count=static_cast<std::size_t>(input.gcount());output.write(bytes.data(),count);}
    if(!input.eof() || input.bad() || output.bytes()!=size)throw std::runtime_error("staged project copy differs");
    if(guard)guard();protect();
    if(path_!=source_path || generation_!=source_generation || revision_!=source_revision || entries_.size()!=revisions.size())throw std::runtime_error("project changed during new-file save");
    for(const auto& [id,revision]:revisions)if(entry(id).document->revision()!=revision)throw std::runtime_error("document changed during new-file save");
    detail::export_checkpoint(cancelled);output.commit(false);
    path_.swap(destination);generation_=snapshot->generation_;saved_revision_=snapshot->saved_revision_;saved_revisions_.swap(snapshot->saved_revisions_);
}

void Project::save_impl(const std::string& path,const std::string& link_directory,const std::atomic<bool>* cancelled,const RecoveryInfo* recovery,const detail::BatchCheckpoint* batch,std::uint64_t file_bytes) {
    auto destination=absolute(path);
    for(const auto& [id,e]:entries_)
        detail::guard_expression_project_output(e.document->image_data_properties(),fs::u8path(path_).parent_path(),destination);
    std::map<std::uint64_t,std::uint64_t> revisions;
    for(const auto& [id,e]:entries_) revisions.emplace(id,e.document->revision());
    Database db(destination,true);
    db.cancellation(cancelled);
    db.exec("PRAGMA foreign_keys=ON; PRAGMA synchronous=FULL;");
    const auto maximum= batch && file_bytes?std::min(batch->max_file_bytes,file_bytes):batch?batch->max_file_bytes:file_bytes;
    if(maximum) {const auto pages=maximum/static_cast<std::uint64_t>(scalar(db,"PRAGMA page_size"));if(!pages) throw std::length_error("batch checkpoint byte limit is too small");db.exec(("PRAGMA max_page_count="+std::to_string(pages)).c_str());}
    Transaction transaction(db,true);
    const bool fresh=scalar(db,"PRAGMA application_id")==0 && scalar(db,"PRAGMA user_version")==0 &&
        scalar(db,"SELECT count(*) FROM sqlite_schema WHERE name NOT LIKE 'sqlite_%'")==0;
    std::uint64_t next_generation=1;
    if(fresh) {
        db.exec("PRAGMA application_id=1364219992; PRAGMA user_version=1;"
            "CREATE TABLE project(identity TEXT NOT NULL,next_image TEXT NOT NULL,active TEXT NOT NULL,generation INTEGER NOT NULL) STRICT;"
            "CREATE TABLE images(id TEXT PRIMARY KEY NOT NULL,name TEXT NOT NULL,reader TEXT NOT NULL,source TEXT NOT NULL,embedded INTEGER NOT NULL,width INTEGER NOT NULL,height INTEGER NOT NULL,vendor TEXT NOT NULL,server_x REAL,server_y REAL,cal_x REAL,cal_y REAL,next_object TEXT NOT NULL,revision TEXT NOT NULL,rgb BLOB) STRICT;"
            "CREATE TABLE levels(image_id TEXT NOT NULL REFERENCES images(id),ordinal INTEGER NOT NULL,width INTEGER NOT NULL,height INTEGER NOT NULL,downsample REAL NOT NULL,PRIMARY KEY(image_id,ordinal)) STRICT;"
            "CREATE TABLE classes(image_id TEXT NOT NULL REFERENCES images(id),name TEXT NOT NULL,color INTEGER NOT NULL,PRIMARY KEY(image_id,name)) STRICT;"
            "CREATE TABLE objects(image_id TEXT NOT NULL REFERENCES images(id),id TEXT NOT NULL,name TEXT NOT NULL,parent TEXT NOT NULL,class_name TEXT NOT NULL,revision TEXT NOT NULL,kind INTEGER NOT NULL,geometry BLOB NOT NULL,nucleus BLOB,PRIMARY KEY(image_id,id)) STRICT;"
            "CREATE INDEX objects_image ON objects(image_id);");
        create_views(db);
    } else {
        if(recovery_table(db)) throw std::invalid_argument("destination is a recovery snapshot; choose a project path");
        auto version=schema(db); Statement existing(db,"SELECT identity,generation FROM project");
        if(!existing.row()) throw std::invalid_argument("project metadata is missing");
        auto identity=existing.string(0); auto generation=existing.integer(1);
        if(identity!=identity_) throw std::invalid_argument("destination contains a different project; choose a new path");
        if(generation<1 || static_cast<std::uint64_t>(generation)!=generation_) throw std::runtime_error("project changed on disk; save to a new path or reopen before saving");
        if(generation==std::numeric_limits<std::int64_t>::max()) throw std::overflow_error("project save generation exhausted");
        next_generation=static_cast<std::uint64_t>(generation)+1;
        if(existing.row()) throw std::invalid_argument("duplicate project metadata");
        // Schema changes share the same transaction as canonical rows. A failed
        // save rolls back both the migration and data, retaining the v1 project.
        if(version==1) create_views(db);
        if(version<3) create_analysis(db);
        if(version<4) create_tissue(db);
        if(version<5) create_cells(db);
        if(version<6) create_cell_links(db);
        if(version<7) create_provenance(db);
        if(version<8) create_classifiers(db);
        if(version<9) create_pixel_classifiers(db);
        if(version<10) create_pixel_streams(db);
        if(version<11) create_tma(db);
        if(version<12) create_tiff_views(db);
        if(version<13) create_raw_analysis(db);
        if(version<14) create_spatial(db);
        if(version<15) create_source_verifications(db);
        if(version<16) create_object_properties(db);
        if(version<17)db.exec("PRAGMA user_version=17");
        if(version<18)create_object_planes(db);
        if(version<19)create_image_properties(db);
        if(version<20)create_image_analysis_settings(db);
        if(version<21)create_image_data_properties(db);
        if(version<22)create_image_axis_calibration(db);
        if(version<23)create_image_metadata(db);
        if(version<24)create_image_details(db);
        if(version<25)create_project_metadata(db);
        if(version<26)create_class_registries(db);
        if(version<27)create_analysis_statistics(db);
        if(version<28)create_neighborhoods(db);
        if(version<29)create_densities(db);
    }
    if(fresh) {create_analysis(db);create_tissue(db);create_cells(db);create_cell_links(db);create_provenance(db);create_classifiers(db);create_pixel_classifiers(db);create_pixel_streams(db);create_tma(db);create_tiff_views(db);create_raw_analysis(db);create_spatial(db);create_source_verifications(db);create_object_properties(db);create_object_planes(db);create_image_properties(db);create_image_analysis_settings(db);create_image_data_properties(db);create_image_axis_calibration(db);create_image_metadata(db);create_image_details(db);create_project_metadata(db);create_class_registries(db);create_analysis_statistics(db);create_neighborhoods(db);create_densities(db);}
    db.exec("DELETE FROM analysis_statistics; DELETE FROM class_semantics; DELETE FROM project_class_defaults; DELETE FROM project_classes; DELETE FROM project_metadata; DELETE FROM image_tags; DELETE FROM image_details; DELETE FROM image_metadata; DELETE FROM image_axis_calibration; DELETE FROM image_data_properties; DELETE FROM image_analysis_settings; DELETE FROM image_properties; DELETE FROM object_planes; DELETE FROM object_properties; DELETE FROM source_verifications; DELETE FROM spatial_density_records; DELETE FROM spatial_neighborhood_records; DELETE FROM spatial_distance_records; DELETE FROM raw_analysis_records; DELETE FROM tiff_views; DELETE FROM tma_cores; DELETE FROM tma_grids; DELETE FROM pixel_stream_records; DELETE FROM pixel_classifier_records; DELETE FROM classifier_records; DELETE FROM analysis_provenance; DELETE FROM cell_feature_links; DELETE FROM cell_records; DELETE FROM tissue_records; DELETE FROM analysis_records; DELETE FROM views; DELETE FROM objects; DELETE FROM classes; DELETE FROM levels; DELETE FROM images; DELETE FROM project;");
    Statement meta(db,"INSERT INTO project VALUES(?,?,?,?)");
    meta.string(1,identity_); meta.string(2,std::to_string(next_id_)); meta.string(3,std::to_string(active_image_)); meta.integer(4,static_cast<std::int64_t>(next_generation)); meta.done();
    Statement global_metadata(db,"INSERT INTO project_metadata VALUES(?,?)");
    for(const auto& [key,value]:metadata_) {global_metadata.string(1,key);global_metadata.string(2,value);global_metadata.done();}
    Statement project_class(db,"INSERT INTO project_classes VALUES(?,?,?)");
    for(const auto& [name,value]:class_registry_) {
        project_class.string(1,name);project_class.string(2,value.parent);
        if(const auto argb=value.color_argb())project_class.integer(3,*argb);else project_class.null(3);project_class.done();
    }
    Statement class_default(db,"INSERT INTO project_class_defaults VALUES(?,?)");
    for(std::size_t position=0;position<class_defaults_.size();++position) {class_default.integer(1,static_cast<std::int64_t>(position));class_default.string(2,class_defaults_[position]);class_default.done();}
    Statement image(db,"INSERT INTO images VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    Statement level(db,"INSERT INTO levels VALUES(?,?,?,?,?)");
    Statement classification(db,"INSERT INTO classes VALUES(?,?,?)");
    Statement class_semantics(db,"INSERT INTO class_semantics VALUES(?,?,?,?)");
    Statement object(db,"INSERT INTO objects VALUES(?,?,?,?,?,?,?,?,?)");
    Statement provenance(db,"INSERT INTO analysis_provenance VALUES(?,?,?)");
    auto write_provenance=[&](std::uint64_t image,std::uint64_t record,const std::shared_ptr<const AnalysisProvenance>& p) {if(p) {provenance.string(1,std::to_string(image));provenance.string(2,std::to_string(record));const auto bytes=ProvenanceCodec::encode(*p);provenance.blob(3,bytes);provenance.done();}};
    Statement tma_grid(db,"INSERT INTO tma_grids VALUES(?,?)");
    Statement tiff_view(db,"INSERT INTO tiff_views VALUES(?,?)");
    Statement tma_core(db,"INSERT INTO tma_cores VALUES(?,?,?,?,?,?)");
    Statement plane_row(db,"INSERT INTO object_planes VALUES(?,?,?,?,?)");
    Statement properties_row(db,"INSERT INTO object_properties VALUES(?,?,?)");
    Statement image_properties_row(db,"INSERT INTO image_properties VALUES(?,?)");
    Statement details_row(db,"INSERT INTO image_details VALUES(?,?)");
    Statement tag_row(db,"INSERT INTO image_tags VALUES(?,?,?)");
    Statement metadata_row(db,"INSERT INTO image_metadata VALUES(?,?,?)");
    Statement axis_row(db,"INSERT INTO image_axis_calibration VALUES(?,?)");
    Statement image_settings_row(db,"INSERT INTO image_analysis_settings VALUES(?,?)");
    Statement data_properties_row(db,"INSERT INTO image_data_properties VALUES(?,?)");
    Statement cell_link(db,"INSERT INTO cell_feature_links VALUES(?,?,?,?)");
    Statement view(db,"INSERT INTO views VALUES(?,?,?,?,?,?,?,?)");
    Statement tissue(db,"INSERT INTO tissue_records VALUES(?,?,?,?,?)");
    Statement cells(db,"INSERT INTO cell_records VALUES(?,?,?,?,?)");
    Statement classifiers(db,"INSERT INTO classifier_records VALUES(?,?,?)");
    Statement pixels(db,"INSERT INTO pixel_classifier_records VALUES(?,?,?)");
    Statement streams(db,"INSERT INTO pixel_stream_records VALUES(?,?,?)");
    Statement raw_records(db,"INSERT INTO raw_analysis_records VALUES(?,?,?)");
    Statement spatial_records(db,"INSERT INTO spatial_distance_records VALUES(?,?,?)");
    Statement density_records(db,"INSERT INTO spatial_density_records VALUES(?,?,?)");
    Statement neighborhood_records(db,"INSERT INTO spatial_neighborhood_records VALUES(?,?,?)");
    Statement verification(db,"INSERT INTO source_verifications VALUES(?,?)");
    Statement analysis_statistics(db,"INSERT INTO analysis_statistics VALUES(?,?,?)");
    Statement analysis(db,"INSERT INTO analysis_records VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    auto directory=link_directory.empty()?fs::u8path(destination).parent_path():fs::u8path(link_directory);
    for(const auto& [id,e]:entries_) {
        if(cancelled && cancelled->load()) throw std::runtime_error("recovery cancelled");
        const auto& doc=*e.document; const auto& m=doc.metadata(); validate_metadata(m);
        auto key=std::to_string(id),source=e.info.source_path;
        if(!e.info.embedded) { auto relative=detail::relative_locator(fs::u8path(source),directory); if(!relative.empty()) source=relative.generic_u8string(); }
        image.string(1,key); image.string(2,e.info.name); image.string(3,e.info.reader); image.string(4,source); image.integer(5,e.info.embedded?1:0);
        image.integer(6,m.width); image.integer(7,m.height); image.string(8,m.vendor); image.optional(9,m.pixel_width_um); image.optional(10,m.pixel_height_um);
        image.optional(11,doc.pixel_width_um_); image.optional(12,doc.pixel_height_um_); image.string(13,std::to_string(doc.next_id_)); image.string(14,std::to_string(doc.revision_));
        if(e.info.embedded) image.blob(15,doc.image().pixels());
        image.done();
        for(const auto& [name,value]:e.info.metadata) {metadata_row.string(1,key);metadata_row.string(2,name);metadata_row.string(3,value);metadata_row.done();}
        if(e.info.description || !e.info.tags.empty()) {
            details_row.string(1,key);if(e.info.description)details_row.string(2,*e.info.description);else details_row.null(2);details_row.done();
            for(std::size_t i=0;i<e.info.tags.size();++i) {tag_row.string(1,key);tag_row.integer(2,static_cast<std::int64_t>(i));tag_row.string(3,e.info.tags[i]);tag_row.done();}
        }
        if(doc.image_axis_calibration_) {const auto bytes=detail::encode_image_axis_calibration(*doc.image_axis_calibration_);axis_row.string(1,key);axis_row.blob(2,bytes);axis_row.done();}
        if(doc.image_data_properties_) {
            const auto properties=detail::expression_properties_for_save(doc.image_data_properties_,fs::u8path(path_).parent_path(),directory,cancelled);
            const auto bytes=detail::encode_image_data_properties(*properties);data_properties_row.string(1,key);data_properties_row.blob(2,bytes);data_properties_row.done();
        }
        if(doc.image_analysis_settings_) {const auto bytes=detail::encode_image_analysis_settings(*doc.image_analysis_settings_);image_settings_row.string(1,key);image_settings_row.blob(2,bytes);image_settings_row.done();}
        if(doc.image_properties_) {const auto bytes=detail::encode_object_properties(*doc.image_properties_);image_properties_row.string(1,key);image_properties_row.blob(2,bytes);image_properties_row.done();}
        if(const auto found=source_verifications_.find(id);found!=source_verifications_.end()) {
            if(e.info.embedded)throw std::invalid_argument("embedded image has source verification");
            const auto bytes=detail::encode_source_verification(*found->second);verification.string(1,key);verification.blob(2,bytes);verification.done();
        }
        if(const auto projection=doc.server()->raw_view()) {if((e.info.reader!="tiff" && e.info.reader!="ome-zarr") || e.info.embedded) throw std::invalid_argument("Raw view does not match project reader");const auto encoded=e.info.reader=="ome-zarr"?detail::encode_zarr_link(*zarr_link_of(doc.server())):tiff_view_encode(*projection);const std::vector<std::uint8_t> bytes(encoded.begin(),encoded.end());tiff_view.string(1,key);tiff_view.blob(2,bytes);tiff_view.done();}
        if(doc.tma_index_) {tma_grid.string(1,key);const auto bytes=TmaCodec::encode(*doc.tma_index_->info);tma_grid.blob(2,bytes);tma_grid.done();}
        auto saved_view=views_.find(id);
        if(saved_view!=views_.end()) {
            const auto& v=saved_view->second; validate_view(v,m);
            view.string(1,key); view.real(2,v.center.x); view.real(3,v.center.y); view.real(4,v.zoom);
            view.integer(5,static_cast<int>(v.display.mode)); view.real(6,v.display.black); view.real(7,v.display.white); view.real(8,v.display.gamma); view.done();
        }
        for(std::size_t i=0;i<m.levels.size();++i) {
            level.string(1,key); level.integer(2,static_cast<std::int64_t>(i)); level.integer(3,m.levels[i].width); level.integer(4,m.levels[i].height); level.real(5,m.levels[i].downsample); level.done();
        }
        for(const auto& [name,c]:doc.classes_) {
            classification.string(1,key);classification.string(2,name);classification.integer(3,c.color);classification.done();
            if(!c.parent.empty() || !c.color_present || c.alpha) {
                class_semantics.string(1,key);class_semantics.string(2,name);class_semantics.string(3,c.parent);
                if(const auto argb=c.color_argb())class_semantics.integer(4,*argb);else class_semantics.null(4);class_semantics.done();
            }
        }
        auto write_object=[&](const Annotation& a) {
            if(cancelled && cancelled->load()) throw std::runtime_error("recovery cancelled");
            object.string(1,key); object.string(2,std::to_string(a.id)); object.string(3,a.name); object.string(4,std::to_string(a.parent_id));
            object.string(5,a.class_name); object.string(6,std::to_string(a.revision)); object.integer(7,static_cast<int>(a.kind));
            auto geometry=encode(a.geometry); object.blob(8,geometry);
            auto nucleus=a.nucleus?encode(*a.nucleus):std::vector<std::uint8_t>{}; if(a.nucleus) object.blob(9,nucleus);
            object.done();
            if(a.plane!=ObjectPlane{}) {plane_row.string(1,key);plane_row.string(2,std::to_string(a.id));plane_row.integer(3,a.plane.c);plane_row.integer(4,a.plane.z);plane_row.integer(5,a.plane.t);plane_row.done();}
            if(a.properties) {const auto bytes=detail::encode_object_properties(*a.properties);properties_row.string(1,key);properties_row.string(2,std::to_string(a.id));properties_row.blob(3,bytes);properties_row.done();}
            if(a.tma_core) {const auto& t=*a.tma_core;tma_core.string(1,key);tma_core.string(2,std::to_string(a.id));tma_core.integer(3,static_cast<std::int64_t>(t.row()));tma_core.integer(4,static_cast<std::int64_t>(t.column()));tma_core.integer(5,t.missing()?1:0);tma_core.string(6,t.case_id());tma_core.done();}
            if(a.cell_feature_link) {cell_link.string(1,key);cell_link.string(2,std::to_string(a.id));cell_link.string(3,std::to_string(a.cell_feature_link->record_id));cell_link.integer(4,a.cell_feature_link->label);cell_link.done();}
        };
        for(const auto& [object_id,a]:doc.objects_) write_object(a);
        for(const auto& block:doc.object_snapshot_) for(const auto& a:block->values) write_object(a);
    }
    for(const auto& image_entry:entries_) {
      const auto image_id=image_entry.first;
      const auto& e=image_entry.second;
      auto write_analysis=[&](const AnalysisRecord& record) {
        write_provenance(image_id,record.id,record.result.provenance);
        if(cancelled && cancelled->load()) throw std::runtime_error("project save cancelled");
        const auto& r=record.result;
        analysis.string(1,std::to_string(image_id));analysis.string(2,std::to_string(record.id));analysis.string(3,std::to_string(r.object_id));
        analysis.string(4,std::to_string(r.object_revision));analysis.string(5,std::to_string(r.document_revision));analysis.string(6,r.algorithm);analysis.string(7,r.input_sha256);
        analysis.integer(8,static_cast<int>(r.kind));analysis.integer(9,r.nucleus?1:0);analysis.integer(10,static_cast<std::int64_t>(record.tile_size));
        analysis.string(11,std::to_string(r.samples));analysis.string(12,std::to_string(r.tiles));analysis.real(13,r.elapsed_ms);
        for(int c=0;c<3;++c) {analysis.real(14+c,r.means[c]);analysis.real(17+c,r.white[c]);}
        for(int i=0;i<3;++i) for(int c=0;c<3;++c) analysis.real(20+i*3+c,r.stains[i][c]);
        const auto geometry=encode(record.roi);analysis.blob(29,geometry);analysis.done();
        if(r.intensity_statistics) {
            const auto bytes=detail::encode_region_statistics(r);analysis_statistics.string(1,std::to_string(image_id));
            analysis_statistics.string(2,std::to_string(record.id));analysis_statistics.blob(3,bytes);analysis_statistics.done();
        }
      };
      for(const auto& [record_id,stored]:e.document->analysis_) write_analysis(*stored.value);
      for(const auto& block:e.document->analysis_snapshot_) for(const auto& record:block->records) if(record) write_analysis(*record);
      auto write_tissue=[&](const TissueRecord& record) {
        write_provenance(image_id,record.id,record.result->provenance());
        if(cancelled && cancelled->load()) throw std::runtime_error("project save cancelled");
        tissue.string(1,std::to_string(image_id));tissue.string(2,std::to_string(record.id));tissue.string(3,record.producer);
        const auto geometry=encode(record.result->roi());const auto mask=TissueArchiveCodec::encode(*record.result,cancelled);
        tissue.blob(4,geometry);tissue.blob(5,mask);tissue.done();
      };
      for(const auto& [record_id,stored]:e.document->tissue_) write_tissue(*stored.value);
      for(const auto& block:e.document->tissue_snapshot_) for(const auto& record:block->records) if(record) write_tissue(*record);
      auto write_cells=[&](const CellRecord& record) {
        write_provenance(image_id,record.id,record.result->nuclei()->provenance());
        if(cancelled && cancelled->load()) throw std::runtime_error("cell archive save cancelled");
        cells.string(1,std::to_string(image_id));cells.string(2,std::to_string(record.id));cells.string(3,record.producer);
        auto geometry=encode(record.result->nuclei()->roi());auto run=CellArchiveCodec::encode(*record.result,cancelled);
        cells.blob(4,geometry);cells.blob(5,run);cells.done();
      };
      for(const auto& entry:e.document->cells_) write_cells(*entry.second.value);
      for(const auto& record:e.document->cells_snapshot_) write_cells(*record);
      auto write_classifier=[&](const ObjectClassifierRecord& record) {
        const auto bytes=ObjectClassifierArchiveCodec::encode(record,cancelled);
        classifiers.string(1,std::to_string(image_id));classifiers.string(2,std::to_string(record.id));classifiers.blob(3,bytes);classifiers.done();
      };
      for(const auto& entry:e.document->classifiers_) write_classifier(*entry.second);
      for(const auto& record:e.document->classifiers_snapshot_) write_classifier(*record);
      auto write_pixel=[&](const PixelClassifierRecord& record) {
        const auto bytes=PixelClassifierArchiveCodec::encode(record,cancelled);
        pixels.string(1,std::to_string(image_id));pixels.string(2,std::to_string(record.id));pixels.blob(3,bytes);pixels.done();
      };
      for(const auto& entry:e.document->pixels_) write_pixel(*entry.second);
      for(const auto& record:e.document->pixels_snapshot_) write_pixel(*record);
      auto write_raw_pixel=[&](const RawPixelRecord& record) {const auto bytes=RawPixelArchiveCodec::encode_record(record,cancelled);pixels.string(1,std::to_string(image_id));pixels.string(2,std::to_string(record.id));pixels.blob(3,bytes);pixels.done();};
      for(const auto& entry:e.document->raw_pixels_) write_raw_pixel(*entry.second);
      for(const auto& record:e.document->raw_pixels_snapshot_) write_raw_pixel(*record);

      auto write_stream=[&](const PixelStreamRecord& record) {const auto bytes=PixelStreamRecordCodec::encode(record,directory.u8string(),cancelled);streams.string(1,std::to_string(image_id));streams.string(2,std::to_string(record.id));streams.blob(3,bytes);streams.done();};
      for(const auto& entry:e.document->streams_) write_stream(*entry.second);
      for(const auto& record:e.document->streams_snapshot_) write_stream(*record);
      auto write_raw_stream=[&](const RawPixelStreamRecord& record) {const auto bytes=RawPixelStreamRecordCodec::encode(record,directory.u8string(),cancelled);streams.string(1,std::to_string(image_id));streams.string(2,std::to_string(record.id));streams.blob(3,bytes);streams.done();};
      for(const auto& entry:e.document->raw_streams_) write_raw_stream(*entry.second);
      for(const auto& record:e.document->raw_streams_snapshot_) write_raw_stream(*record);
      auto write_raw=[&](const RawAnalysisRecord& record) {
          const auto bytes=RawAnalysisCodec::encode(record,e.document->metadata(),cancelled);
          raw_records.string(1,std::to_string(image_id));raw_records.string(2,std::to_string(record.id));raw_records.blob(3,bytes);raw_records.done();
      };
      auto write_spatial=[&](const SpatialDistanceRecord& record) {
          const auto bytes=SpatialDistanceArchiveCodec::encode(record,cancelled);
          spatial_records.string(1,std::to_string(image_id));spatial_records.string(2,std::to_string(record.id));spatial_records.blob(3,bytes);spatial_records.done();
      };
      auto write_neighborhood=[&](const SpatialNeighborhoodRecord& record) {
          const auto bytes=SpatialNeighborhoodArchiveCodec::encode(record,cancelled);
          neighborhood_records.string(1,std::to_string(image_id));neighborhood_records.string(2,std::to_string(record.id));neighborhood_records.blob(3,bytes);neighborhood_records.done();
      };
      for(const auto& entry:e.document->neighborhood_records_) write_neighborhood(*entry.second.value);
      for(const auto& record:e.document->neighborhood_snapshot_) write_neighborhood(*record);
      auto write_density=[&](const SpatialDensityRecord& record) {
          const auto bytes=SpatialDensityArchiveCodec::encode(record,cancelled);
          density_records.string(1,std::to_string(image_id));density_records.string(2,std::to_string(record.id));density_records.blob(3,bytes);density_records.done();
      };
      for(const auto& entry:e.document->density_records_) write_density(*entry.second.value);
      for(const auto& record:e.document->density_snapshot_) write_density(*record);
      for(const auto& entry:e.document->spatial_records_) write_spatial(*entry.second.value);
      for(const auto& record:e.document->spatial_snapshot_) write_spatial(*record);
      for(const auto& entry:e.document->raw_analysis_) write_raw(*entry.second.value);
      for(const auto& block:e.document->raw_analysis_snapshot_) for(const auto& record:block->records) if(record) write_raw(*record);


    }
    // Canonical rows and recovery provenance must commit together.
    if(recovery) {
        db.exec("PRAGMA application_id=1364218456; CREATE TABLE recovery(identity TEXT NOT NULL,session TEXT NOT NULL,base_path TEXT NOT NULL,base_generation TEXT NOT NULL,created_ms INTEGER NOT NULL,image_count INTEGER NOT NULL,object_count INTEGER NOT NULL) STRICT;");
        Statement row(db,"INSERT INTO recovery VALUES(?,?,?,?,?,?,?)");
        row.string(1,recovery->identity); row.string(2,recovery->session); row.string(3,recovery->base_path); row.string(4,std::to_string(recovery->base_generation));
        row.integer(5,recovery->created_unix_ms); row.integer(6,static_cast<std::int64_t>(recovery->images)); row.integer(7,static_cast<std::int64_t>(recovery->objects)); row.done();
    }
    if(batch) detail::write_batch_checkpoint(db.db,*batch,cancelled);else db.exec("DROP TABLE IF EXISTS qpx_batch_checkpoint;");
    if(cancelled && cancelled->load()) throw std::runtime_error("project save cancelled");
    transaction.commit();
    path_.swap(destination); generation_=next_generation; saved_revision_=revision_; saved_revisions_.swap(revisions);
}
std::shared_ptr<Project> Project::open(const std::string& path) {
    return open_impl(path,false);
}
std::shared_ptr<Project> Project::open_without_journal_recovery(const std::string& path) {return open_impl(path,false,"",nullptr,false);}
std::shared_ptr<Project> Project::recover(const std::string& path,const std::string& base) { return open_impl(path,true,base); }
std::shared_ptr<Project> Project::open_impl(const std::string& path,bool recovery,const std::string& base_path,LoadProgress* progress,bool repair_journal) {
    auto stopped=[&] {if(progress && progress->cancellation()->load()) throw std::runtime_error("project opening cancelled");};
    stopped();
    auto source=absolute(path); Database db(source,false,progress?progress->cancellation():nullptr,repair_journal);
    db.cancellation(progress?progress->cancellation():nullptr); Transaction transaction(db,false);
    if(recovery_table(db)!=recovery) throw std::invalid_argument(recovery?"not a recovery snapshot":"use Project.recover for recovery snapshots");
    auto version=schema(db,recovery);
    std::optional<RecoveryInfo> recovery_info;
    if(recovery) recovery_info=recovery_header(db,source);
    std::string associated_base;
    if(!base_path.empty()) {
        if(!recovery_info || recovery_info->base_path.empty() || !recovery_info->base_generation)
            throw std::invalid_argument("only a saved-project recovery can be associated with a primary location");
        associated_base=absolute(base_path);
        const auto base=read_save_identity(associated_base);
        if(base.first!=recovery_info->identity) throw std::invalid_argument("recovery association requires the same primary project identity");
        // Keep the captured generation even if the relocated primary advanced.
        // A later save must still reject that stale baseline, rather than rebase.
    }
    stopped();
    if(progress) {
        progress->total_images.store(static_cast<std::size_t>(scalar(db,"SELECT count(*) FROM images")));
        progress->total_objects.store(static_cast<std::size_t>(scalar(db,"SELECT count(*) FROM objects")));
        progress->phase.store(2);
    }
    auto project=std::make_shared<Project>();
    Statement meta(db,"SELECT identity,next_image,active,generation FROM project");
    if(!meta.row()) throw std::invalid_argument("project metadata is missing");
    project->identity_=meta.string(0); project->next_id_=number(meta.string(1)); project->active_image_=number(meta.string(2));
    auto generation=meta.integer(3);
    if(project->identity_.size()!=32 || project->identity_.find_first_not_of("0123456789abcdef")!=std::string::npos || !project->next_id_ || generation<1 || meta.row())
        throw std::invalid_argument("invalid project metadata");
    project->generation_=static_cast<std::uint64_t>(generation);
    if(version>=25) {
        Statement row(db,"SELECT metadata_key,metadata_value FROM project_metadata ORDER BY metadata_key");
        std::size_t bytes=sizeof(project->metadata_);
        while(row.row()) {
            if(project->metadata_.size()>=4096)throw std::invalid_argument("persisted project metadata exceeds 4096 entries");
            auto name=row.string(0),value=row.string(1);bytes+=validate_string_metadata_item(name,value);
            if(bytes>1024*1024)throw std::invalid_argument("persisted project metadata exceeds 1MiB payload");
            if(!project->metadata_.emplace(std::move(name),std::move(value)).second)throw std::invalid_argument("duplicate persisted project metadata key");
        }
    }
    if(version>=26) {
        std::vector<ObjectClass> definitions;Statement row(db,"SELECT name,parent_name,color_argb FROM project_classes ORDER BY name");std::size_t bytes=0;
        while(row.row()) {
            stopped();if(definitions.size()>=65536)throw std::length_error("persisted project class count exceeds limit");
            std::optional<std::uint32_t> argb;
            if(row.type(2)!=SQLITE_NULL) {const auto value=row.integer(2);if(value<0 || std::uint64_t(value)>0xffffffffULL)throw std::invalid_argument("invalid persisted class ARGB");argb=static_cast<std::uint32_t>(value);}
            auto value=ObjectClass::definition(row.string(0),argb,row.string(1));bytes+=sizeof(ObjectClass)+128+2*value.name.size()+value.parent.size();
            if(bytes>128ull*1024*1024)throw std::length_error("persisted project class payload exceeds limit");definitions.push_back(std::move(value));
        }
        project->class_registry_=detail::normalized_class_registry(definitions);
        Statement defaults(db,"SELECT position,class_name FROM project_class_defaults ORDER BY position");
        while(defaults.row()) {
            stopped();if(project->class_defaults_.size()>=65536 || defaults.integer(0)!=static_cast<std::int64_t>(project->class_defaults_.size()))throw std::invalid_argument("invalid project class default sequence");
            auto name=defaults.string(1);bytes+=sizeof(std::string)+name.size();if(bytes>128ull*1024*1024)throw std::length_error("persisted project class payload exceeds limit");project->class_defaults_.push_back(std::move(name));
        }
        validate_project_classes(project->class_registry_,project->class_defaults_);
    }
    Statement images(db,"SELECT id,name,reader,source,embedded,width,height,vendor,server_x,server_y,cal_x,cal_y,next_object,revision,rgb FROM images");
    Statement levels(db,"SELECT ordinal,width,height,downsample FROM levels WHERE image_id=? ORDER BY ordinal");
    Statement classes(db,"SELECT name,color FROM classes WHERE image_id=? ORDER BY name");
    Statement objects(db,version>=6?"SELECT o.id,o.name,o.parent,o.class_name,o.revision,o.kind,o.geometry,o.nucleus,l.record_id,l.label FROM objects o LEFT JOIN cell_feature_links l ON l.image_id=o.image_id AND l.object_id=o.id WHERE o.image_id=?":"SELECT id,name,parent,class_name,revision,kind,geometry,nucleus FROM objects WHERE image_id=?");
    while(images.row()) {
        stopped(); if(progress) progress->phase.store(2);
        auto key=images.string(0); ProjectImage info;
        info.id=number(key); info.name=images.string(1); info.reader=images.string(2); auto file=images.string(3);
        auto embedded=images.integer(4); if(embedded!=0 && embedded!=1) throw std::invalid_argument("invalid embedded-image flag");
        info.embedded=embedded==1; info.width=images.integer(5); info.height=images.integer(6); text(info.name);
        if(!info.id || info.id>=project->next_id_) throw std::invalid_argument("invalid project image ID");
        ImageMetadata metadata; metadata.width=info.width; metadata.height=info.height; metadata.vendor=images.string(7);
        metadata.pixel_width_um=images.optional(8); metadata.pixel_height_um=images.optional(9);
        levels.string(1,key); std::int64_t ordinal=0;
        while(levels.row()) { if(levels.integer(0)!=ordinal++ || ordinal>65536) throw std::invalid_argument("invalid pyramid level order"); metadata.levels.push_back({levels.integer(1),levels.integer(2),levels.real(3)}); }
        levels.check(sqlite3_reset(levels.stmt)); levels.check(sqlite3_clear_bindings(levels.stmt)); validate_metadata(metadata);
        std::shared_ptr<ImageServer> server;
        if(info.embedded) {
            auto bytes=sqlite3_column_bytes(images.stmt,14);
            if(info.reader!="embedded" || !file.empty() || images.type(14)!=SQLITE_BLOB || metadata.levels.size()!=1 || info.width>std::numeric_limits<int>::max() || info.height>std::numeric_limits<int>::max() ||
                info.width>256*1024*1024/3 || info.height>(256*1024*1024)/(3*info.width) || bytes!=info.width*info.height*3)
                throw std::invalid_argument("invalid embedded image payload");
            auto* p=static_cast<const std::uint8_t*>(sqlite3_column_blob(images.stmt,14));
            server=dense_image_server(Image(static_cast<int>(info.width),static_cast<int>(info.height),{p,p+bytes}));
            if(metadata.vendor!=server->metadata().vendor || metadata.pixel_width_um!=server->metadata().pixel_width_um || metadata.pixel_height_um!=server->metadata().pixel_height_um)
                throw std::invalid_argument("embedded source metadata does not match dense pixels");
        } else {
            if(file.empty() || images.type(14)!=SQLITE_NULL || reader_name(file,info.reader)!=info.reader) throw std::invalid_argument("invalid linked image descriptor");
            text(file,32768); auto location=fs::u8path(file); if(location.is_relative()) location=fs::u8path(source).parent_path()/location;
            std::shared_ptr<const TiffView> selected;std::shared_ptr<const detail::ZarrLink> zarr;
            if(version>=12) {Statement row(db,"SELECT payload FROM tiff_views WHERE image_id=?");row.string(1,key);if(row.row()) {const auto n=sqlite3_column_bytes(row.stmt,0);if(row.type(0)!=SQLITE_BLOB || n<0 || n>4096 || (info.reader!="tiff" && info.reader!="ome-zarr")) throw std::invalid_argument("invalid project TIFF view");const auto payload=std::string(static_cast<const char*>(sqlite3_column_blob(row.stmt,0)),std::size_t(n));if(info.reader=="ome-zarr"){zarr=detail::decode_zarr_link(payload);selected=std::make_shared<const TiffView>(zarr->options.view);}else selected=std::make_shared<const TiffView>(tiff_view_decode(payload));if(row.row()) throw std::invalid_argument("duplicate project TIFF view");}}
            info.source_path=absolute(location.u8string()); server=std::make_shared<LinkedServer>(metadata,info.source_path,info.reader,std::move(selected),nullptr,nullptr,std::move(zarr));
        }
        std::vector<ObjectClass> definitions;
        classes.string(1,key);
        while(classes.row()) { auto color=classes.integer(1); if(color<0 || color>0xffffff) throw std::invalid_argument("invalid persisted class color"); definitions.push_back({classes.string(0),static_cast<std::uint32_t>(color)}); }
        classes.check(sqlite3_reset(classes.stmt)); classes.check(sqlite3_clear_bindings(classes.stmt));
        if(version>=26) {
            std::map<std::string,std::size_t> at;for(std::size_t index=0;index<definitions.size();++index)if(!at.emplace(definitions[index].name,index).second)throw std::invalid_argument("duplicate persisted class");
            std::set<std::string> seen;Statement row(db,"SELECT class_name,parent_name,color_argb FROM class_semantics WHERE image_id=?");row.string(1,key);
            while(row.row()) {
                stopped();const auto name=row.string(0);const auto found=at.find(name);
                if(found==at.end() || !seen.insert(name).second)throw std::invalid_argument("orphaned or duplicate class semantics");
                auto& value=definitions[found->second];value.parent=row.string(1);value.color_present=row.type(2)!=SQLITE_NULL;
                if(value.color_present) {
                    const auto argb=row.integer(2);if(argb<0 || std::uint64_t(argb)>0xffffffffULL || (std::uint32_t(argb)&0xffffff)!=value.color)throw std::invalid_argument("inconsistent persisted class ARGB/RGB");value.alpha=std::uint8_t(std::uint32_t(argb)>>24);
                } else value.alpha=0;
            }
        }
        std::shared_ptr<const TmaGridInfo> grid;
        std::map<std::uint64_t,std::shared_ptr<const TmaCoreInfo>> core_metadata;
        if(version>=11) {
            Statement row(db,"SELECT payload FROM tma_grids WHERE image_id=?");row.string(1,key);
            if(row.row()) {const auto n=sqlite3_column_bytes(row.stmt,0);if(row.type(0)!=SQLITE_BLOB || n<0 || n>256*1024) throw std::invalid_argument("invalid TMA grid payload");grid=TmaCodec::decode(sqlite3_column_blob(row.stmt,0),static_cast<std::size_t>(n));if(row.row()) throw std::invalid_argument("duplicate TMA grid");}
            Statement cores(db,"SELECT object_id,row_index,column_index,missing,case_id FROM tma_cores WHERE image_id=?");cores.string(1,key);
            while(cores.row()) {stopped();const auto r=cores.integer(1),c=cores.integer(2),missing=cores.integer(3);if(!grid || r<0 || c<0 || (missing!=0 && missing!=1) || core_metadata.size()>=10000) throw std::invalid_argument("invalid TMA core metadata");
                const auto id=number(cores.string(0));if(!id || !core_metadata.emplace(id,TmaCodec::core(grid,static_cast<std::size_t>(r),static_cast<std::size_t>(c),missing!=0,cores.string(4))).second) throw std::invalid_argument("duplicate TMA core metadata");}
            if(grid && core_metadata.empty()) throw std::invalid_argument("TMA grid has no core objects");
        }
        std::map<std::uint64_t,std::shared_ptr<const ObjectProperties>> property_values;
        if(version>=16) {
            Statement rows(db,"SELECT object_id,payload FROM object_properties WHERE image_id=?");rows.string(1,key);
            while(rows.row()) {stopped();const auto n=sqlite3_column_bytes(rows.stmt,1);const auto id=number(rows.string(0));
                if(!id || rows.type(1)!=SQLITE_BLOB || n<0 || n>2*1024*1024 || !property_values.emplace(id,detail::decode_object_properties(sqlite3_column_blob(rows.stmt,1),std::size_t(n),version>=17)).second)throw std::invalid_argument("invalid project object properties");}
        }
        std::map<std::uint64_t,ObjectPlane> object_planes;
        if(version>=18) {
            Statement rows(db,"SELECT object_id,c,z,t FROM object_planes WHERE image_id=?");rows.string(1,key);
            while(rows.row()) {
                stopped();const auto id=number(rows.string(0));const auto c=rows.integer(1),z=rows.integer(2),t=rows.integer(3);
                if(!id || c < -1 || c>=65536 || z<0 || z>=65536 || t<0 || t>=65536 || !object_planes.emplace(id,ObjectPlane{std::int32_t(c),std::uint32_t(z),std::uint32_t(t)}).second)
                    throw std::invalid_argument("invalid project object plane");
            }
        }
        std::vector<Annotation> annotations;
        objects.string(1,key);
        while(objects.row()) {
            stopped();
            auto kind=objects.integer(5); if(kind<0 || kind>2) throw std::invalid_argument("unknown persisted object kind");
            auto geometry=decode(objects,6); std::optional<Roi> nucleus; if(objects.type(7)!=SQLITE_NULL) nucleus=decode(objects,7);
            annotations.push_back({number(objects.string(0)),objects.string(1),std::move(geometry),number(objects.string(2)),objects.string(3),number(objects.string(4)),static_cast<ObjectKind>(kind),std::move(nucleus)});
            if(version>=6 && (objects.type(8)!=SQLITE_NULL || objects.type(9)!=SQLITE_NULL)) {
                const auto label=objects.integer(9);if(label<1 || label>1000000) throw std::invalid_argument("invalid cell feature label");
                annotations.back().cell_feature_link=CellFeatureLink{number(objects.string(8)),static_cast<std::uint32_t>(label)};
            }
            if(const auto it=object_planes.find(annotations.back().id);it!=object_planes.end()) {annotations.back().plane=it->second;object_planes.erase(it);}
            if(const auto tma=core_metadata.find(annotations.back().id);tma!=core_metadata.end()) {annotations.back().tma_core=tma->second;core_metadata.erase(tma);}
            if(const auto it=property_values.find(annotations.back().id);it!=property_values.end()) {annotations.back().properties=it->second;property_values.erase(it);}
            if(progress) progress->objects.fetch_add(1,std::memory_order_relaxed);
        }
        objects.check(sqlite3_reset(objects.stmt)); objects.check(sqlite3_clear_bindings(objects.stmt));
        if(!object_planes.empty())throw std::invalid_argument("orphaned object planes");
        if(!property_values.empty())throw std::invalid_argument("orphaned object properties");
        if(!core_metadata.empty()) throw std::invalid_argument("orphaned TMA core metadata");
        stopped(); if(progress) progress->phase.store(3);
        std::shared_ptr<const ObjectProperties> image_properties;
        if(version>=19) {
            Statement row(db,"SELECT payload FROM image_properties WHERE image_id=?");row.string(1,key);
            if(row.row()) {
                const auto n=sqlite3_column_bytes(row.stmt,0);
                if(row.type(0)!=SQLITE_BLOB || n<0 || n>2*1024*1024)throw std::invalid_argument("invalid image properties payload");
                image_properties=detail::decode_object_properties(sqlite3_column_blob(row.stmt,0),std::size_t(n));
                if(row.row())throw std::invalid_argument("duplicate image properties");
            }
        }
        std::optional<ImageAnalysisSettings> image_settings;
        if(version>=20) {
            Statement row(db,"SELECT payload FROM image_analysis_settings WHERE image_id=?");row.string(1,key);
            if(row.row()) {const auto n=sqlite3_column_bytes(row.stmt,0);if(row.type(0)!=SQLITE_BLOB || n<0 || n>32768)throw std::invalid_argument("invalid image analysis settings payload");image_settings=detail::decode_image_analysis_settings(sqlite3_column_blob(row.stmt,0),std::size_t(n));if(row.row())throw std::invalid_argument("duplicate image analysis settings");}
        }
        std::shared_ptr<const ImageDataProperties> data_properties;
        if(version>=21) {
            Statement row(db,"SELECT payload FROM image_data_properties WHERE image_id=?");row.string(1,key);
            if(row.row()) {const auto n=sqlite3_column_bytes(row.stmt,0);if(row.type(0)!=SQLITE_BLOB || n<0 || n>2*ImageDataProperties::max_payload)throw std::invalid_argument("invalid ImageData properties payload");data_properties=detail::decode_image_data_properties(sqlite3_column_blob(row.stmt,0),std::size_t(n));if(row.row())throw std::invalid_argument("duplicate ImageData properties");}
        }
        data_properties=detail::expression_properties_after_open(std::move(data_properties),fs::u8path(source).parent_path(),
            recovery_info && recovery_info->base_path.empty());
        if(version>=23) {
            Statement row(db,"SELECT metadata_key,metadata_value FROM image_metadata WHERE image_id=? ORDER BY metadata_key");row.string(1,key);
            std::size_t bytes=sizeof(info.metadata);
            while(row.row()) {
                if(info.metadata.size()>=4096)throw std::invalid_argument("persisted image metadata exceeds 4096 entries");
                auto name=row.string(0),value=row.string(1);bytes+=validate_string_metadata_item(name,value);
                if(bytes>1024*1024)throw std::invalid_argument("persisted image metadata exceeds 1MiB payload");
                if(!info.metadata.emplace(std::move(name),std::move(value)).second)throw std::invalid_argument("duplicate persisted image metadata key");
            }
        }
        if(version>=24) {
            Statement row(db,"SELECT description FROM image_details WHERE image_id=?");row.string(1,key);const bool present=row.row();
            if(present) {if(row.type(0)!=SQLITE_NULL) {info.description=row.string(0,1024*1024);detail::bounded_utf8(*info.description,1024*1024);}if(row.row())throw std::invalid_argument("duplicate persisted image details");}
            Statement tags(db,"SELECT position,tag FROM image_tags WHERE image_id=? ORDER BY position");tags.string(1,key);
            std::set<std::string> seen;std::size_t bytes=0;
            while(tags.row()) {
                if(!present || info.tags.size()>=4096 || tags.integer(0)!=static_cast<std::int64_t>(info.tags.size()))throw std::invalid_argument("invalid persisted image tag sequence");
                auto tag=tags.string(1);detail::bounded_utf8(tag,4096);bytes+=64+tag.size();
                if(bytes>1024*1024 || !seen.insert(tag).second)throw std::invalid_argument("invalid persisted image tag set");
                info.tags.push_back(std::move(tag));
            }
        }
        std::optional<ImageAxisCalibration> axis_calibration;
        if(version>=22) {
            Statement row(db,"SELECT payload FROM image_axis_calibration WHERE image_id=?");row.string(1,key);
            if(row.row()) {const auto n=sqlite3_column_bytes(row.stmt,0);if(row.type(0)!=SQLITE_BLOB || n<0 || n>85+8*ImageAxisCalibration::max_timepoints)throw std::invalid_argument("invalid image axis calibration payload");axis_calibration=detail::decode_image_axis_calibration(sqlite3_column_blob(row.stmt,0),std::size_t(n));if(row.row())throw std::invalid_argument("duplicate image axis calibration");}
        }
        auto document=Document::restore(std::move(server),annotations,definitions,images.optional(10),images.optional(11),number(images.string(12)),number(images.string(13)),std::move(image_properties),std::move(image_settings),std::move(data_properties),std::move(axis_calibration));
        project->saved_revisions_.emplace(info.id,document->revision());
        if(!project->entries_.emplace(info.id,Entry{std::move(info),std::move(document),true}).second) throw std::invalid_argument("duplicate project image ID");
        stopped(); if(progress) progress->images.fetch_add(1,std::memory_order_relaxed);
    }
    stopped(); if(progress) progress->phase.store(4);
    if(project->active_image_ && !project->entries_.count(project->active_image_)) throw std::invalid_argument("project active image does not exist");
    if(version>=2) {
        Statement views(db,"SELECT image_id,center_x,center_y,zoom,mode,black,white,gamma FROM views");
        while(views.row()) {
            auto id=number(views.string(0)); auto mode=views.integer(4);
            if(!project->entries_.count(id)) throw std::invalid_argument("orphaned project view");
            if(mode<0 || mode>4) throw std::invalid_argument("invalid persisted display mode");
            ProjectView v{{views.real(1),views.real(2)},views.real(3),{static_cast<DisplayMode>(mode),views.real(5),views.real(6),views.real(7)}};
            validate_view(v,project->entry(id).document->metadata());
            if(!project->views_.emplace(id,v).second) throw std::invalid_argument("duplicate persisted view");
        }
    }
    std::unique_ptr<Statement> provenance;
    if(version>=7) provenance=std::make_unique<Statement>(db,"SELECT payload FROM analysis_provenance WHERE image_id=? AND record_id=?");
    auto read_provenance=[&](std::uint64_t image,std::uint64_t record)->std::shared_ptr<const AnalysisProvenance> {
        if(!provenance) return {};auto& row=*provenance;row.string(1,std::to_string(image));row.string(2,std::to_string(record));std::shared_ptr<const AnalysisProvenance> result;
        if(row.row()) {const auto n=sqlite3_column_bytes(row.stmt,0);if(row.type(0)!=SQLITE_BLOB || n<0) throw std::invalid_argument("invalid provenance blob");result=ProvenanceCodec::decode(static_cast<const std::uint8_t*>(sqlite3_column_blob(row.stmt,0)),static_cast<std::size_t>(n));if(row.row()) throw std::invalid_argument("duplicate provenance");}
        if(result) {const auto& metadata=project->entry(image).document->metadata();if(result->source.width!=metadata.width || result->source.height!=metadata.height) throw std::invalid_argument("provenance source extent mismatch");}
        row.check(sqlite3_reset(row.stmt));row.check(sqlite3_clear_bindings(row.stmt));return result;
    };
    if(version>=3) {
        std::string query="SELECT image_id,id,object_id,object_revision,document_revision,algorithm,input_sha256,kind,nucleus,tile_size,samples,tiles,elapsed_ms,mean_r,mean_g,mean_b,white_r,white_g,white_b,s00,s01,s02,s10,s11,s12,s20,s21,s22,geometry";
        if(version>=27) query+=",(SELECT payload FROM analysis_statistics s WHERE s.image_id=analysis_records.image_id AND s.id=analysis_records.id)";
        query+=" FROM analysis_records ORDER BY image_id,id";Statement rows(db,query.c_str());
        while(rows.row()) {
            stopped();auto image_id=number(rows.string(0));auto image=project->entries_.find(image_id);
            if(image==project->entries_.end()) throw std::invalid_argument("orphaned analysis record");
            AnalysisRecord record;auto& r=record.result;
            record.id=number(rows.string(1));r.object_id=number(rows.string(2));r.object_revision=number(rows.string(3));r.document_revision=number(rows.string(4));
            r.algorithm=rows.string(5);r.input_sha256=rows.string(6);
            auto kind=rows.integer(7),nucleus=rows.integer(8),tile=rows.integer(9);
            if(kind<0 || kind>1 || nucleus<0 || nucleus>1 || tile<16 || tile>1024) throw std::invalid_argument("invalid persisted analysis parameters");
            r.kind=static_cast<AnalysisKind>(kind);r.nucleus=nucleus!=0;record.tile_size=static_cast<std::size_t>(tile);
            r.samples=number(rows.string(10));r.tiles=number(rows.string(11));r.elapsed_ms=rows.real(12);
            for(int c=0;c<3;++c) {r.means[c]=rows.real(13+c);r.white[c]=rows.real(16+c);}
            for(int i=0;i<3;++i) for(int c=0;c<3;++c) r.stains[i][c]=rows.real(19+i*3+c);
            record.roi=decode(rows,28);r.provenance=read_provenance(image_id,record.id);
            if(version>=27 && rows.type(29)!=SQLITE_NULL) {
                const auto size=sqlite3_column_bytes(rows.stmt,29);
                if(rows.type(29)!=SQLITE_BLOB || size<0) throw std::invalid_argument("invalid region statistics payload");
                r.intensity_statistics=detail::decode_region_statistics(r,static_cast<const std::uint8_t*>(sqlite3_column_blob(rows.stmt,29)),std::size_t(size));
            }
            image->second.document->restore_analysis(std::move(record));
        }
    }
    if(version>=4) {
        Statement rows(db,"SELECT image_id,id,producer,geometry,mask FROM tissue_records ORDER BY image_id,id");
        while(rows.row()) {
            stopped();const auto image=project->entries_.find(number(rows.string(0)));
            if(image==project->entries_.end()) throw std::invalid_argument("orphaned tissue record");
            auto record=std::make_shared<TissueRecord>();record->id=number(rows.string(1));record->producer=rows.string(2);
            if(image->second.document->analysis_.count(record->id) || image->second.document->tissue_.count(record->id)) throw std::invalid_argument("duplicate tissue record ID");
            auto roi=decode(rows,3);const auto size=sqlite3_column_bytes(rows.stmt,4);
            if(rows.type(4)!=SQLITE_BLOB || size<0) throw std::invalid_argument("invalid tissue mask blob");
            record->result=TissueArchiveCodec::decode(static_cast<const std::uint8_t*>(sqlite3_column_blob(rows.stmt,4)),static_cast<std::size_t>(size),std::move(roi),image->second.document->metadata(),progress?progress->cancellation():nullptr);
            ProvenanceCodec::attach(record->result,read_provenance(number(rows.string(0)),record->id));
            image->second.document->restore_tissue(std::move(record));
        }
    }
    if(version>=5) {
        Statement rows(db,"SELECT image_id,id,producer,geometry,run FROM cell_records ORDER BY image_id,id");
        while(rows.row()) {
            stopped();const auto image=project->entries_.find(number(rows.string(0)));
            if(image==project->entries_.end()) throw std::invalid_argument("orphaned cell record");
            auto record=std::make_shared<CellRecord>();record->id=number(rows.string(1));record->producer=rows.string(2);
            if(image->second.document->analysis_.count(record->id) || image->second.document->tissue_.count(record->id) || image->second.document->cells_.count(record->id)) throw std::invalid_argument("duplicate cell record ID");
            auto roi=decode(rows,3);const auto size=sqlite3_column_bytes(rows.stmt,4);
            if(rows.type(4)!=SQLITE_BLOB || size<0) throw std::invalid_argument("invalid cell archive blob");
            record->result=CellArchiveCodec::decode(static_cast<const std::uint8_t*>(sqlite3_column_blob(rows.stmt,4)),static_cast<std::size_t>(size),std::move(roi),image->second.document->metadata(),progress?progress->cancellation():nullptr);
            const auto p=read_provenance(number(rows.string(0)),record->id);ProvenanceCodec::attach(record->result->nuclei(),p);ProvenanceCodec::attach(record->result->cells(),p);
            image->second.document->restore_cells(std::move(record));
        }
    }
    if(version>=8) {
        Statement rows(db,"SELECT image_id,id,payload FROM classifier_records ORDER BY image_id,id");
        while(rows.row()) {
            stopped();const auto image=project->entries_.find(number(rows.string(0)));
            if(image==project->entries_.end()) throw std::invalid_argument("orphaned classifier record");
            const auto size=sqlite3_column_bytes(rows.stmt,2);
            if(rows.type(2)!=SQLITE_BLOB || size<0) throw std::invalid_argument("invalid classifier archive blob");
            const auto record=ObjectClassifierArchiveCodec::decode(static_cast<const std::uint8_t*>(sqlite3_column_blob(rows.stmt,2)),static_cast<std::size_t>(size),image->second.document->metadata(),progress?progress->cancellation():nullptr);
            if(record->id!=number(rows.string(1))) throw std::invalid_argument("classifier record identity mismatch");
            image->second.document->restore_classifier(record);
        }
    }
    if(version>=9) {
        Statement rows(db,"SELECT image_id,id,payload FROM pixel_classifier_records ORDER BY image_id,id");
        while(rows.row()) {
            stopped();const auto image=project->entries_.find(number(rows.string(0)));if(image==project->entries_.end()) throw std::invalid_argument("orphaned pixel classifier record");
            const auto size=sqlite3_column_bytes(rows.stmt,2);if(rows.type(2)!=SQLITE_BLOB || size<0) throw std::invalid_argument("invalid pixel classifier blob");
            const auto* payload=static_cast<const std::uint8_t*>(sqlite3_column_blob(rows.stmt,2));
            if(size>=4 && std::memcmp(payload,"QXR\1",4)==0) {
                const auto record=RawPixelArchiveCodec::decode_record(payload,std::size_t(size),image->second.document->metadata(),progress?progress->cancellation():nullptr);
                if(record->id!=number(rows.string(1))) throw std::invalid_argument("raw pixel record identity mismatch");image->second.document->restore_raw_pixel(record);continue;
            }
            const auto record=PixelClassifierArchiveCodec::record(static_cast<const std::uint8_t*>(sqlite3_column_blob(rows.stmt,2)),static_cast<std::size_t>(size),image->second.document->metadata(),progress?progress->cancellation():nullptr);
            if(record->id!=number(rows.string(1))) throw std::invalid_argument("pixel classifier record identity mismatch");image->second.document->restore_pixel_classifier(record);
        }
    }
    if(version>=10) {
        Statement rows(db,"SELECT image_id,id,payload FROM pixel_stream_records ORDER BY image_id,id");
        while(rows.row()) {
            stopped();const auto image=project->entries_.find(number(rows.string(0)));if(image==project->entries_.end()) throw std::invalid_argument("orphaned pixel stream record");
            const auto size=sqlite3_column_bytes(rows.stmt,2);if(rows.type(2)!=SQLITE_BLOB || size<0 || size>128*1024) throw std::invalid_argument("invalid pixel stream record blob");
            const auto* payload=static_cast<const std::uint8_t*>(sqlite3_column_blob(rows.stmt,2));
            if(size>=4 && std::memcmp(payload,"QWL\1",4)==0) {
                const auto record=RawPixelStreamRecordCodec::decode(payload,std::size_t(size),fs::u8path(source).parent_path().u8string(),progress?progress->cancellation():nullptr);
                if(record->id!=number(rows.string(1))) throw std::invalid_argument("raw stream record identity mismatch");image->second.document->restore_raw_pixel_stream(record);continue;
            }
            const auto record=PixelStreamRecordCodec::decode(static_cast<const std::uint8_t*>(sqlite3_column_blob(rows.stmt,2)),static_cast<std::size_t>(size),fs::u8path(source).parent_path().u8string(),progress?progress->cancellation():nullptr);
            if(record->id!=number(rows.string(1))) throw std::invalid_argument("pixel stream record identity mismatch");image->second.document->restore_pixel_stream(record);
        }
    }
    if(version>=13) {
        Statement rows(db,"SELECT image_id,id,payload FROM raw_analysis_records ORDER BY image_id,id");
        while(rows.row()) {
            stopped();const auto image=project->entries_.find(number(rows.string(0)));
            if(image==project->entries_.end() || image->second.info.embedded || (image->second.info.reader!="tiff" && image->second.info.reader!="ome-zarr")) throw std::invalid_argument("orphaned or incompatible raw analysis record");
            const auto size=sqlite3_column_bytes(rows.stmt,2);if(rows.type(2)!=SQLITE_BLOB || size<0 || size>16*1024*1024) throw std::invalid_argument("invalid raw analysis record blob");
            const auto record=RawAnalysisCodec::decode(static_cast<const std::uint8_t*>(sqlite3_column_blob(rows.stmt,2)),std::size_t(size),image->second.document->metadata(),progress?progress->cancellation():nullptr);
            if(record->id!=number(rows.string(1))) throw std::invalid_argument("raw analysis record identity mismatch");image->second.document->restore_raw_analysis(record);
        }
    }
    if(version>=14) {
        Statement rows(db,"SELECT image_id,id,payload FROM spatial_distance_records ORDER BY image_id,id");
        while(rows.row()) {
            stopped();const auto image=project->entries_.find(number(rows.string(0)));
            if(image==project->entries_.end()) throw std::invalid_argument("orphaned spatial distance record");
            const auto size=sqlite3_column_bytes(rows.stmt,2);
            if(rows.type(2)!=SQLITE_BLOB || size<0 || size>128*1024*1024) throw std::invalid_argument("invalid spatial distance blob");
            const auto record=SpatialDistanceArchiveCodec::decode(static_cast<const std::uint8_t*>(sqlite3_column_blob(rows.stmt,2)),std::size_t(size),progress?progress->cancellation():nullptr);
            if(record->id!=number(rows.string(1))) throw std::invalid_argument("spatial record identity mismatch");image->second.document->restore_spatial(record);
        }
    }
    if(version>=28) {
        Statement rows(db,"SELECT image_id,id,payload FROM spatial_neighborhood_records ORDER BY image_id,id");
        while(rows.row()) {
            stopped();const auto image=project->entries_.find(number(rows.string(0)));
            if(image==project->entries_.end()) throw std::invalid_argument("orphaned spatial neighborhood record");
            const auto size=sqlite3_column_bytes(rows.stmt,2);
            if(rows.type(2)!=SQLITE_BLOB || size<0 || size>128*1024*1024) throw std::invalid_argument("invalid spatial neighborhood blob");
            const auto record=SpatialNeighborhoodArchiveCodec::decode(static_cast<const std::uint8_t*>(sqlite3_column_blob(rows.stmt,2)),std::size_t(size),progress?progress->cancellation():nullptr);
            if(record->id!=number(rows.string(1))) throw std::invalid_argument("neighborhood record identity mismatch");image->second.document->restore_neighborhood(record);
        }
    }
    if(version>=29) {
        Statement rows(db,"SELECT image_id,id,payload FROM spatial_density_records ORDER BY image_id,id");
        while(rows.row()) {
            stopped();const auto image=project->entries_.find(number(rows.string(0)));
            if(image==project->entries_.end()) throw std::invalid_argument("orphaned spatial density record");
            const auto size=sqlite3_column_bytes(rows.stmt,2);
            if(rows.type(2)!=SQLITE_BLOB || size<0 || size>128*1024*1024) throw std::invalid_argument("invalid spatial density blob");
            const auto record=SpatialDensityArchiveCodec::decode(static_cast<const std::uint8_t*>(sqlite3_column_blob(rows.stmt,2)),std::size_t(size),progress?progress->cancellation():nullptr);
            if(record->id!=number(rows.string(1))) throw std::invalid_argument("density record identity mismatch");image->second.document->restore_density(record);
        }
    }
    if(version>=15) {
        Statement rows(db,"SELECT image_id,payload FROM source_verifications ORDER BY image_id");
        while(rows.row()) {
            stopped();const auto id=number(rows.string(0));const auto image=project->entries_.find(id);
            if(image==project->entries_.end() || image->second.info.embedded)throw std::invalid_argument("orphaned or embedded source verification");
            const auto n=sqlite3_column_bytes(rows.stmt,1);if(rows.type(1)!=SQLITE_BLOB || n<0 || n>2*1024*1024)throw std::invalid_argument("invalid source verification payload");
            const auto value=detail::decode_source_verification(static_cast<const std::uint8_t*>(sqlite3_column_blob(rows.stmt,1)),std::size_t(n));
            if(!project->source_verifications_.emplace(id,value).second)throw std::invalid_argument("duplicate source verification");
        }
    }
    if(version>=12 && scalar(db,"SELECT count(*) FROM tiff_views t LEFT JOIN images i ON i.id=t.image_id WHERE i.id IS NULL OR i.reader NOT IN ('tiff','ome-zarr') OR i.embedded!=0")) throw std::invalid_argument("orphaned or incompatible TIFF view data");
    if(version>=27 && scalar(db,"SELECT count(*) FROM analysis_statistics s WHERE NOT EXISTS (SELECT 1 FROM analysis_records a WHERE a.image_id=s.image_id AND a.id=s.id)")) throw std::invalid_argument("orphaned region statistics");
    if(version>=26 && scalar(db,"SELECT count(*) FROM class_semantics s WHERE NOT EXISTS (SELECT 1 FROM classes c WHERE c.image_id=s.image_id AND c.name=s.class_name)"))throw std::invalid_argument("orphaned class semantics");
    if(version>=24 && (scalar(db,"SELECT count(*) FROM image_details d WHERE NOT EXISTS (SELECT 1 FROM images i WHERE i.id=d.image_id)") || scalar(db,"SELECT count(*) FROM image_tags t WHERE NOT EXISTS (SELECT 1 FROM image_details d WHERE d.image_id=t.image_id)")))throw std::invalid_argument("orphaned image details or tags");
    if(version>=23 && scalar(db,"SELECT count(*) FROM image_metadata p WHERE NOT EXISTS (SELECT 1 FROM images i WHERE i.id=p.image_id)"))throw std::invalid_argument("orphaned image metadata");
    if(version>=22 && scalar(db,"SELECT count(*) FROM image_axis_calibration p WHERE NOT EXISTS (SELECT 1 FROM images i WHERE i.id=p.image_id)"))throw std::invalid_argument("orphaned image axis calibration");
    if(version>=21 && scalar(db,"SELECT count(*) FROM image_data_properties p WHERE NOT EXISTS (SELECT 1 FROM images i WHERE i.id=p.image_id)"))throw std::invalid_argument("orphaned ImageData properties");
    if(version>=20 && scalar(db,"SELECT count(*) FROM image_analysis_settings p WHERE NOT EXISTS (SELECT 1 FROM images i WHERE i.id=p.image_id)"))throw std::invalid_argument("orphaned image analysis settings");
    if(version>=19 && scalar(db,"SELECT count(*) FROM image_properties p WHERE NOT EXISTS (SELECT 1 FROM images i WHERE i.id=p.image_id)"))throw std::invalid_argument("orphaned image properties");
    if(version>=18 && scalar(db,"SELECT count(*) FROM object_planes p LEFT JOIN objects o ON o.image_id=p.image_id AND o.id=p.object_id WHERE o.id IS NULL"))throw std::invalid_argument("orphaned object plane data");
    if(version>=16 && scalar(db,"SELECT count(*) FROM object_properties p WHERE NOT EXISTS (SELECT 1 FROM objects o WHERE o.image_id=p.image_id AND o.id=p.object_id)"))throw std::invalid_argument("orphaned object properties");
    if(version>=11 && (scalar(db,"SELECT count(*) FROM tma_grids WHERE image_id NOT IN (SELECT id FROM images)") || scalar(db,"SELECT count(*) FROM tma_cores WHERE image_id NOT IN (SELECT id FROM images)"))) throw std::invalid_argument("orphaned TMA grid data");
    if(version>=7 && scalar(db,"SELECT count(*) FROM analysis_provenance p WHERE NOT EXISTS (SELECT 1 FROM analysis_records a WHERE a.image_id=p.image_id AND a.id=p.record_id) AND NOT EXISTS (SELECT 1 FROM tissue_records t WHERE t.image_id=p.image_id AND t.id=p.record_id) AND NOT EXISTS (SELECT 1 FROM cell_records c WHERE c.image_id=p.image_id AND c.id=p.record_id)")) throw std::invalid_argument("orphaned analysis provenance");
    if(version>=6) {
        if(scalar(db,"SELECT count(*) FROM cell_feature_links l WHERE NOT EXISTS (SELECT 1 FROM objects o WHERE o.image_id=l.image_id AND o.id=l.object_id)")) throw std::invalid_argument("orphaned cell feature link");
        for(const auto& [image_id,e]:project->entries_) for(const auto& [id,a]:e.document->objects_) if(a.cell_feature_link) {
            stopped();const auto& link=*a.cell_feature_link;const auto run=e.document->cells_.find(link.record_id);
            // Removed archives remain explicit historical links; present archives
            // must resolve to a real feature label in this same document.
            if(run!=e.document->cells_.end() && link.label>run->second.value->result->features().size()) throw std::invalid_argument("invalid cell feature label");
        }
    }
    if(scalar(db,"SELECT count(*) FROM objects WHERE image_id NOT IN (SELECT id FROM images)") ||
       scalar(db,"SELECT count(*) FROM classes WHERE image_id NOT IN (SELECT id FROM images)") ||
       scalar(db,"SELECT count(*) FROM levels WHERE image_id NOT IN (SELECT id FROM images)")) throw std::invalid_argument("orphaned project data");
    stopped(); transaction.commit();
    if(recovery_info) {
        project->path_=associated_base.empty()?recovery_info->base_path:associated_base; project->generation_=recovery_info->base_generation;
        project->recovery_source_=source;
        project->revision_=1; project->saved_revision_=0;
    } else project->path_.swap(source);
    stopped(); if(progress) progress->phase.store(5);
    return project;
}
void Project::save_recovery(const std::string& path,const std::string& link_directory,const RecoveryInfo& info,const std::atomic<bool>* cancelled) {
    save_impl(path,link_directory,cancelled,&info);
    Database db(path,false); db.cancellation(cancelled);
    Statement check(db,"PRAGMA quick_check"); if(!check.row() || check.string(0)!="ok") throw std::runtime_error("recovery snapshot integrity check failed");
    recovery_header(db,path);
}
std::optional<RecoveryInfo> Project::inspect_recovery_work(const std::string& path,const std::atomic<bool>* cancelled) {
    // Opening may recover a hot journal before classifying the committed state.
    Database db(path,false); db.cancellation(cancelled); Transaction transaction(db,false);
    {
        Statement mode(db,"PRAGMA journal_mode");
        if(!mode.row() || mode.string(0)!="delete") throw std::invalid_argument("unexpected recovery work journal mode");
    }
    if(scalar(db,"PRAGMA application_id")==0 && scalar(db,"PRAGMA user_version")==0 &&
       scalar(db,"SELECT count(*) FROM sqlite_schema WHERE name NOT LIKE 'sqlite_%'")==0) {
        transaction.commit(); return std::nullopt;
    }
    auto info=recovery_header(db,path);
    Statement check(db,"PRAGMA quick_check");
    if(!check.row() || check.string(0)!="ok") throw std::runtime_error("abandoned recovery integrity check failed");
    transaction.commit(); return info;
}
RecoveryInfo Project::inspect_recovery_header(const std::string& path,const std::atomic<bool>* cancelled) {
    auto source=absolute(path); Database db(source,false); db.cancellation(cancelled); Transaction transaction(db,false);
    auto info=recovery_header(db,source); transaction.commit();
    return info;
}
std::pair<std::string,std::uint64_t> Project::read_save_identity(const std::string& path) {
    Database base(absolute(path),false); Transaction transaction(base,false); schema(base);
    if(recovery_table(base)) throw std::invalid_argument("recovery cannot be a primary project");
    Statement meta(base,"SELECT identity,generation FROM project");
    if(!meta.row()) throw std::invalid_argument("primary project metadata is missing");
    auto identity=meta.string(0); const auto generation=meta.integer(1);
    if(identity.size()!=32 || identity.find_first_not_of("0123456789abcdef")!=std::string::npos || generation<1 || meta.row())
        throw std::invalid_argument("invalid primary project identity or generation");
    transaction.commit(); return {std::move(identity),static_cast<std::uint64_t>(generation)};
}
RecoveryInfo Project::inspect_recovery(const std::string& path) {
    auto info=inspect_recovery_header(path);
    if(info.base_path.empty()) info.base_status="unsaved";
    else {
        try {
            if(!fs::exists(fs::u8path(info.base_path))) info.base_status="missing";
            else {
                const auto base=read_save_identity(info.base_path);
                info.base_status=base.first==info.identity && base.second==info.base_generation?"matching":"changed";
            }
        } catch(const std::exception&) { info.base_status="unreadable"; }
    }
    return info;
}
#endif
}

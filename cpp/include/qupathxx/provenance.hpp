#pragma once
#include "qupathxx/image_server.hpp"
namespace qpx {
struct BuildEnvironment {
    std::string app_version,source_tree_sha256,git_revision,compiler,compiler_flags,configuration,platform,cmake_version;
    std::string geos_version,openslide_version,sqlite_version;
    bool gui=false,python=false,interchange=false;
    std::size_t payload_bytes() const;
    void validate() const;
};
struct AnalysisProvenance {
    std::uint64_t object_id=0,object_revision=0,document_revision=0,captured_unix_ms=0;
    bool nucleus=false;
    bool source_only=false; // Explicit whole-source capture; never a fabricated object ID.
    ImageMetadata source;
    std::string reader,locator;
    std::optional<double> effective_pixel_width_um,effective_pixel_height_um;
    std::shared_ptr<const BuildEnvironment> environment;
    std::shared_ptr<const ReaderSource> reader_source;
    void validate() const;
    std::size_t payload_bytes() const;
};
std::shared_ptr<const BuildEnvironment> build_environment();
std::shared_ptr<const AnalysisProvenance> capture_provenance(const ImageServer&,std::uint64_t,std::uint64_t,std::uint64_t,bool,std::optional<double>,std::optional<double>);
std::shared_ptr<const AnalysisProvenance> capture_source_provenance(const ImageServer&,std::uint64_t document_revision,std::optional<double>,std::optional<double>);
std::string provenance_json(const AnalysisProvenance&);
void save_provenance(const AnalysisProvenance&,const std::string& path);
}

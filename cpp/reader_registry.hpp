#pragma once
// Internal native selection authority. This header is intentionally not SDK surface.
#include "qupathxx/ome_zarr.hpp"
#include <string>
#include <vector>

namespace qpx::detail {
enum class ReaderPolicy { Project, RegionCli, RgbNominal, SpatialImport, QuPathImport };
enum class ReaderAdapter { FileObserved, MemoryRgb };
struct ReaderFeatures {
    bool openslide=false,tiff=false,ome_zarr=false;
};
struct ReaderDescriptor {
    std::string provider_id,display_name;
    bool available=false;
    std::string unavailable_reason;
    // Declared upper bounds, not source-specific capabilities or analysis admission.
    bool display_rgba=true,raw_samples=false;
    std::string version,contract,module_sha256;
};
struct ReaderSelection {
    std::string provider_id;
    ReaderPolicy policy=ReaderPolicy::Project;
    ReaderAdapter adapter=ReaderAdapter::FileObserved;
};
struct ReaderOpenOptions {
    TiffOpenOptions tiff;
    OmeZarrOpenOptions zarr;
};
struct ReaderRoute {
    ReaderSelection selection;
    bool raw_route=false;
};

ReaderFeatures reader_features();
bool known_reader_id(const std::string&) noexcept;
bool valid_reader_request(const std::string&) noexcept;
std::vector<ReaderDescriptor> reader_catalog(ReaderFeatures features=reader_features());
std::string reader_catalog_json(ReaderFeatures features=reader_features());
// Pure path-text/build-feature selection: never probes or checks source availability.
ReaderSelection select_reader(const std::string& path,const std::string& requested,
    ReaderPolicy policy=ReaderPolicy::Project,ReaderFeatures features=reader_features());
ReaderRoute desktop_add_selection(const std::string& path,const std::string& requested,
    ReaderFeatures features=reader_features());
ReaderRoute desktop_startup_selection(const std::string& path,const std::string& requested,
    ReaderFeatures features=reader_features());
std::shared_ptr<ImageServer> open_reader(const std::string& path,const ReaderSelection&,
    const ReaderOpenOptions& options={},const std::atomic<bool>* cancelled=nullptr);
}

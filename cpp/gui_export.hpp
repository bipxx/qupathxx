#pragma once
#include "export_output.hpp"
#include "workflow_file_version.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/cell_tables.hpp"
#include "qupathxx/measurement_tables.hpp"
#include <tuple>
namespace qpx::detail {
enum class GuiExportOperation {GeoJson,ObjectCsv,SavedCells,ImageCells,ProjectCells,ImageMeasurements,ProjectMeasurements};
struct ExportOwnerStamp {
    const Project* project=nullptr;const Document* document=nullptr;const ImageServer* server=nullptr;
    std::uint64_t image=0,revision=0,calibration=0,selection=0,project_generation=0,project_revision=0;
    std::string source_generation;
    std::vector<std::tuple<std::uint64_t,const Document*,std::uint64_t,std::uint64_t,const ImageServer*>> members;
    auto values() const {return std::tie(project,document,server,image,revision,calibration,selection,project_generation,project_revision,source_generation,members);}
    bool operator==(const ExportOwnerStamp& other) const {return values()==other.values();}
};
struct PendingGuiExport {
    GuiExportOperation operation=GuiExportOperation::GeoJson;std::string path,scope;
    std::vector<std::uint64_t> ids;std::vector<std::string> protected_paths;
    CellTableOptions cell_options;MeasurementTableOptions measurement_options;
    MeasurementTableKind measurement_kind=MeasurementTableKind::Roi;
    bool selected_only=false,allow_losses=false,nested=false,intensity=false;
    ExportOwnerStamp owner;std::shared_ptr<const FileVersion> destination;
    bool project_scope() const {return operation==GuiExportOperation::ProjectCells || operation==GuiExportOperation::ProjectMeasurements;}
};
inline bool same_export_destination(const FileVersion& a,const FileVersion& b) {
    return std::tie(a.path,a.identity_scheme,a.identity,a.stamp_scheme,a.bytes,a.modified_token,a.change_token)==std::tie(b.path,b.identity_scheme,b.identity,b.stamp_scheme,b.bytes,b.modified_token,b.change_token);
}
inline std::vector<std::string> export_protected_paths(const Project* project,const Document& document,const std::string& layout,const std::vector<std::string>& recovery) {
    std::vector<std::string> paths;
    auto database=[&](const std::string& p){if(!p.empty())for(const char* suffix:{"","-journal","-wal","-shm"})paths.push_back(p+suffix);};
    if(project) {database(project->path());database(project->recovery_source());for(const auto& image:project->images())if(!image.embedded && !image.source_path.empty())paths.push_back(image.source_path);}
    for(const auto& p:recovery)database(p);
    if(const auto server=document.server()) {
        if(!server->source_locator().empty())paths.push_back(server->source_locator());
        if(const auto source=server->reader_source()) {if(!source->locator.empty())paths.push_back(source->locator);if(source->primary)paths.push_back(source->primary->path);for(const auto& file:source->companions)paths.push_back(file.path);}
    }
    if(!layout.empty())paths.push_back(layout);
    std::sort(paths.begin(),paths.end());paths.erase(std::unique(paths.begin(),paths.end()),paths.end());return paths;
}
}

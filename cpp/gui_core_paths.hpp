#pragma once
#include "gui_path_field.hpp"
#include "gui_export.hpp"
namespace qpx::detail {
// Independent destinations; shared scripted options deliberately seed both splits.
struct GuiCorePaths {
    ui::PathField autosave{{"##Autosave path",ui::PathRole::NewFile,{".qpxrecover"},"","Automatic destination"}};
    ui::PathField recovery{{"##Restore recovery",ui::PathRole::OpenFile,{".qpxrecover"},"","Recovery file to open"}};
    ui::PathField recovery_primary{{"##Recovery primary",ui::PathRole::NewFile,{".qpxproj"},"","Use recorded location when empty"}};
    ui::PathField import_source{{"##Import source",ui::PathRole::OpenFile,{".geojson",".json"},"","GeoJSON source file"}};
    ui::PathField geojson{{"##Interchange path",ui::PathRole::NewFile,{".geojson",".json"},"",".geojson destination"}};
    ui::PathField object_csv{{"##Object CSV path",ui::PathRole::NewFile,{".csv"},"",".csv destination"}};
    ui::PathField cells{{"##Cell table path",ui::PathRole::NewFile,{".csv"},"","results.csv",false,false,true}};
    ui::PathField roi{{"##Measurement destination",ui::PathRole::NewFile,{".csv"},"","results.csv",false,false,true}};
    ui::PathField tissue{{"##Tissue table destination",ui::PathRole::NewFile,{".csv"},"","results.csv",false,false,true}};
    ui::PathField stain_load{{"Model file",ui::PathRole::OpenFile,{".json"},"","Stain model JSON"}};
    ui::PathField stain_save{{"Export model file",ui::PathRole::NewFile,{".json"},"","Stain model JSON"}};
    ui::PathField fingerprint_input{{"##Fingerprint input",ui::PathRole::OpenFile,{},"","File to hash"}};
    ui::PathField fingerprint_output{{"##Fingerprint output",ui::PathRole::NewFile,{".json"},"","Optional .json destination"}};
    explicit GuiCorePaths(const GuiOptions& options) {
        autosave.set_option(options.autosave_path);recovery.set_option(options.recovery_path);recovery_primary.set_option(options.recovery_base_path);
        for(auto* path:{&import_source,&geojson,&object_csv})path->set_option(options.interchange_path);
        cells.set_option(options.cell_table_path);roi.set_option(options.measurement_table_path);tissue.set_option(options.measurement_table_path);
        stain_load.set_option(options.stain_model_path);stain_save.set_option(options.stain_model_path);
        fingerprint_input.set_option(options.file_fingerprint_path);fingerprint_output.set_option(options.file_fingerprint_output);
    }
    std::vector<ui::PathField*> paths(){return {&autosave,&recovery,&recovery_primary,&import_source,&geojson,&object_csv,&cells,&roi,&tissue,&stain_load,&stain_save,&fingerprint_input,&fingerprint_output};}
    ui::PathField& output(GuiExportOperation operation,int measurement_kind) {
        if(operation==GuiExportOperation::GeoJson)return geojson;
        if(operation==GuiExportOperation::ObjectCsv)return object_csv;
        if(operation==GuiExportOperation::ImageMeasurements || operation==GuiExportOperation::ProjectMeasurements)return measurement_kind==0?roi:tissue;
        return cells;
    }
};
inline std::string suggested_export_name(const std::string& project,const std::string& image,GuiExportOperation operation,int measurement_kind) {
    auto stem=std::filesystem::u8path(project.empty()?image:project).stem().u8string();if(stem.empty())stem="image";
    const char* kind=operation==GuiExportOperation::GeoJson?"annotations":operation==GuiExportOperation::ObjectCsv?"measurements":operation==GuiExportOperation::ImageMeasurements || operation==GuiExportOperation::ProjectMeasurements?(measurement_kind==0?"roi":"tissue"):"cells";
    return stem+"-"+kind+(operation==GuiExportOperation::GeoJson?".geojson":".csv");
}
}

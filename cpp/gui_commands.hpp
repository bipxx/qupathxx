#pragma once
#include <imgui.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <map>
#include <unordered_map>
#include <string_view>
#include <bitset>
#include <functional>
#include <vector>
#include <imgui_internal.h>

namespace qpx {struct GuiInput;}
namespace qpx::ui {
enum class KeyScope : std::uint8_t {Global,NotInText,Canvas};
struct Availability {bool enabled=true;const char* reason=nullptr;};
enum class CommandId : std::uint16_t {
    Header_OpenProject,
    Header_AddImages,
    Header_SaveProject,
    Header_SaveProjectAs,
    Header_Close,
    Header_Undo,
    Header_Redo,
    Header_CopyObjects,
    Header_PasteObjects,
    Header_FitImage,
    Header_ShowObjects,
    Header_ProjectPanel,
    Header_ImagePanel,
    Header_AnnotationsPanel,
    Header_WorkflowPanel,
    Header_ResetLayout,
    Header_ReloadUi,
    Header_IncreaseUiSize,
    Header_DecreaseUiSize,
    Header_AutomaticUiSize,
    Header_ThemeDark,
    Header_ThemeLight,
    Header_ThemeSystem,
    Header_ChooseLanguage,
    Header_QuickReference,
    Header_Documentation,
    Header_About,
    Shell_Project,
    Shell_Image,
    Shell_Annotations,
    Shell_Workflow,
    Shell_Select,
    Shell_Rectangle,
    Shell_Ellipse,
    Shell_Polygon,
    Shell_Polyline,
    Shell_Points,
    Shell_Vertices,
    Shell_Hole,
    Shell_Fit,
    Shell_Undo,
    Shell_Redo,
    Shell_Spatial,
    Shell_Channels,
    Shell_Objects,
    Shell_ResetLayout,
    Shell_Settings,
    Tool_ObjectClassifier,
    Tool_PixelClassifier,
    Tool_RawPixelClassifier,
    Tool_Fluorescence,
    Tool_Spatial,
    Tool_Genes,
    Tool_Registration,
    Tool_SpatialImport,
    Tool_Batch,
    Tool_Tma,
    Tool_Density,
    Tool_Channels,
    Child_MarkerRules,
    Child_DarkFlat,
    Child_Unmixing,
    Child_SavedStreams,
    Action_Apply_Boolean_B,
    Action_Merge_annotations_M,
    Action_Apply_object_edit,
    Action_Object_information,
    Action_Apply_Apply_object_display_state,
    Action_Clear_properties_and_unlock_Undo,
    Action_Import_GeoJSON_F7,
    Action_Export_GeoJSON_F6,
    Action_Edit_analysis_inputs,
    Action_Use_saved_stains_for_analysis,
    Action_Export_all_kept_image_cells,
    Action_Export_image_cells_Ctrl_Alt_X,
    Action_Export_project_cells_Ctrl_Shift_X,
    Action_Export_image_Ctrl_Alt_Q,
    Action_Export_project_Ctrl_Shift_Q,
    Action_Hash_file_Ctrl_Shift_F,
    Action_Review_for_project_Ctrl_Alt_F,
    Action_Details_Saved_analysis_details,
    Action_Cancel_table_Ctrl_Alt_Shift_X,
    Action_Cancel_export_Ctrl_Alt_Shift_Q,
    Action_Estimate_stains_F3,
    Action_Use_estimated_stains_Shift_F3,
    Action_Analyze_region_Ctrl_M,
    Action_Detect_tissue_Ctrl_D,
    Action_Trace_contours_Ctrl_J,
    Action_Add_annotations_Ctrl_Shift_J,
    Action_Detect_cells_Ctrl_G,
    Action_Add_cells_Ctrl_Shift_G,
    Action_Export_cell_table_Ctrl_Alt_X,
    Action_Cancel_estimate_Ctrl_F3,
    Action_Open_folder_Batch_output_folder,
    Action_Run_batch_Ctrl_Alt_B,
    Action_Run_resume_tiles,
    Action_Run_fluorescence,
    Action_Keep_fluorescence_cells,
    Action_Keep_fluorescence_run,
    Action_Replay_fluorescence_record,
    Action_Load_fluorescence_settings,
    Action_Remove_fluorescence_record,
    Action_Cancel_fluorescence,
    Action_Load_model,
    Action_Save_model,
    Action_Train_from_labelled_objects,
    Action_Keep_training_record_in_project,
    Action_Predict_object_classes,
    Action_Apply_predictions_one_Undo,
    Action_Cancel_classifier_work,
    Action_Use_saved_classifier_model,
    Action_Remove_kept_result_Remove_classifier_record,
    Action_Cancel_pixel_work,
    Action_Load_pixel_model,
    Action_Save_pixel_model,
    Action_Train_labelled_areas,
    Action_Keep_training_cohort,
    Action_Predict_area,
    Action_Trace_pixel_contours,
    Action_Publish_class_annotations,
    Action_Hide_pixel_overlay,
    Action_Show_pixel_overlay,
    Action_Keep_prediction_mask,
    Action_Export_pixel_stream,
    Action_Saved_stream_files,
    Action_Use_saved_model,
    Action_Preview_saved_mask,
    Action_Remove_record,
    Action_Close_kept_mask_Close_kept_pixel_mask,
    Action_Preview_cohorts_and_calibration,
    Action_Run_project_density,
    Action_Cancel_density,
    Action_Cancel_operation,
    Action_Train_raw_classifier,
    Action_Keep_raw_training,
    Action_Inspect_saved_contours,
    Action_Reverify_saved_contours_for_apply,
    Action_Apply_reverified_contours,
    Action_Load_raw_model,
    Action_Save_raw_model,
    Action_Predict_raw_window,
    Action_Export_raw_prediction,
    Action_Open_and_verify_raw_file,
    Action_Keep_verified_raw_file,
    Action_Open_retained_raw_file,
    Action_Inspect_retained_contours,
    Action_Reverify_retained_contours_for_apply,
    Action_Apply_reverified_retained_contours,
    Action_Remove_raw_file_record,
    Action_Trace_raw_contours,
    Action_Apply_raw_contours,
    Action_Keep_result_Ctrl_K,
    Action_Keep_tissue_mask_Ctrl_U,
    Action_Keep_cell_run_Ctrl_Shift_C,
    Action_Go_to_tables_ROI_tables,
    Action_Go_to_tables_Tissue_tables,
    Action_Go_to_tables_Cell_tables,
    Action_Recheck_saved_Ctrl_Shift_K,
    Action_Recheck_saved_mask_Ctrl_Shift_U,
    Action_Recheck_cells_Ctrl_Alt_C,
    Action_Cancel_analysis,
    Action_Cancel_tissue,
    Action_Cancel_cells_Ctrl_Alt_G,
    Action_Detect_grid_preview,
    Action_Load_grid_preview,
    Action_Create_core_grid,
    Action_Cancel_grid_operation,
    Action_Apply_grid_preview,
    Action_Discard_grid_preview,
    Action_Select_present_cores,
    Action_Per_core_batch,
    Action_Apply_case_ID_to_selected_cores,
    Action_Apply_core_geometry,
    Action_Add_core_in_vacant_slot,
    Action_Apply_labels,
    Action_Add_row_above,
    Action_Add_row_below,
    Action_Add_column_left,
    Action_Add_column_right,
    Action_Remove_row,
    Action_Remove_column,
    Action_Move_grid_and_child_objects,
    Action_Dissolve_grid_keep_objects,
    Action_Export_QuPath_grid,
    Action_Export_case_summary_CSV,
    Action_Export_core_metadata_CSV,
    Action_Cancel_CSV_export,
    DesktopExit,
    Key_RedoAlternate,
    Key_SelectAll,
    Key_Delete,
    Key_NewProject,
    Key_NextImage,
    Key_PreviousImage,
    Key_RelinkImage,
    Key_RemoveImage,
    Key_Recover,
    Key_ReviewCheckpoint,
    Key_FindRecoveries,
    Key_Autosave,
    Key_StopRecoveryMaintenance,
    Key_PasteCurrentPlane,
    Key_CancelFingerprint,
    Key_EscapeCanvas,
    Key_FinishDraft,
    Key_InsertVertex,
    Key_CancelBatch,
    Count
};
struct CommandInfo {CommandId id;const char* caption;ImGuiKeyChord chord;KeyScope scope;bool repeat;const char* legacy_id;CommandId chord_owner=CommandId::Count;bool claim_navigation=false;};
// Rows retain the old identifier verbatim. Aliases in different presentation
// domains keep separate rows. An alias has no chord and names its owning row.
inline constexpr std::array command_table={
    CommandInfo{CommandId::Header_OpenProject,"Open project...",ImGuiMod_Ctrl|ImGuiKey_O,KeyScope::Global,false,"OpenProject"},
    CommandInfo{CommandId::Header_AddImages,"Add images...",ImGuiKey_F8,KeyScope::NotInText,false,"AddImages"},
    CommandInfo{CommandId::Header_SaveProject,"Save project",ImGuiMod_Ctrl|ImGuiKey_S,KeyScope::Global,false,"SaveProject"},
    CommandInfo{CommandId::Header_SaveProjectAs,"Save project as...",0,KeyScope::NotInText,false,"SaveProjectAs"},
    CommandInfo{CommandId::Header_Close,"Close",0,KeyScope::NotInText,false,"Close"},
    CommandInfo{CommandId::Header_Undo,"Undo",ImGuiMod_Ctrl|ImGuiKey_Z,KeyScope::NotInText,false,"Undo"},
    CommandInfo{CommandId::Header_Redo,"Redo",ImGuiMod_Ctrl|ImGuiKey_Y,KeyScope::NotInText,false,"Redo"},
    CommandInfo{CommandId::Header_CopyObjects,"Copy objects",ImGuiMod_Ctrl|ImGuiKey_C,KeyScope::NotInText,false,"CopyObjects"},
    CommandInfo{CommandId::Header_PasteObjects,"Paste objects",ImGuiMod_Ctrl|ImGuiKey_V,KeyScope::NotInText,false,"PasteObjects"},
    CommandInfo{CommandId::Header_FitImage,"Fit image",0,KeyScope::NotInText,false,"FitImage"},
    CommandInfo{CommandId::Header_ShowObjects,"Show objects",ImGuiKey_O,KeyScope::NotInText,false,"ShowObjects"},
    CommandInfo{CommandId::Header_ProjectPanel,"Project",ImGuiMod_Ctrl|ImGuiKey_1,KeyScope::Global,false,"ProjectPanel"},
    CommandInfo{CommandId::Header_ImagePanel,"Image / Brightness and contrast",ImGuiMod_Ctrl|ImGuiKey_2,KeyScope::Global,false,"ImagePanel"},
    CommandInfo{CommandId::Header_AnnotationsPanel,"Annotations",ImGuiMod_Ctrl|ImGuiKey_3,KeyScope::Global,false,"AnnotationsPanel"},
    CommandInfo{CommandId::Header_WorkflowPanel,"Workflow / Measurements",ImGuiMod_Ctrl|ImGuiKey_4,KeyScope::Global,false,"WorkflowPanel"},
    CommandInfo{CommandId::Header_ResetLayout,"Reset layout",0,KeyScope::NotInText,false,"ResetLayout"},
    CommandInfo{CommandId::Header_ReloadUi,"Reload UI resources",ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_R,KeyScope::NotInText,false,"ReloadUi"},
    CommandInfo{CommandId::Header_IncreaseUiSize,"Increase UI size",0,KeyScope::NotInText,false,"IncreaseUiSize"},
    CommandInfo{CommandId::Header_DecreaseUiSize,"Decrease UI size",0,KeyScope::NotInText,false,"DecreaseUiSize"},
    CommandInfo{CommandId::Header_AutomaticUiSize,"Automatic UI size",0,KeyScope::NotInText,false,"AutomaticUiSize"},
    CommandInfo{CommandId::Header_ThemeDark,"Dark",0,KeyScope::NotInText,false,"ThemeDark"},
    CommandInfo{CommandId::Header_ThemeLight,"Light",0,KeyScope::NotInText,false,"ThemeLight"},
    CommandInfo{CommandId::Header_ThemeSystem,"System",0,KeyScope::NotInText,false,"ThemeSystem"},
    CommandInfo{CommandId::Header_ChooseLanguage,"Choose language",ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_L,KeyScope::Global,false,"ChooseLanguage"},
    CommandInfo{CommandId::Header_QuickReference,"Quick reference",ImGuiKey_F1,KeyScope::Global,false,"QuickReference"},
    CommandInfo{CommandId::Header_Documentation,"Documentation",0,KeyScope::NotInText,false,"Documentation"},
    CommandInfo{CommandId::Header_About,"About qupathxx",0,KeyScope::NotInText,false,"About"},
    CommandInfo{CommandId::Shell_Project,"Project",0,KeyScope::Global,false,"Project",CommandId::Header_ProjectPanel},
    CommandInfo{CommandId::Shell_Image,"Image",0,KeyScope::Global,false,"Image",CommandId::Header_ImagePanel},
    CommandInfo{CommandId::Shell_Annotations,"Annotations",0,KeyScope::Global,false,"Annotations",CommandId::Header_AnnotationsPanel},
    CommandInfo{CommandId::Shell_Workflow,"Workflow",0,KeyScope::Global,false,"Workflow",CommandId::Header_WorkflowPanel},
    CommandInfo{CommandId::Shell_Select,"Select",0,KeyScope::NotInText,false,"Select"},
    CommandInfo{CommandId::Shell_Rectangle,"Rectangle",ImGuiKey_R,KeyScope::NotInText,false,"Rectangle"},
    CommandInfo{CommandId::Shell_Ellipse,"Ellipse",ImGuiKey_E,KeyScope::NotInText,false,"Ellipse"},
    CommandInfo{CommandId::Shell_Polygon,"Polygon",ImGuiKey_P,KeyScope::NotInText,false,"Polygon"},
    CommandInfo{CommandId::Shell_Polyline,"Polyline",ImGuiKey_L,KeyScope::NotInText,false,"Polyline"},
    CommandInfo{CommandId::Shell_Points,"Points",ImGuiKey_T,KeyScope::NotInText,false,"Points"},
    CommandInfo{CommandId::Shell_Vertices,"Edit vertices",ImGuiKey_V,KeyScope::NotInText,false,"Vertices"},
    CommandInfo{CommandId::Shell_Hole,"Draw hole",ImGuiKey_H,KeyScope::NotInText,false,"Hole"},
    CommandInfo{CommandId::Shell_Fit,"Fit image",0,KeyScope::NotInText,false,"Fit"},
    CommandInfo{CommandId::Shell_Undo,"Undo",0,KeyScope::NotInText,false,"Undo",CommandId::Header_Undo},
    CommandInfo{CommandId::Shell_Redo,"Redo",0,KeyScope::NotInText,false,"Redo",CommandId::Header_Redo},
    CommandInfo{CommandId::Shell_Spatial,"Spatial...",0,KeyScope::NotInText,false,"Spatial"},
    CommandInfo{CommandId::Shell_Channels,"Raw channels",0,KeyScope::NotInText,false,"Channels",CommandId::Tool_Channels},
    CommandInfo{CommandId::Shell_Objects,"Show objects",0,KeyScope::NotInText,false,"Objects",CommandId::Header_ShowObjects},
    CommandInfo{CommandId::Shell_ResetLayout,"Reset layout",0,KeyScope::NotInText,false,"ResetLayout"},
    CommandInfo{CommandId::Shell_Settings,"Workspace settings",0,KeyScope::NotInText,false,"Settings"},
    CommandInfo{CommandId::Tool_ObjectClassifier,"Object classifiers...",ImGuiKey_F4,KeyScope::NotInText,false,"ObjectClassifier"},
    CommandInfo{CommandId::Tool_PixelClassifier,"Pixel classifiers...",ImGuiKey_F5,KeyScope::NotInText,false,"PixelClassifier"},
    CommandInfo{CommandId::Tool_RawPixelClassifier,"Raw pixel classifier",0,KeyScope::NotInText,false,"RawPixelClassifier"},
    CommandInfo{CommandId::Tool_Fluorescence,"Fluorescence cells...",0,KeyScope::NotInText,false,"Fluorescence"},
    CommandInfo{CommandId::Tool_Spatial,"Spatial...",0,KeyScope::NotInText,false,"Spatial"},
    CommandInfo{CommandId::Tool_Genes,"Gene expression...",0,KeyScope::NotInText,false,"Genes"},
    CommandInfo{CommandId::Tool_Registration,"Landmark registration...",0,KeyScope::NotInText,false,"Registration"},
    CommandInfo{CommandId::Tool_SpatialImport,"Import spatial expression...",0,KeyScope::NotInText,false,"SpatialImport"},
    CommandInfo{CommandId::Tool_Batch,"Batch analysis...",ImGuiMod_Ctrl|ImGuiMod_Alt|ImGuiKey_B,KeyScope::NotInText,false,"Batch"},
    CommandInfo{CommandId::Tool_Tma,"Tissue microarray...",ImGuiMod_Ctrl|ImGuiMod_Alt|ImGuiKey_T,KeyScope::NotInText,false,"Tma"},
    CommandInfo{CommandId::Tool_Density,"Project density...",0,KeyScope::NotInText,false,"Density"},
    CommandInfo{CommandId::Tool_Channels,"Raw channels...",ImGuiKey_F2,KeyScope::NotInText,false,"Channels"},
    CommandInfo{CommandId::Child_MarkerRules,"Marker rules...",0,KeyScope::NotInText,false,"MarkerRules"},
    CommandInfo{CommandId::Child_DarkFlat,"Dark/flat correction...",0,KeyScope::NotInText,false,"DarkFlat"},
    CommandInfo{CommandId::Child_Unmixing,"Linear unmixing...",0,KeyScope::NotInText,false,"Unmixing"},
    CommandInfo{CommandId::Child_SavedStreams,"Saved stream files",0,KeyScope::NotInText,false,"SavedStreams"},
    CommandInfo{CommandId::Action_Apply_Boolean_B,"Apply operation",ImGuiMod_Shift|ImGuiKey_B,KeyScope::Canvas,false,"Apply Boolean (B)"},
    CommandInfo{CommandId::Action_Merge_annotations_M,"Merge annotations",ImGuiMod_Shift|ImGuiKey_M,KeyScope::Canvas,false,"Merge annotations (M)"},
    CommandInfo{CommandId::Action_Apply_object_edit,"Apply object changes",0,KeyScope::NotInText,false,"Apply object edit"},
    CommandInfo{CommandId::Action_Object_information,"Object information",0,KeyScope::NotInText,false,"Object information"},
    CommandInfo{CommandId::Action_Apply_Apply_object_display_state,"Apply color and probability",0,KeyScope::NotInText,false,"Apply###Apply object display state"},
    CommandInfo{CommandId::Action_Clear_properties_and_unlock_Undo,"Clear properties and unlock",0,KeyScope::NotInText,false,"Clear properties and unlock (Undo)"},
    CommandInfo{CommandId::Action_Import_GeoJSON_F7,"Import GeoJSON",ImGuiKey_F7,KeyScope::NotInText,false,"Import GeoJSON (F7)"},
    CommandInfo{CommandId::Action_Export_GeoJSON_F6,"Export GeoJSON",ImGuiKey_F6,KeyScope::NotInText,false,"Export GeoJSON (F6)"},
    CommandInfo{CommandId::Action_Edit_analysis_inputs,"Edit inputs",0,KeyScope::NotInText,false,"Edit analysis inputs"},
    CommandInfo{CommandId::Action_Use_saved_stains_for_analysis,"Use profile",0,KeyScope::NotInText,false,"Use saved stains for analysis"},
    CommandInfo{CommandId::Action_Export_all_kept_image_cells,"Export CSV",0,KeyScope::NotInText,false,"Export all kept image cells"},
    CommandInfo{CommandId::Action_Export_image_cells_Ctrl_Alt_X,"Export CSV",ImGuiMod_Ctrl|ImGuiMod_Alt|ImGuiKey_X,KeyScope::NotInText,false,"Export image cells (Ctrl+Alt+X)"},
    CommandInfo{CommandId::Action_Export_project_cells_Ctrl_Shift_X,"Export CSV",ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_X,KeyScope::NotInText,false,"Export project cells (Ctrl+Shift+X)"},
    CommandInfo{CommandId::Action_Export_image_Ctrl_Alt_Q,"Export CSV",ImGuiMod_Ctrl|ImGuiMod_Alt|ImGuiKey_Q,KeyScope::NotInText,false,"Export image (Ctrl+Alt+Q)"},
    CommandInfo{CommandId::Action_Export_project_Ctrl_Shift_Q,"Export CSV",ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_Q,KeyScope::NotInText,false,"Export project (Ctrl+Shift+Q)"},
    CommandInfo{CommandId::Action_Hash_file_Ctrl_Shift_F,"Hash file",ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_F,KeyScope::NotInText,false,"Hash file (Ctrl+Shift+F)"},
    CommandInfo{CommandId::Action_Review_for_project_Ctrl_Alt_F,"Review for project",ImGuiMod_Ctrl|ImGuiMod_Alt|ImGuiKey_F,KeyScope::NotInText,false,"Review for project (Ctrl+Alt+F)"},
    CommandInfo{CommandId::Action_Details_Saved_analysis_details,"Kept profile details",0,KeyScope::NotInText,false,"Details###Saved analysis details"},
    CommandInfo{CommandId::Action_Cancel_table_Ctrl_Alt_Shift_X,"Cancel",0,KeyScope::NotInText,false,"Cancel table (Ctrl+Alt+Shift+X)"},
    CommandInfo{CommandId::Action_Cancel_export_Ctrl_Alt_Shift_Q,"Cancel",0,KeyScope::NotInText,false,"Cancel export (Ctrl+Alt+Shift+Q)"},
    CommandInfo{CommandId::Action_Estimate_stains_F3,"Estimate stains",ImGuiKey_F3,KeyScope::NotInText,false,"Estimate stains (F3)"},
    CommandInfo{CommandId::Action_Use_estimated_stains_Shift_F3,"Use estimate",ImGuiMod_Shift|ImGuiKey_F3,KeyScope::NotInText,false,"Use estimated stains (Shift+F3)"},
    CommandInfo{CommandId::Action_Analyze_region_Ctrl_M,"Run analysis",ImGuiMod_Ctrl|ImGuiKey_M,KeyScope::NotInText,false,"Analyze region (Ctrl+M)"},
    CommandInfo{CommandId::Action_Detect_tissue_Ctrl_D,"Detect tissue",ImGuiMod_Ctrl|ImGuiKey_D,KeyScope::NotInText,false,"Detect tissue (Ctrl+D)"},
    CommandInfo{CommandId::Action_Trace_contours_Ctrl_J,"Trace contours",ImGuiMod_Ctrl|ImGuiKey_J,KeyScope::NotInText,false,"Trace contours (Ctrl+J)"},
    CommandInfo{CommandId::Action_Add_annotations_Ctrl_Shift_J,"Add annotations",ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_J,KeyScope::NotInText,false,"Add annotations (Ctrl+Shift+J)"},
    CommandInfo{CommandId::Action_Detect_cells_Ctrl_G,"Detect cells",ImGuiMod_Ctrl|ImGuiKey_G,KeyScope::NotInText,false,"Detect cells (Ctrl+G)"},
    CommandInfo{CommandId::Action_Add_cells_Ctrl_Shift_G,"Add cells",ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_G,KeyScope::NotInText,false,"Add cells (Ctrl+Shift+G)"},
    CommandInfo{CommandId::Action_Export_cell_table_Ctrl_Alt_X,"Export CSV",0,KeyScope::NotInText,false,"Export cell table (Ctrl+Alt+X)",CommandId::Action_Export_image_cells_Ctrl_Alt_X},
    CommandInfo{CommandId::Action_Cancel_estimate_Ctrl_F3,"Cancel",0,KeyScope::NotInText,false,"Cancel estimate (Ctrl+F3)"},
    CommandInfo{CommandId::Action_Open_folder_Batch_output_folder,"Open folder",0,KeyScope::NotInText,false,"Open folder###Batch output folder"},
    CommandInfo{CommandId::Action_Run_batch_Ctrl_Alt_B,"Run batch",0,KeyScope::NotInText,false,"Run batch (Ctrl+Alt+B)"},
    CommandInfo{CommandId::Action_Run_resume_tiles,"Run or resume tiles",0,KeyScope::NotInText,false,"Run / resume tiles"},
    CommandInfo{CommandId::Action_Run_fluorescence,"Run analysis",0,KeyScope::NotInText,false,"Run fluorescence"},
    CommandInfo{CommandId::Action_Keep_fluorescence_cells,"Add cells",0,KeyScope::NotInText,false,"Keep fluorescence cells"},
    CommandInfo{CommandId::Action_Keep_fluorescence_run,"Keep result",0,KeyScope::NotInText,false,"Keep fluorescence run"},
    CommandInfo{CommandId::Action_Replay_fluorescence_record,"Recheck result",0,KeyScope::NotInText,false,"Replay fluorescence record"},
    CommandInfo{CommandId::Action_Load_fluorescence_settings,"Load settings from this result",0,KeyScope::NotInText,false,"Load settings from this result###Fluorescence load result settings"},
    CommandInfo{CommandId::Action_Remove_fluorescence_record,"Remove result",0,KeyScope::NotInText,false,"Remove fluorescence record"},
    CommandInfo{CommandId::Action_Cancel_fluorescence,"Cancel",0,KeyScope::NotInText,false,"Cancel fluorescence"},
    CommandInfo{CommandId::Action_Load_model,"Load model",0,KeyScope::NotInText,false,"Load model"},
    CommandInfo{CommandId::Action_Save_model,"Save model",0,KeyScope::NotInText,false,"Save model"},
    CommandInfo{CommandId::Action_Train_from_labelled_objects,"Train classifier",0,KeyScope::NotInText,false,"Train from labelled objects"},
    CommandInfo{CommandId::Action_Keep_training_record_in_project,"Keep training data",0,KeyScope::NotInText,false,"Keep training record in project"},
    CommandInfo{CommandId::Action_Predict_object_classes,"Predict classes",0,KeyScope::NotInText,false,"Predict object classes"},
    CommandInfo{CommandId::Action_Apply_predictions_one_Undo,"Apply predictions",0,KeyScope::NotInText,false,"Apply predictions (one Undo)"},
    CommandInfo{CommandId::Action_Cancel_classifier_work,"Cancel",0,KeyScope::NotInText,false,"Cancel classifier work"},
    CommandInfo{CommandId::Action_Use_saved_classifier_model,"Use kept model",0,KeyScope::NotInText,false,"Use saved classifier model"},
    CommandInfo{CommandId::Action_Remove_kept_result_Remove_classifier_record,"Remove kept result",0,KeyScope::NotInText,false,"Remove kept result###Remove classifier record"},
    CommandInfo{CommandId::Action_Cancel_pixel_work,"Cancel",0,KeyScope::NotInText,false,"Cancel pixel work"},
    CommandInfo{CommandId::Action_Load_pixel_model,"Load pixel model",0,KeyScope::NotInText,false,"Load pixel model"},
    CommandInfo{CommandId::Action_Save_pixel_model,"Save pixel model",0,KeyScope::NotInText,false,"Save pixel model"},
    CommandInfo{CommandId::Action_Train_labelled_areas,"Train classifier",0,KeyScope::NotInText,false,"Train labelled areas"},
    CommandInfo{CommandId::Action_Keep_training_cohort,"Keep training data",0,KeyScope::NotInText,false,"Keep training cohort"},
    CommandInfo{CommandId::Action_Predict_area,"Predict area",0,KeyScope::NotInText,false,"Predict area"},
    CommandInfo{CommandId::Action_Trace_pixel_contours,"Trace contours",0,KeyScope::NotInText,false,"Trace pixel contours"},
    CommandInfo{CommandId::Action_Publish_class_annotations,"Add annotations",0,KeyScope::NotInText,false,"Publish class annotations"},
    CommandInfo{CommandId::Action_Hide_pixel_overlay,"Hide pixel overlay",0,KeyScope::NotInText,false,"Hide pixel overlay"},
    CommandInfo{CommandId::Action_Show_pixel_overlay,"Show pixel overlay",0,KeyScope::NotInText,false,"Show pixel overlay"},
    CommandInfo{CommandId::Action_Keep_prediction_mask,"Keep result",0,KeyScope::NotInText,false,"Keep prediction mask"},
    CommandInfo{CommandId::Action_Export_pixel_stream,"Export prediction file",0,KeyScope::NotInText,false,"Export pixel stream"},
    CommandInfo{CommandId::Action_Saved_stream_files,"Saved stream files",0,KeyScope::NotInText,false,"Saved stream files"},
    CommandInfo{CommandId::Action_Use_saved_model,"Use kept model",0,KeyScope::NotInText,false,"Use saved model"},
    CommandInfo{CommandId::Action_Preview_saved_mask,"Preview kept mask",0,KeyScope::NotInText,false,"Preview saved mask"},
    CommandInfo{CommandId::Action_Remove_record,"Remove kept result",0,KeyScope::NotInText,false,"Remove record"},
    CommandInfo{CommandId::Action_Close_kept_mask_Close_kept_pixel_mask,"Close kept mask",0,KeyScope::NotInText,false,"Close kept mask###Close kept pixel mask"},
    CommandInfo{CommandId::Action_Preview_cohorts_and_calibration,"Check inputs",0,KeyScope::NotInText,false,"Preview cohorts and calibration"},
    CommandInfo{CommandId::Action_Run_project_density,"Run density",0,KeyScope::NotInText,false,"Run project density"},
    CommandInfo{CommandId::Action_Cancel_density,"Cancel density",0,KeyScope::NotInText,false,"Cancel density"},
    CommandInfo{CommandId::Action_Cancel_operation,"Cancel",0,KeyScope::NotInText,false,"Cancel operation"},
    CommandInfo{CommandId::Action_Train_raw_classifier,"Train classifier",0,KeyScope::NotInText,false,"Train raw classifier"},
    CommandInfo{CommandId::Action_Keep_raw_training,"Keep training data",0,KeyScope::NotInText,false,"Keep raw training"},
    CommandInfo{CommandId::Action_Inspect_saved_contours,"Inspect contours",0,KeyScope::NotInText,false,"Inspect saved contours"},
    CommandInfo{CommandId::Action_Reverify_saved_contours_for_apply,"Recheck before adding",0,KeyScope::NotInText,false,"Reverify saved contours for apply"},
    CommandInfo{CommandId::Action_Apply_reverified_contours,"Add rechecked annotations",0,KeyScope::NotInText,false,"Apply reverified contours"},
    CommandInfo{CommandId::Action_Load_raw_model,"Load raw model",0,KeyScope::NotInText,false,"Load raw model"},
    CommandInfo{CommandId::Action_Save_raw_model,"Save raw model",0,KeyScope::NotInText,false,"Save raw model"},
    CommandInfo{CommandId::Action_Predict_raw_window,"Predict window",0,KeyScope::NotInText,false,"Predict raw window"},
    CommandInfo{CommandId::Action_Export_raw_prediction,"Export prediction file",0,KeyScope::NotInText,false,"Export raw prediction"},
    CommandInfo{CommandId::Action_Open_and_verify_raw_file,"Open prediction file",0,KeyScope::NotInText,false,"Open and verify raw file"},
    CommandInfo{CommandId::Action_Keep_verified_raw_file,"Keep file reference",0,KeyScope::NotInText,false,"Keep verified raw file"},
    CommandInfo{CommandId::Action_Open_retained_raw_file,"Open kept file",0,KeyScope::NotInText,false,"Open retained raw file"},
    CommandInfo{CommandId::Action_Inspect_retained_contours,"Inspect contours",0,KeyScope::NotInText,false,"Inspect retained contours"},
    CommandInfo{CommandId::Action_Reverify_retained_contours_for_apply,"Recheck before adding",0,KeyScope::NotInText,false,"Reverify retained contours for apply"},
    CommandInfo{CommandId::Action_Apply_reverified_retained_contours,"Add rechecked annotations",0,KeyScope::NotInText,false,"Apply reverified retained contours"},
    CommandInfo{CommandId::Action_Remove_raw_file_record,"Remove kept result",0,KeyScope::NotInText,false,"Remove raw file record"},
    CommandInfo{CommandId::Action_Trace_raw_contours,"Trace contours",0,KeyScope::NotInText,false,"Trace raw contours"},
    CommandInfo{CommandId::Action_Apply_raw_contours,"Add annotations",0,KeyScope::NotInText,false,"Apply raw contours"},
    CommandInfo{CommandId::Action_Keep_result_Ctrl_K,"Keep result",ImGuiMod_Ctrl|ImGuiKey_K,KeyScope::NotInText,false,"Keep result (Ctrl+K)"},
    CommandInfo{CommandId::Action_Keep_tissue_mask_Ctrl_U,"Keep result",ImGuiMod_Ctrl|ImGuiKey_U,KeyScope::NotInText,false,"Keep tissue mask (Ctrl+U)"},
    CommandInfo{CommandId::Action_Keep_cell_run_Ctrl_Shift_C,"Keep result",ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_C,KeyScope::NotInText,false,"Keep cell run (Ctrl+Shift+C)"},
    CommandInfo{CommandId::Action_Go_to_tables_ROI_tables,"View kept results",0,KeyScope::NotInText,false,"Go to tables###ROI tables"},
    CommandInfo{CommandId::Action_Go_to_tables_Tissue_tables,"View kept results",0,KeyScope::NotInText,false,"Go to tables###Tissue tables"},
    CommandInfo{CommandId::Action_Go_to_tables_Cell_tables,"View kept results",0,KeyScope::NotInText,false,"Go to tables###Cell tables"},
    CommandInfo{CommandId::Action_Recheck_saved_Ctrl_Shift_K,"Recheck result",ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_K,KeyScope::NotInText,false,"Recheck saved (Ctrl+Shift+K)"},
    CommandInfo{CommandId::Action_Recheck_saved_mask_Ctrl_Shift_U,"Recheck result",ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_U,KeyScope::NotInText,false,"Recheck saved mask (Ctrl+Shift+U)"},
    CommandInfo{CommandId::Action_Recheck_cells_Ctrl_Alt_C,"Recheck result",ImGuiMod_Ctrl|ImGuiMod_Alt|ImGuiKey_C,KeyScope::NotInText,false,"Recheck cells (Ctrl+Alt+C)"},
    CommandInfo{CommandId::Action_Cancel_analysis,"Cancel",0,KeyScope::NotInText,false,"Cancel analysis"},
    CommandInfo{CommandId::Action_Cancel_tissue,"Cancel",0,KeyScope::NotInText,false,"Cancel tissue"},
    CommandInfo{CommandId::Action_Cancel_cells_Ctrl_Alt_G,"Cancel",0,KeyScope::NotInText,false,"Cancel cells (Ctrl+Alt+G)"},
    CommandInfo{CommandId::Action_Detect_grid_preview,"Preview grid",0,KeyScope::NotInText,false,"Detect grid preview"},
    CommandInfo{CommandId::Action_Load_grid_preview,"Load preview",0,KeyScope::NotInText,false,"Load grid preview"},
    CommandInfo{CommandId::Action_Create_core_grid,"Create grid",0,KeyScope::NotInText,false,"Create core grid"},
    CommandInfo{CommandId::Action_Cancel_grid_operation,"Cancel",0,KeyScope::NotInText,false,"Cancel grid operation"},
    CommandInfo{CommandId::Action_Apply_grid_preview,"Add grid",0,KeyScope::NotInText,false,"Apply grid preview"},
    CommandInfo{CommandId::Action_Discard_grid_preview,"Discard preview",0,KeyScope::NotInText,false,"Discard grid preview"},
    CommandInfo{CommandId::Action_Select_present_cores,"Select present cores",0,KeyScope::NotInText,false,"Select present cores"},
    CommandInfo{CommandId::Action_Per_core_batch,"Analyze cores...",0,KeyScope::NotInText,false,"Per-core batch..."},
    CommandInfo{CommandId::Action_Apply_case_ID_to_selected_cores,"Apply case ID",0,KeyScope::NotInText,false,"Apply case ID to selected cores"},
    CommandInfo{CommandId::Action_Apply_core_geometry,"Apply geometry",0,KeyScope::NotInText,false,"Apply core geometry"},
    CommandInfo{CommandId::Action_Add_core_in_vacant_slot,"Add core",0,KeyScope::NotInText,false,"Add core in vacant slot"},
    CommandInfo{CommandId::Action_Apply_labels,"Apply labels",0,KeyScope::NotInText,false,"Apply labels"},
    CommandInfo{CommandId::Action_Add_row_above,"Add row above",0,KeyScope::NotInText,false,"Add row above"},
    CommandInfo{CommandId::Action_Add_row_below,"Add row below",0,KeyScope::NotInText,false,"Add row below"},
    CommandInfo{CommandId::Action_Add_column_left,"Add column left",0,KeyScope::NotInText,false,"Add column left"},
    CommandInfo{CommandId::Action_Add_column_right,"Add column right",0,KeyScope::NotInText,false,"Add column right"},
    CommandInfo{CommandId::Action_Remove_row,"Remove row %s (%zu cores)",0,KeyScope::NotInText,false,"Remove row"},
    CommandInfo{CommandId::Action_Remove_column,"Remove column %s (%zu cores)",0,KeyScope::NotInText,false,"Remove column"},
    CommandInfo{CommandId::Action_Move_grid_and_child_objects,"Move grid and child objects",0,KeyScope::NotInText,false,"Move grid and child objects"},
    CommandInfo{CommandId::Action_Dissolve_grid_keep_objects,"Dissolve grid (keep objects)",0,KeyScope::NotInText,false,"Dissolve grid (keep objects)"},
    CommandInfo{CommandId::Action_Export_QuPath_grid,"Export QPTMA",0,KeyScope::NotInText,false,"Export QuPath grid"},
    CommandInfo{CommandId::Action_Export_case_summary_CSV,"Export CSV",0,KeyScope::NotInText,false,"Export case summary CSV"},
    CommandInfo{CommandId::Action_Export_core_metadata_CSV,"Export CSV",0,KeyScope::NotInText,false,"Export core metadata CSV"},
    CommandInfo{CommandId::Action_Cancel_CSV_export,"Cancel",0,KeyScope::NotInText,false,"Cancel CSV export"},
    CommandInfo{CommandId::DesktopExit,"Exit",0,KeyScope::Global,false,"Close"},
    CommandInfo{CommandId::Key_RedoAlternate,"Redo",ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_Z,KeyScope::NotInText,false,"RedoAlternate"},
    CommandInfo{CommandId::Key_SelectAll,"Select all objects",ImGuiMod_Ctrl|ImGuiKey_A,KeyScope::Canvas,false,"SelectAll"},
    CommandInfo{CommandId::Key_Delete,"Delete selected objects",ImGuiKey_Delete,KeyScope::Canvas,false,"Delete"},
    CommandInfo{CommandId::Key_NewProject,"New project",ImGuiMod_Ctrl|ImGuiKey_N,KeyScope::NotInText,false,"NewProject"},
    CommandInfo{CommandId::Key_NextImage,"Next image",ImGuiMod_Ctrl|ImGuiKey_PageDown,KeyScope::NotInText,false,"NextImage",CommandId::Count,true},
    CommandInfo{CommandId::Key_PreviousImage,"Previous image",ImGuiMod_Ctrl|ImGuiKey_PageUp,KeyScope::NotInText,false,"PreviousImage",CommandId::Count,true},
    CommandInfo{CommandId::Key_RelinkImage,"Relink image",ImGuiKey_F9,KeyScope::NotInText,false,"RelinkImage"},
    CommandInfo{CommandId::Key_RemoveImage,"Remove image",ImGuiKey_F12,KeyScope::NotInText,false,"RemoveImage"},
    CommandInfo{CommandId::Key_Recover,"Open selected checkpoint",ImGuiKey_F11,KeyScope::NotInText,false,"Recover"},
    CommandInfo{CommandId::Key_ReviewCheckpoint,"Delete selected checkpoint...",ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_Backspace,KeyScope::NotInText,false,"ReviewCheckpoint"},
    CommandInfo{CommandId::Key_FindRecoveries,"Find recovery files",ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_F5,KeyScope::NotInText,false,"FindRecoveries"},
    CommandInfo{CommandId::Key_Autosave,"Save recovery now",ImGuiKey_F10,KeyScope::NotInText,false,"Autosave"},
    CommandInfo{CommandId::Key_StopRecoveryMaintenance,"Stop maintenance",0,KeyScope::NotInText,false,"StopRecoveryMaintenance"},
    CommandInfo{CommandId::Key_PasteCurrentPlane,"Paste to displayed plane",ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_V,KeyScope::NotInText,false,"PasteCurrentPlane"},
    CommandInfo{CommandId::Key_CancelFingerprint,"Cancel file hash",0,KeyScope::NotInText,false,"CancelFingerprint"},
    CommandInfo{CommandId::Key_EscapeCanvas,"Cancel drawing or clear selection",ImGuiKey_Escape,KeyScope::Canvas,false,"EscapeCanvas"},
    CommandInfo{CommandId::Key_FinishDraft,"Finish shape",ImGuiKey_Enter,KeyScope::Canvas,false,"FinishDraft"},
    CommandInfo{CommandId::Key_InsertVertex,"Insert vertex",ImGuiKey_I,KeyScope::Canvas,false,"InsertVertex"},
    CommandInfo{CommandId::Key_CancelBatch,"Cancel batch",0,KeyScope::NotInText,false,"CancelBatch"},
};
static_assert(command_table.size()==static_cast<std::size_t>(CommandId::Count));
static_assert([] {
    for(std::size_t i=0;i<command_table.size();++i)
        if(static_cast<std::size_t>(command_table[i].id)!=i)return false;
    return true;
}(),"Command rows must be indexed by their identifiers");
inline constexpr const CommandInfo& command(CommandId id) {return command_table.at(static_cast<std::size_t>(id));}
inline std::string chord_text(ImGuiKeyChord chord) {
    std::string result;
    for(auto mod:{ImGuiMod_Ctrl,ImGuiMod_Alt,ImGuiMod_Shift,ImGuiMod_Super})if(chord&mod) {
        result+=mod==ImGuiMod_Ctrl?"Ctrl+":mod==ImGuiMod_Alt?"Alt+":mod==ImGuiMod_Shift?"Shift+":"Super+";
    }
    const auto key=static_cast<ImGuiKey>(chord&~ImGuiMod_Mask_);
    if(key!=ImGuiKey_None)result+=ImGui::GetKeyName(key);
    return result;
}
// Construct once; readers never build a chord string during a frame.
inline const char* command_chord(CommandId id) {
    static const auto texts=[] {std::array<std::string,command_table.size()> values{};
        for(std::size_t i=0;i<values.size();++i)values[i]=chord_text(command(command_table[i].chord_owner==CommandId::Count?command_table[i].id:command_table[i].chord_owner).chord);return values;}();
    return texts.at(static_cast<std::size_t>(id)).c_str();
}
inline const char* command_label(CommandId id) {
    static const auto labels=[] {std::array<std::string,command_table.size()> values{};
        for(std::size_t i=0;i<values.size();++i) {
            const auto& row=command_table[i];const auto* stable=std::strstr(row.legacy_id,"###");
            values[i]=std::string(row.caption)+"###"+(stable?stable+3:row.legacy_id);
        }return values;}();
    return labels.at(static_cast<std::size_t>(id)).c_str();
}
// Presentation aliases preserve existing widget IDs without duplicating any
// caption. Construct once per (row, legacy widget identity), never per frame.
inline const char* presentation_label(CommandId id,const char* legacy) {
    static std::map<std::pair<CommandId,std::string>,std::string> labels;
    // Search without constructing a temporary owning string on the hot path.
    for(const auto& entry:labels)if(entry.first.first==id && entry.first.second==legacy)return entry.second.c_str();
    return labels.emplace(std::make_pair(id,std::string(legacy)),std::string(command(id).caption)+"###"+legacy).first->second.c_str();
}
inline const CommandInfo& workflow_command(const char* original) {
    static const auto index=[] {
        std::unordered_map<std::string_view,const CommandInfo*> rows;
        for(const auto& row:command_table)if(row.id>=CommandId::Action_Apply_Boolean_B)rows.emplace(row.legacy_id,&row);
        return rows;
    }();
    if(const auto found=index.find(original);found!=index.end())return *found->second;
    throw std::logic_error("Unregistered workflow original");
}
struct ChordContext {bool dialog_open=false,text_input=false,workspace_focused=false,control_active=false;};
// An unsubmitted popup can retain a stack entry indefinitely. Only submitted
// windows own keys; retain last-frame ownership through a dismissal frame.
inline bool drawn_popup_owns_keys(bool before_new_frame) {
    for(const auto& popup:ImGui::GetCurrentContext()->OpenPopupStack)
        if(popup.Window && (before_new_frame?popup.Window->Active:(popup.Window->WasActive || popup.Window->Active)))return true;
    return false;
}
// Built once before command sites. Workspace panels edit the image's objects;
// a floating tool or settings window must not borrow that keyboard ownership.
inline ChordContext chord_context(bool popup_owned_before,ImGuiID canvas_window,const std::array<ImGuiID,5>& workspace_windows,bool external_control=false) {
    const auto& g=*ImGui::GetCurrentContext();const auto& io=g.IO;
    const auto belongs=[&](const ImGuiWindow* window) {
        if(!window)return false;
        const auto root=window->RootWindow?window->RootWindow->ID:window->ID;
        if(canvas_window && (window->ID==canvas_window || root==canvas_window))return true;
        for(const auto id:workspace_windows)if(id && root==id)return true;
        return false;
    };
    const bool pointer=belongs(g.HoveredWindow);
    const bool focus=belongs(g.NavWindow);
    // A canvas drag is not a panel control. A released mouse control no longer
    // owns the key, even while ImGui is clearing its activation state.
    const auto canvas_item=ImHashStr("image canvas",0,canvas_window);
    const bool released_mouse=g.ActiveIdSource==ImGuiInputSource_Mouse && g.ActiveIdMouseButton>=0 &&
        !io.MouseDown[g.ActiveIdMouseButton] && !io.WantTextInput;
    const bool shell_pressed=external_control && (io.MouseDown[0] || ImGui::IsKeyDown(ImGuiKey_Tab) ||
        ImGui::IsKeyDown(ImGuiKey_Enter) || ImGui::IsKeyDown(ImGuiKey_Space));
    const bool control_active=shell_pressed || (g.ActiveId!=0 && g.ActiveId!=canvas_item && !released_mouse) ||
        (g.NavActivateId!=0 && g.NavActivateId!=canvas_item);
    return {popup_owned_before || drawn_popup_owns_keys(false),io.WantTextInput,
        !control_active && !io.WantTextInput && (pointer || focus),control_active};
}
inline bool chord_allowed(CommandId id,const ChordContext& context,bool scripted=false) {
    const auto& row=command(id);
    return row.chord_owner==CommandId::Count && !context.dialog_open &&
        (row.scope==KeyScope::Global || !context.text_input) &&
        (row.scope!=KeyScope::Canvas || (!context.control_active && (scripted || context.workspace_focused)));
}
// Private UI/test seam, like workflow_trace: no public input enum or options.
struct CommandTrace {
    std::function<void(const std::function<void(const GuiInput&)>&)> replay_input;
    std::function<void(int)> before_context;
    std::function<void(const ChordContext&,const std::bitset<command_table.size()>&)> after_frame;
    bool probe_all=false,end_session=false;
    int tool=-1,task=-1;bool objects_visible=true,project_browser_enter=false;std::size_t draft_points=0;
    struct Recovery {bool busy=false,published=false,cancel_requested=false,cancelled=false;std::uint64_t completed=0;std::string phase;} recovery;
    struct TaskJob {std::string state;std::uint64_t generation=0;bool busy=false;};
    bool observe_task_jobs=false;
    std::array<TaskJob,3> task_jobs;
    std::string batch_state;bool batch_busy=false,fingerprint_busy=false;
};
inline thread_local CommandTrace* command_trace=nullptr;
struct CommandFrame {
    ChordContext context;
    std::bitset<command_table.size()> requested;
    mutable std::array<std::uint8_t,command_table.size()> evaluated{};
    mutable std::bitset<command_table.size()> fired;
    void request(CommandId id) {requested.set(static_cast<std::size_t>(id));}
};
// Register every frame, as the former global Shortcut readers did. Ownership
// keeps ImGui's unmodified PageUp/PageDown navigation from also using the key;
// chord_pressed remains the only application command reader.
inline void claim_command_navigation() {
    for(const auto& row:command_table)if(row.claim_navigation)
        ImGui::SetShortcutRouting(row.chord,ImGuiInputFlags_RouteGlobal,ImHashStr(row.legacy_id));
}
// Escape/Enter also reach focused controls so scripted dialogs and menus react
// as they do to the keyboard. Resolve a
// request only when its queued press registers, including a trickled press one
// frame after a mouse release. Never dispatch once now and once after trickling.
struct ReplayControlPresses {
    struct Press {unsigned int event;CommandId command;ImGuiKey key;};
    std::vector<Press> pending;
    void enqueue(CommandId command,ImGuiKey key) {
        auto& g=*ImGui::GetCurrentContext();const auto before=g.InputEventsQueue.Size;
        g.IO.AddKeyEvent(key,true);
        if(g.InputEventsQueue.Size>before)pending.push_back({g.InputEventsQueue.back().EventId,command,key});
    }
    void dispatch(CommandFrame& frame) {
        const auto& queue=ImGui::GetCurrentContext()->InputEventsQueue;
        for(auto it=pending.begin();it!=pending.end();) {
            bool queued=false;for(const auto& event:queue)if(event.EventId==it->event){queued=true;break;}
            if(queued){++it;continue;}
            const auto* key=ImGui::GetKeyData(it->key);
            if(key->Down && key->DownDuration==0)frame.request(it->command);
            it=pending.erase(it);
        }
    }
};
inline bool chord_pressed(CommandId id,const CommandFrame& frame) {
    const auto index=static_cast<std::size_t>(id);auto& cached=frame.evaluated[index];
    if(cached)return cached==2;
    const auto& row=command_table[index];const bool scripted=frame.requested[index];
    const bool pressed=chord_allowed(id,frame.context,scripted) &&
        (scripted || (row.chord && (ImGui::GetIO().KeyMods&ImGuiMod_Mask_)==(row.chord&ImGuiMod_Mask_) &&
            ImGui::IsKeyPressed(static_cast<ImGuiKey>(row.chord&~ImGuiMod_Mask_),row.repeat)));
    cached=pressed?2:1;frame.fired[index]=pressed;return pressed;
}
inline bool chord_pressed(CommandId id,const ChordContext& context) {return chord_pressed(id,CommandFrame{context});}
// Accept a request to stop work whoever owns the keyboard: it cannot start anything.
inline bool batch_cancel_requested(const CommandFrame& frame) {
    constexpr auto index=static_cast<std::size_t>(CommandId::Key_CancelBatch);
    if(!frame.requested[index])return false;
    frame.evaluated[index]=2;frame.fired.set(index);return true;
}
inline thread_local CommandFrame* active_command_frame=nullptr;
// Embedded controller fixtures have no outer application loop. They use the
// same gate and one snapshot per frame, without claiming a canvas focus owner.
inline const CommandFrame& frame_commands() {
    if(active_command_frame)return *active_command_frame;
    static thread_local CommandFrame embedded;
    static thread_local ImGuiContext* owner=nullptr;static thread_local int frame=-1;
    if(owner!=ImGui::GetCurrentContext() || frame!=ImGui::GetFrameCount()) {
        owner=ImGui::GetCurrentContext();frame=ImGui::GetFrameCount();
        embedded=CommandFrame{chord_context(false,0,{})};
    }
    return embedded;
}
struct CommandFrameScope {
    CommandFrame* previous=active_command_frame;
    explicit CommandFrameScope(CommandFrame& frame) {active_command_frame=&frame;}
    ~CommandFrameScope() {active_command_frame=previous;}
};
static_assert([] {
    for(std::size_t i=0;i<command_table.size();++i) {
        const auto& row=command_table[i];
        if(row.chord_owner!=CommandId::Count && (row.chord || command(row.chord_owner).chord_owner!=CommandId::Count))return false;
        if(row.chord)for(std::size_t j=0;j<i;++j)if(command_table[j].chord==row.chord)return false;
    }return true;
}(),"A chord has exactly one owner");
}

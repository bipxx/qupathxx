"""Guard the shared GUI export routes; behavioral coverage is native."""
from pathlib import Path
from gui_contract_calls import has_call
from gui_command_inventory import calls, code_mask
import re

ROOT = Path(__file__).resolve().parents[1]


def test_path_fields_preserve_labelled_actions_and_group_order():
    gui = (ROOT / "cpp/gui.cpp").read_text(encoding="utf8")
    group = gui[gui.index('ui::SeparatorText("Import GeoJSON")'):gui.index('if(!interchange_status.empty())', gui.index('ui::SeparatorText("Import GeoJSON")'))]
    assert group.index('paths.import_source.draw(path_context,false,{},{},"Import source");') < group.index('ui::Checkbox("Allow lossy conversion"') < group.index('ui::WorkflowAction("Import GeoJSON (F7)")')
    assert group.index('paths.geojson.draw(path_context,false,{},{},"Export destination");') < group.index('ui::Checkbox("Selected objects only"') < group.index('ui::WorkflowAction("Export GeoJSON (F6)")')
    assert group.index('paths.object_csv.draw(path_context,false,{},{},"Export destination");') < group.index('ui::Checkbox("Include mean RGB"') < group.index('ui::Button("Export measurements CSV")')
    assert 'ui::WorkflowAction(caption,ui::design::Action::Primary,available)' in gui
    table = gui[gui.index('paths.output(table_kind==0?'):gui.index('// Independent jobs remain visible')]
    assert table.index('.draw(path_context,false,{},{},"CSV file");') < table.index('ui::WorkflowAction(caption') < table.index('ui::WorkflowReason(caption,available)')
    hash_block = gui[gui.index('paths.fingerprint_output.draw('):gui.index('if(ui::Button(ui::presentation_label(ui::CommandId::Key_CancelFingerprint,"Cancel file hash")')]
    assert hash_block.index('paths.fingerprint_output.draw(path_context,false,{},{},"Optional JSON destination");') < hash_block.index('ui::TextWrapped("Existing JSON destinations ask before replacement.') < hash_block.index('ui::WorkflowAction("Hash file (Ctrl+Shift+F)",ui::design::Action::Primary,hash_available)')
    paths = (ROOT / "cpp/gui_core_paths.hpp").read_text(encoding="utf8")
    assert 'ui::PathField geojson{{"##Interchange path",ui::PathRole::NewFile,{".geojson",".json"},"",".geojson destination"}}' in paths
    assert 'ui::PathField object_csv{{"##Object CSV path",ui::PathRole::NewFile,{".csv"},"",".csv destination"}}' in paths

def test_gui_exports_share_guarded_dispatch():
    gui = (ROOT / "cpp/gui.cpp").read_text(encoding="utf8")
    assert "auto begin_export=" in gui and "auto dispatch_export=" in gui
    assert "save_geojson(document," not in gui
    assert "save_measurements_csv(document," not in gui
    assert "cell_table_job.request(" not in gui
    assert "measurement_job.request(" not in gui
    assert "ExportWritePolicy::CreateNew" in gui
    assert 'ui::chord_pressed(ui::CommandId::Action_Export_GeoJSON_F6,ui::frame_commands()))begin_export' in gui
    assert "GuiInputTarget::ConfirmReplaceExport" in gui
    gate = (ROOT / "cpp/gui_output_gate.hpp").read_text(encoding="utf8")
    assert "same_export_destination(*destination_,*workflow_file_version(pending_->path))" in gate
    assert "capture_export_owner(stamp.selected_only,stamp.project_scope,false).same_context(stamp)" in gui
    assert "output_gate.submit(std::move(output))" in gui
    assert "paths.import_source.state().resolved" in gui
    paths = (ROOT / "cpp/gui_core_paths.hpp").read_text(encoding="utf8")
    assert 'ui::PathField import_source{{"##Import source"' in paths
    assert 'ui::PathField geojson{{"##Interchange path"' in paths
    assert 'ui::PathField object_csv{{"##Object CSV path"' in paths

def test_sampling_caption_and_run_share_native_builders():
    gui = (ROOT / "cpp/gui.cpp").read_text(encoding="utf8")
    gui += (ROOT / "cpp/gui_analysis_tasks.hpp").read_text(encoding="utf8")
    for native_type in ['TissueJob','CellJob']:
        jobs=re.findall(r'\b'+native_type+r'\s+(\w+)\s*;',code_mask(gui))
        assert len(jobs)==1, native_type
        requests=list(calls(gui,re.escape(jobs[0])+r'\.request'))
        assert any(args and has_call(gui,'sampling_summary',args[0]) for args in requests), native_type
    assert "tissue_sampling_cache.update" in gui and "cell_sampling_cache.update" in gui
    assert "key.parameters(tissue_parameters)" in gui
    assert "key.parameters(cell_parameters)" in gui
    assert "analysis_intensity_statistics" in gui

def test_visibility_is_presentation_only_and_filter_does_not_change_export():
    gui = (ROOT / "cpp/gui.cpp").read_text(encoding="utf8")
    assert "if(objects_visible)for (auto id:visible_ids)" in gui
    assert "next_overlay_key.objects_visible=objects_visible" in gui
    assert "else if (!tool && objects_visible)" in gui
    assert "if(!objects_visible) {objects_visible=true;return;}" in gui
    assert "select_ordered_range(*selection,list_ids" in gui
    assert "selection->select_range(" not in gui
    assert "###Object/" in gui
    assert "detail::draw_shape_measurements(document,selected" in gui
    assert "document.analysis_record_count()" in gui

def test_export_jobs_are_polled_before_rendering_collapsible_panels():
    gui = (ROOT / "cpp/gui.cpp").read_text(encoding="utf8")
    assert gui.index("const auto measurement_export_status=measurement_job.poll();") < gui.index("auto begin_export=")
    assert 'ui::Combo("Table",&table_kind' in gui
    assert gui.index("const auto table_status=cell_table_job.poll();") > gui.index("// Independent jobs remain visible")
    assert "if(!measurement_export_status.error.empty())ui::TextWrapped" in gui

def test_hidden_delete_requires_a_fresh_key_press():
    gui = (ROOT / "cpp/gui.cpp").read_text(encoding="utf8")
    assert "has_image && ui::chord_pressed(ui::CommandId::Key_Delete,ui::frame_commands())) delete_selection();" in gui
    commands=(ROOT / "cpp/gui_commands.hpp").read_text(encoding="utf8")
    assert 'Key_Delete,"Delete selected objects",ImGuiKey_Delete,KeyScope::Canvas,false' in commands


def test_one_gate_owns_overwrite_decisions_and_all_file_writers():
    gate = (ROOT / "cpp/gui_output_gate.hpp").read_text(encoding="utf8")
    gui = (ROOT / "cpp/gui.cpp").read_text(encoding="utf8")
    assert gui.count("detail::OutputGate output_gate;") == 1
    assert "RenderDialog(ui::DialogKind::ReplaceExport" not in gui
    assert gate.count("RenderDialog(ui::DialogKind::ReplaceExport") == 1
    assert gate.index("guard_output_intent(intent.path,intent.protected_paths)") < gate.index("std::filesystem::exists(std::filesystem::u8path(intent.path))")
    assert "else write(intent,ExportWritePolicy::CreateNew)" in gate
    assert "if(intent.preauthorized) {write(intent,ExportWritePolicy::ReplaceExisting);return;}" in gate
    assert "if(!owner_valid(pending_->owner))" in gate
    assert "if(action==ui::DialogAction::Confirm && !confirm(owner_valid))" in gate
    assert "same_export_destination" in gate
    assert "if(protection_)guard_output_intent(intent.path,protection_())" in gate
    assert "existing_refusal" in gate
    notices = (ROOT / "cpp/gui_notifications.hpp").read_text(encoding="utf8")
    assert 'control("Copy path"' in notices and 'control("Open folder"' in notices
    assert "ImGui::SetClipboardText(entry.notice.path.c_str())" in notices and "open_in_shell(" in notices
    assert '"Exported.",ui::notice_path(path),true' in gate
    assert "active_fail_=intent.fail" in gate
    assert "std::move(error),active_fail_" in gate
    assert "if(i->fail)i->fail(message);" in gate
    for file, count in {"gui_tma.hpp": 2, "gui_raw.hpp": 2, "gui_spatial.hpp": 1,
                        "gui_pixel_classifier.hpp": 2, "gui_object_classifier.hpp": 1,
                        "gui_raw_pixel.hpp": 3}.items():
        source = (ROOT / "cpp" / file).read_text(encoding="utf8")
        assert source.count("gate->submit(") + source.count("gate.submit(") == count, file
    assert 'output_gate.submit(output,detail::OutputScope{std::string(ui::i18n::text("File fingerprint"))' in gui
    tasks = (ROOT / "cpp/gui_analysis_tasks.hpp").read_text(encoding="utf8")
    assert 'output_gate.submit(paths.stain_save.state().resolved' in tasks
    assert "pending_path_field->take_picked()" in gui and "submit_export(std::move(intent))" in gui


def test_no_replace_checkbox_or_secondary_protection_list():
    import re
    for path in (ROOT / "cpp").glob("gui*.*"):
        if path.suffix not in {".hpp", ".cpp"}:
            continue
        source = path.read_text(encoding="utf8")
        assert not re.search(r'Checkbox\("Replace\b', source), path
        if path.name != "gui_export.hpp":
            assert "static std::vector<std::string> protected_paths(const Project*" not in source
        assert "spatial_project_paths(project,catalog" not in source
    registry = (ROOT / "cpp/gui_dialog_contract.hpp").read_text(encoding="utf8")
    inventory = registry[registry.index("#define QPX_UI_DIALOGS"):registry.index("enum class DialogKind")]
    assert inventory.count("X(ReplaceExport,") == 1
    assert not re.search(r"X\(Replace(?:Model|Prediction|Checkpoint|Stream)", inventory)
    for file, flags in {"gui_object_classifier.hpp": ["classifier_replace_model"],
                        "gui_pixel_classifier.hpp": ["pixel_replace_model", "pixel_stream_replace"],
                        "gui_raw_pixel.hpp": ["options_.replace_model", "settings.checkpoint.replace_existing", "save.replace_existing"]}.items():
        source = (ROOT / "cpp" / file).read_text(encoding="utf8")
        for flag in flags:
            assert flag in source


def test_overwrite_captions_match_policy_and_retry_keeps_native_capture():
    gui = (ROOT / "cpp/gui.cpp").read_text(encoding="utf8")
    assert "dispatch_export(intent,policy,prepared);" in gui
    assert "dispatch_export(intent,policy,std::move(prepared));" not in gui
    raw = (ROOT / "cpp/gui_raw.hpp").read_text(encoding="utf8")
    tma = (ROOT / "cpp/gui_tma.hpp").read_text(encoding="utf8")
    assert "Existing files are kept." not in raw
    assert "when requested and creates a new file." not in tma
    assert "Existing files ask before replacement." in raw and "Existing files ask before replacement." in tma
    assert 'output_scope(case_summary_?"Export case summary CSV":"Export core metadata CSV",case_summary_ && case_cohort_,ui::command(ui::CommandId::Tool_Tma).caption)' in tma


def test_fix1_owner_capture_is_not_per_frame_and_project_scope_is_explicit():
    gui = (ROOT / "cpp/gui.cpp").read_text(encoding="utf8")
    gate = (ROOT / "cpp/gui_output_gate.hpp").read_text(encoding="utf8")
    draw = gate[gate.index("    void draw("):]
    assert "capture_(" not in draw
    assert "capture_(pending_->owner.selected_only,pending_->owner.project_scope)==pending_->owner" in gate
    assert "owner(bool selected=true,bool project_scope=false)" in gate
    assert "owner(true,project_scope)" in gate
    assert "if(capture_members && project_scope && options.project)" in gui
    assert "capture_export_owner(stamp.selected_only,stamp.project_scope,false).same_context(stamp)" in gui
    dispatch = gui[gui.index("auto dispatch_export="):gui.index("auto submit_export=")]
    assert "export_owner(intent)" not in dispatch
    assert "capture_export_owner(intent.selected_only,intent.project_scope(),false).same_context(intent.owner)" in dispatch
    tma = (ROOT / "cpp/gui_tma.hpp").read_text(encoding="utf8")
    assert 'std::vector<std::string>{},case_summary_ && case_cohort_);' in tma


def test_fix1_native_raw_guard_bounded_errors_and_association_order():
    pixel = (ROOT / "cpp/gui_raw_pixel.hpp").read_text(encoding="utf8")
    assert pixel.count("guard_raw_pixel_frontend_output(project,") == 6
    assert "export_path(settings.checkpoint_path);guard_raw_pixel_frontend_output(project,path)" in pixel
    assert "export_path(stream_path_.state().resolved);guard_raw_pixel_frontend_output(project,path)" in pixel
    assert "guard_raw_pixel_frontend_output(project,path);jobs_.save_model" in pixel
    assert "guard_raw_pixel_frontend_output(project,path);save.replace_existing" in pixel
    raw = (ROOT / "cpp/gui_raw.hpp").read_text(encoding="utf8")
    assert 'bounded_utf8(result.error,8192);}catch(...) {result.error="Raw export failed";}' in raw
    assert 'csv_stats_.state="failed";failure(std::runtime_error(message))' in raw
    gui = (ROOT / "cpp/gui.cpp").read_text(encoding="utf8")
    assoc = gui[gui.index("const auto associate=fingerprint_associate;"):gui.index("auto start=[&,associate")]
    assert assoc.index('if(associate && !has_image)') < assoc.index('document.server()->reader_source()') < assoc.index('if(associate && !source)')
    assert 'select an image to associate its source' in assoc


def test_fix1_long_paths_missing_gates_and_passive_receipts():
    tma = (ROOT / "cpp/gui_tma.hpp").read_text(encoding="utf8")
    metadata = tma[tma.index("void write_metadata("):tma.index("public:", tma.index("void write_metadata("))]
    assert 'wide.substr(2)' in metadata and 'UNC' in metadata
    assert 'const auto destination=path;' in metadata and 'csv_destination_=destination;' in metadata
    for file in ["gui_tma.hpp", "gui_raw.hpp", "gui_spatial.hpp", "gui_object_classifier.hpp", "gui_pixel_classifier.hpp", "gui_raw_pixel.hpp"]:
        source = (ROOT / "cpp" / file).read_text(encoding="utf8")
        assert "if(gate)gate->submit" not in source
        assert "output_gate_available(gate," in source
    for file in ["gui_pixel_classifier.hpp", "gui_raw_pixel.hpp"]:
        assert "ImGui::Dummy({0,ImGui::GetFrameHeight()})" not in (ROOT / "cpp" / file).read_text(encoding="utf8")
    gate = (ROOT / "cpp/gui_output_gate.hpp").read_text(encoding="utf8")
    notices = (ROOT / "cpp/gui_notifications.hpp").read_text(encoding="utf8")
    assert "ImGuiWindowFlags_NoFocusOnAppearing" in notices
    assert "Export complete" not in gate
    assert "BringWindowToDisplayFront" not in gate and "raise_success_" not in gate
    assert "draw_activity" in notices and "ImGuiListClipper" in notices


def test_fix1_core_kinds_object_counts_and_create_only_guidance():
    gui = (ROOT / "cpp/gui.cpp").read_text(encoding="utf8")
    for kind in ["GeoJSON", "Object measurements CSV", "ROI measurements CSV", "Tissue measurements CSV", "Cell table CSV"]:
        assert '"'+kind+'"' in gui
    assert 'objects?"Scope: %s / %llu objects":"Scope: %s / %llu rows"' in gui
    assert "output.kind=kind;" in gui
    gate = (ROOT / "cpp/gui_output_gate.hpp").read_text(encoding="utf8")
    assert gate.index("pending_->kind.c_str()") < gate.index('ui::TextUnformatted("Export destination")')
    for file in ["gui_tma.hpp", "gui_spatial.hpp"]:
        source = (ROOT / "cpp" / file).read_text(encoding="utf8")
        assert 'ui::i18n::text("This file already exists and this export never replaces a file. Choose a new file name.")' in source
        assert '"export commit failed (destination may already exist)"' not in source

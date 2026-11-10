"""Guard the shared GUI export routes; behavioral coverage is native."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def test_gui_exports_share_guarded_dispatch():
    gui = (ROOT / "cpp/gui.cpp").read_text(encoding="utf8")
    assert "auto begin_export=" in gui and "auto dispatch_export=" in gui
    assert "save_geojson(document," not in gui
    assert "save_measurements_csv(document," not in gui
    assert "cell_table_job.request(" not in gui
    assert "measurement_job.request(" not in gui
    assert "ExportWritePolicy::CreateNew" in gui
    assert 'ImGui::IsKeyPressed(ImGuiKey_F6,false))begin_export' in gui
    assert "GuiInputTarget::ConfirmReplaceExport" in gui
    assert "same_export_destination" in gui
    assert "interchange_import_path" in gui

def test_sampling_caption_and_run_share_native_builders():
    gui = (ROOT / "cpp/gui.cpp").read_text(encoding="utf8")
    assert "tissue.request(build_tissue_request())" in gui
    assert "cell_job.request(build_cell_request())" in gui
    assert "sampling_summary(build_tissue_request())" in gui
    assert "sampling_summary(build_cell_request()" in gui
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
    assert "if(measurement_key || export_measurement_image" in gui
    assert "if(!measurement_export_status.error.empty())ui::TextWrapped" in gui

def test_hidden_delete_requires_a_fresh_key_press():
    gui = (ROOT / "cpp/gui.cpp").read_text(encoding="utf8")
    assert "has_image && ImGui::IsKeyPressed(ImGuiKey_Delete,false)) delete_selection();" in gui

from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def test_image_panel_is_flat_and_edits_are_explicit():
    gui=(ROOT/"cpp/gui.cpp").read_text(encoding="utf8")
    image=gui[gui.index("        std::string image_name;"):gui.index('int list_filter=')]
    assert "CollapsingHeader" not in image
    assert 'Display mode###Channels' in image
    assert 'calibration_editor.open(document,active_image,workspace_epoch)' in image
    editor=(ROOT/"cpp/gui_calibration_editor.hpp").read_text(encoding="utf8")
    assert 'document->calibration_revision()==revision_' in editor
    assert 'document->set_calibration(width_,height_)' in editor
    assert 'document->clear_calibration()' in editor
    assert 'DialogAction::Cancel' in editor
    assert 'if(overview.state()=="failed")' in image

def test_saved_stains_remain_explicit_and_do_not_require_roi():
    gui=(ROOT/"cpp/gui.cpp").read_text(encoding="utf8")
    workflow=gui.index("        workflow_navigation();")
    saved=gui.index('ui::SeparatorText("Image analysis settings")',workflow)
    roi=gui.index("        if (selected) {",saved)
    assert workflow<saved<roi
    assert 'ui::Button("Use saved stains for analysis")' in gui[saved:roi]

def test_image_information_is_owned_and_read_only():
    gui=(ROOT/"cpp/gui.cpp").read_text(encoding="utf8")
    body=gui[gui.index("const bool image_info_owner="):gui.index("        if(language_requested)")]
    assert "workspace_epoch" in body and "active_document" in body
    assert "Imported measurements" in body
    assert "historical metadata, not live ROI measurements" in body
    assert "document.set_" not in body
    assert "image_info_active ||" in gui

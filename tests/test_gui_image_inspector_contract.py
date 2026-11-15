from pathlib import Path
from gui_contract_calls import has_call, compact
from gui_command_inventory import calls
ROOT=Path(__file__).resolve().parents[1]

def test_recovery_field_order_and_pathfield_owned_widths():
    gui=(ROOT/"cpp/gui.cpp").read_text(encoding="utf8")
    start=gui.index('if(project_settings_section==2) {')
    end=gui.index('if(entry_editor.draw(',start)
    recovery=gui[start:end]
    ordered=[
        'Recovery preserves unsaved work separately from Save project.',
        'ui::InputDouble("Interval (seconds)"',
        'ui::FieldHelp("0 disables periodic recovery.")',
        'paths.autosave.draw(',
        'ui::icon_action("Save recovery now (F10)"',
        'ui::Checkbox("Remove old inactive checkpoints"',
        'ui::CaptionedStrip checkpoints("Recovery checkpoints",3,0,true)',
        'ui::icon_action("Find recovery files (Ctrl+Shift+F5)"',
        'ui::icon_action("Open recovery (F11)"',
        'ui::icon_action("Delete selected checkpoint..."',
        'for(const auto& item:recovery_candidates)',
        'paths.recovery.draw(path_context,false,{},{},"Checkpoint file")',
        'paths.recovery_primary.draw(path_context,false,{},{},"Primary location (optional)")',
        'if(autosave.busy() && autosave_stats.checkpoint_published)',
        'if(maintenance_job.valid()) ui::TextDisabled',
    ]
    positions=[recovery.index(item) for item in ordered]
    assert positions==sorted(positions)
    assert 'Select a checkpoint or choose a checkpoint file.' in recovery
    assert 'checkpoints.destructive()' in recovery
    assert 'ui::SetNextItemWidth(240); paths.recovery_primary.draw' not in gui
    assert 'ui::SetNextItemWidth(240);paths.fingerprint_input.draw' not in gui

def test_project_forms_and_drop_callback_keep_private_owners():
    gui=(ROOT/"cpp/gui.cpp").read_text(encoding="utf8")
    forms=(ROOT/"cpp/gui_project_forms.hpp").read_text(encoding="utf8")
    paths=(ROOT/"cpp/gui_core_paths.hpp").read_text(encoding="utf8")
    for label, hint in [("##Project path", "Destination .qpxproj file"),
                        ("##Open project path", "Project file to open"),
                        ("##Image source", "Image file to add or relink"),
                        ("##QuPath destination", "New native project destination")]:
        assert f'"{label}"' in forms and f'"{hint}"' in forms
        assert f'InputTextWithHint("{label}"' not in gui
    for label in ["##Autosave path", "##Restore recovery", "##Recovery primary",
                  "##Import source", "##Interchange path", "##Cell table path",
                  "##Measurement destination", "Model file", "##Fingerprint input",
                  "##Fingerprint output"]:
        assert f'"{label}"' in paths
        assert f'InputTextWithHint("{label}"' not in gui
    assert gui.count("glfwSetDropCallback(")==1
    callback=gui[gui.index("glfwSetDropCallback("):gui.index("glfwMakeContextCurrent(")]
    assert "glfwGetCursorPos" in callback and "emplace_back(paths[i])" in callback
    assert "add_image(" not in callback and "Project::open" not in callback
    assert "forms.process_one(" in gui and "Adding %zu of %zu" in forms
    assert "forms.draw_open(" in gui and "forms.draw_save(" in gui and "forms.draw_image(" in gui
    assert "project_load=std::async" in gui and "save_job.request" in gui

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
    saved=gui.index('ui::TextWrapped("Kept profile: %s"',workflow)
    roi=gui.index("        if (selected) {",saved)
    assert workflow<saved<roi
    assert has_call(gui[saved:roi],'ui::WorkflowAction','"Use saved stains for analysis"','ui::design::Action::Secondary')
    actions=[args for args in calls(gui[saved:roi],'ui::WorkflowAction') if args[0]=='"Use saved stains for analysis"']
    reasons=[args for args in calls(gui[saved:roi],'ui::WorkflowReason') if args[0]=='"Use saved stains for analysis"']
    assert len(actions)==len(reasons)==1 and len(actions[0])==3 and compact(actions[0][2])==compact(reasons[0][1])
    table=(ROOT/"cpp/gui_commands.hpp").read_text(encoding="utf8")
    assert '"Use profile",0,KeyScope::NotInText,false,"Use saved stains for analysis"' in table
    # The shortened action retains its original ID, explicit native apply and target.
    assert has_call(gui[saved:roi],'inputs.use_stains','settings->basis()','settings->white()')
    assert has_call(gui[saved:roi],'mark_target','GuiInputTarget::UseImageStains')

def test_image_information_is_owned_and_read_only():
    gui=(ROOT/"cpp/gui.cpp").read_text(encoding="utf8")
    body=gui[gui.index("const bool image_info_owner="):gui.index("        if(language_requested)")]
    assert "workspace_epoch" in body and "active_document" in body
    assert "Imported measurements" in body
    assert "historical metadata, not live ROI measurements" in body
    assert "document.set_" not in body
    assert "image_info_active ||" in gui

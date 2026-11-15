"""Bidirectional membership and the single command-key authority."""
from pathlib import Path
import re
from gui_command_inventory import inventory
from gui_contract_calls import has_call
import pytest

ROOT=Path(__file__).resolve().parents[1]


def test_every_workflow_original_has_exactly_one_row_and_no_orphans():
    sources={p.as_posix():p.read_text(encoding='utf8') for p in (ROOT/'cpp').glob('gui*') if p.suffix in ['.hpp','.cpp']}
    originals=set(inventory(sources, originals_only=True))
    table=(ROOT/'cpp/gui_commands.hpp').read_text(encoding='utf8')
    rows=re.findall(r'CommandInfo\{CommandId::Action_\w+,"[^"]*",[^\n]+,"([^"]*)"(?:,CommandId::\w+)?\}',table)
    assert len(rows)==len(set(rows)), 'Duplicate workflow original'
    assert originals==set(rows), (originals-set(rows),set(rows)-originals)


def test_readers_use_static_authority_and_dispatch_is_installed():
    commands=(ROOT/'cpp/gui_commands.hpp').read_text(encoding='utf8')
    assert 'struct Availability {bool enabled=true;const char* reason=nullptr;};' in commands
    assert 'command_table.size()==static_cast<std::size_t>(CommandId::Count)' in commands
    assert 'static_cast<std::size_t>(command_table[i].id)!=i' in commands
    assert 'Action_Run_batch_Ctrl_Alt_B,"Run batch",0,' in commands
    assert 'Action_Cancel_analysis,"Cancel",0,KeyScope::NotInText,false' in commands
    assert 'Action_Cancel_tissue,"Cancel",0,KeyScope::NotInText,false' in commands
    assert 'CommandId chord_owner=CommandId::Count;' in commands
    assert 'mutable std::array<std::uint8_t,command_table.size()> evaluated' in commands
    assert 'bool chord_pressed(CommandId id,const CommandFrame& frame)' in commands
    for filename,marker in [('gui_header.hpp','header_commands.size()'),('gui_shell.hpp','shell_commands.size()'),('gui_tool_catalog.hpp','tool_commands.size()')]:
        assert marker in (ROOT/'cpp'/filename).read_text(encoding='utf8')
    workspace=(ROOT/'cpp/gui_workspace.hpp').read_text(encoding='utf8')
    assert 'ActivityButton(activity_label(i)' in workspace
    assert 'legacy?legacy+3:panes[index]' in workspace


def test_inventory_rejects_unresolved_original():
    for call in ['WorkflowAction(unknown)', 'WorkflowIcon(unknown,ui::Icon::Stop)',
                 'WorkflowAction(compute("Analyze region (Ctrl+M)"))',
                 'WorkflowAction("Analyze region (Ctrl+M)" + suffix)']:
        with pytest.raises(AssertionError):inventory({'gui_new.hpp':call},originals_only=True)
    with pytest.raises(AssertionError):
        inventory({'gui_tma.hpp':'WorkflowAction(text,action); button(unknown,GuiInputTarget::TmaRemoveRow,false);'},originals_only=True)


def test_changed_caption_has_fifteen_translations():
    catalogue=(ROOT/'cpp/gui_i18n_catalog.inc').read_text(encoding='utf8')
    # Existing titles reuse catalogue translations; every new dispatcher caption
    # must have the same complete fifteen-locale coverage.
    table=(ROOT/'cpp/gui_commands.hpp').read_text(encoding='utf8')
    keys=['Exit','Spatial...','Batch analysis...','Project density...','Raw channels...','Saved stream files']
    keys+=re.findall(r'CommandInfo\{CommandId::Key_\w+,"([^"]*)"',table)
    for key in keys:
        row=re.search(r'^\{"'+re.escape(key)+r'", \{([^\n]+)\}\},',catalogue,re.M)
        assert row,key
        assert len(re.findall(r'"(?:[^"\\]|\\.)*"',row[1]))==15,key


def test_all_legacy_enum_domains_have_one_row_each():
    table=(ROOT/'cpp/gui_commands.hpp').read_text(encoding='utf8')
    for filename,enum,prefix in [('gui_header.hpp','HeaderCommand','Header'),('gui_shell.hpp','ShellCommand','Shell'),('gui_tool_catalog.hpp','ToolId','Tool'),('gui_tool_catalog.hpp','ToolChild','Child')]:
        source=(ROOT/'cpp'/filename).read_text(encoding='utf8')
        domain=set(re.search(r'enum class '+enum+r'\s*\{([^}]+)\}',source)[1].replace('\n','').replace(' ','').split(',')) - {'Count'}
        rows=re.findall(r'CommandInfo\{CommandId::'+prefix+r'_(\w+),"[^"]*",[^\n]+,"([^"]*)"(?:,CommandId::\w+)?\}',table)
        assert len(rows)==len(domain) and {name for name,_ in rows}==domain
        assert all(name==legacy for name,legacy in rows)


# Exact source lines, not entire exempt files or functions. Additional predicates
# on these lines also fail closed through the exact comparison below.
EXEMPT_COMMAND_KEYS={
    'gui.cpp': {
        '!native_overlay && ImGui::IsKeyPressed(ImGuiKey_Tab),!native_overlay && ImGui::IsKeyPressed(ImGuiKey_Enter),!native_overlay && ImGui::IsKeyPressed(ImGuiKey_Space),io.KeyShift,io.KeyCtrl});': 'shell focused-control navigation',
        'const bool open_key=ImGui::IsItemFocused() && !io.WantTextInput && !editing_blocked && ImGui::IsKeyPressed(ImGuiKey_Enter);': 'focused project row activation',
        'const bool cancelled=ImGui::IsKeyPressed(ImGuiKey_Escape);': 'inline rename editing',
        'if(io.KeyShift) {const bool anchor_shown=std::find(list_ids.begin(),list_ids.end(),list_anchor)!=list_ids.end();detail::select_ordered_range(*selection,list_ids,list_anchor,a.id,io.KeyCtrl);if(!anchor_shown)list_anchor=a.id;}': 'object-list mouse range selection',
        'if(io.KeyCtrl) selection->toggle(a.id); else selection->replace({a.id});': 'object-list mouse selection',
        'if(io.KeyShift) { marquee=true; marquee_anchor=p; marquee_additive=io.KeyCtrl; }': 'canvas mouse marquee',
        'if(hits.empty()) { if(!io.KeyCtrl) selection->clear(); }': 'canvas mouse selection',
        'else if(io.KeyCtrl) selection->toggle(hits.back()); else selection->replace({hits.back()});': 'canvas mouse selection',
    },
    'gui_dialogs.hpp': {
        'if(state.busy || !state.foreground || !ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::GetIO().WantTextInput)return false;': 'registry-owned dialog Escape',
    },
}
COMMAND_PREDICATE=re.compile(r'\b(?:IsKeyPressed|IsKeyChordPressed|Shortcut)\s*\(|(?:\bio|ImGui::GetIO\(\))\.Key(?:Ctrl|Shift|Alt|Super)\b')


def assert_single_key_authority(sources):
    for filename,source in sources.items():
        if filename=='gui_commands.hpp':continue
        exemptions=EXEMPT_COMMAND_KEYS.get(filename,{})
        for line in source.splitlines():
            if COMMAND_PREDICATE.search(line):
                assert line.strip() in exemptions, f'Command key bypass in {filename}: {line.strip()}'


def test_no_command_key_bypasses_dispatcher():
    assert_single_key_authority({p.name:p.read_text(encoding='utf8') for p in (ROOT/'cpp').glob('gui*') if p.suffix in ['.hpp','.cpp']})


def test_replay_keys_request_commands_with_only_control_key_injection():
    gui=(ROOT/'cpp/gui.cpp').read_text(encoding='utf8')
    for branch in re.findall(r'case GuiInputKind::\w+Key:[^\n]+',gui):
        assert 'requested_command=' in branch and 'AddKeyEvent' not in branch,branch
    assert 'if(event.kind==GuiInputKind::EscapeKey)replay_controls.enqueue(requested_command,ImGuiKey_Escape);' in gui
    assert 'else if(event.kind==GuiInputKind::FinishKey)replay_controls.enqueue(requested_command,ImGuiKey_Enter);' in gui
    assert gui.count('replay_controls.enqueue(')==2
    path=gui[gui.index('auto replay_emit='):gui.index('auto suspend_window=')]
    assert re.findall(r'input.AddKeyEvent\(([^;]+)\);',path)==['ImGuiMod_Ctrl,true','ImGuiMod_Shift,true']
    assert path.index('input.AddMousePosEvent')<path.index('if(requested_command!=')<path.index('if(event.ctrl)')
    assert 'else command_inputs.request(requested_command);' in path
    commands=(ROOT/'cpp/gui_commands.hpp').read_text(encoding='utf8')
    assert 'if(key->Down && key->DownDuration==0)frame.request(it->command);' in commands
    assert 'if(queued){++it;continue;}' in commands
    assert gui.count('command_inputs.context=ui::chord_context(')==1
    assert 'CommandFrameScope command_scope(command_inputs)' in gui


def test_table_ownership_cancel_and_siblings():
    source=(ROOT/'cpp/gui_commands.hpp').read_text(encoding='utf8')
    rows={m[0]:m[1:] for m in re.findall(r'CommandInfo\{CommandId::(\w+),"([^"]*)",(.*?),KeyScope::(\w+),(true|false),"([^"]*)"(?:,CommandId::(\w+))?(?:,true)?\}',source)}
    chords=[]
    for id,(caption,chord,scope,repeat,legacy,owner) in rows.items():
        if owner and owner!='Count':assert chord=='0' and rows[owner][-1]==''
        if chord!='0':chords.append(chord)
        if ('Cancel' in id or 'Stop' in id) and id!='Key_EscapeCanvas':assert chord=='0',id
        assert repeat=='false',id
    assert len(chords)==len(set(chords))
    assert {id for id,row in rows.items() if row[1]!='0' and row[2]=='Global'}=={
        'Header_OpenProject','Header_SaveProject','Header_ProjectPanel','Header_ImagePanel',
        'Header_AnnotationsPanel','Header_WorkflowPanel','Header_ChooseLanguage','Header_QuickReference'}
    assert {id for id,row in rows.items() if row[2]=='Canvas'}=={
        'Key_SelectAll','Key_Delete','Key_EscapeCanvas','Key_FinishDraft','Key_InsertVertex',
        'Action_Apply_Boolean_B','Action_Merge_annotations_M'}
    for run,add in [('Action_Trace_contours_Ctrl_J','Action_Add_annotations_Ctrl_Shift_J'),('Action_Detect_cells_Ctrl_G','Action_Add_cells_Ctrl_Shift_G')]:
        assert set(rows[add][1].split('|'))==set(rows[run][1].split('|'))|{'ImGuiMod_Shift'}


def test_reasons_are_catalogue_keys_and_wrappers_keep_disabled_tooltips():
    """Rendered counterparts: --reasons-help and --milestone3-review."""
    files=['gui_workflow_ui.hpp','gui_object_classifier.hpp','gui_pixel_classifier.hpp',
           'gui_raw.hpp','gui_raw_pixel.hpp','gui_fluorescence.cpp','gui_dark_flat.cpp',
           'gui_linear_unmixing.cpp','gui_marker_rules.cpp','gui_analysis_tasks.hpp','gui_batch.hpp',
           'gui_registration.cpp','gui_genes.hpp','gui_tma.hpp']
    source='\n'.join((ROOT/'cpp'/name).read_text(encoding='utf8') for name in files)
    keys=set(re.findall(r'(?:return |Availability)\{false,"([^"]+)"',source))
    keys.update(re.findall(r'return unavailable\("([^"]+)"',source))
    gui=(ROOT/'cpp/gui.cpp').read_text(encoding='utf8')
    table=gui[gui.index('const auto tables_availability='):gui.index('auto export_image_cells=')]
    keys.update(re.findall(r'return \{false,"([^"]+)"',table))
    keys.update(['Finish the current dialog first.','Wait for the current work to finish.',
                 'Keep a cell run for the selected annotation before exporting kept cells.',
                 'Select an annotation with a kept cell run.',
                 'Hash the file first.',
                 'White reference kept. Native RGB8 stain application requires each white channel in 1..255.',
                 'The selected correction no longer matches this image. Choose it again in Dark/flat correction.',
                 'Choose a correction in Dark/flat correction, or turn off Use selected dark/flat correction.'])
    rendered=(ROOT/'cpp/tests/gui_reasons_help_checks.hpp').read_text(encoding='utf8')
    listed=rendered[rendered.index('reason_keys[]={'):rendered.index('inline void inspect')]
    assert set(re.findall(r'"([^"\n]+)"',listed))==keys
    catalogue=(ROOT/'cpp/gui_i18n_catalog.inc').read_text(encoding='utf8')
    for key in keys:
        row=re.search(r'^\{"'+re.escape(key)+r'", \{([^\n]+)\}\},',catalogue,re.M)
        assert row, key
        values=re.findall(r'"((?:[^"\\]|\\.)*)"',row[1])
        assert len(values)==15, key
        conversions=re.findall(r'%(?:[-+ #0]*\d*(?:\.\d+)?[hlzjtL]*[diuoxXfFeEgGaAcspn%])',key)
        assert all(re.findall(r'%(?:[-+ #0]*\d*(?:\.\d+)?[hlzjtL]*[diuoxXfFeEgGaAcspn%])',value)==conversions for value in values), key
    wrappers=(ROOT/'cpp/gui_workflow_ui.hpp').read_text(encoding='utf8')
    assert 'ImGuiHoveredFlags_AllowWhenDisabled' in wrappers
    assert 'std::string reason,inline_reason' in wrappers
    assert 'trace_workflow_action(original,caption,role,availability)' in wrappers
    for name in files[1:]:
        assert 'availability(' in (ROOT/'cpp'/name).read_text(encoding='utf8'),name


def test_help_is_generated_from_owning_rows_and_cached_by_language():
    source=(ROOT/'cpp/gui_help.hpp').read_text(encoding='utf8')
    assert 'for(const auto& row:ui::command_table)' in source
    assert 'row.chord && row.chord_owner==ui::CommandId::Count' in source
    assert 'language!=ui::i18n::state.effective' in source
    assert 'ui::chord_text(row.chord)+": "+shortcut_caption(row)' in source
    for key in ['Right drag: pan','Scroll: zoom','Ctrl+click: toggle selection','Shift+drag: select box','Double-click: finish shape']:
        assert has_call(source,'ui::TextWrapped','"'+key+'"')
    for key in ['R: rectangle','Ctrl+A: select all','Escape: cancel','Enter or double-click: finish shape']:
        assert '"'+key+'"' not in source


def test_raw_missing_identifier_lookup_is_revision_scoped():
    source=(ROOT/'cpp/gui_raw_pixel.hpp').read_text(encoding='utf8')
    assert 'if(!lookup_valid_ || lookup_record_!=record_ || lookup_revision_!=doc.revision())' in source
    assert 'lookup_record_=record_;lookup_revision_=doc.revision();lookup_valid_=true;' in source
    assert 'if(record_==matched_record_)record_=0;matched_record_=0;' in source


def test_cells_export_alias_shares_its_kept_run_answer():
    """Rendered counterpart: --reasons-help, including kept/Live source."""
    tasks=(ROOT/'cpp/gui_analysis_tasks.hpp').read_text(encoding='utf8')
    gui=(ROOT/'cpp/gui.cpp').read_text(encoding='utf8')
    assert tasks.count('return {false,"In Tables, choose kept runs in Cell source."};')==1
    assert 'return kept_cells_export_availability(document,selected,cell_table_options,' in tasks
    assert 'if(kind==0 && scope==0)return detail::analysis_tasks::kept_cells_export_availability(' in gui
    assert 'editing_blocked,for_cells)' in gui
    assert 'const bool image_key_for_cells=workspace.enabled() && workspace.workflow_task()==Task::Cells;' in gui
    assert 'const bool image_cells_key=ui::chord_pressed(ui::CommandId::Action_Export_image_cells_Ctrl_Alt_X,ui::frame_commands());' in gui
    assert 'tables_availability(0,0,image_key_for_cells)' in gui
    assert 'project_cells || cancel_cells_table || image_cells_key' in gui


def test_milestone4_review_batch_handoff_and_explicit_table_context():
    """Rendered counterparts: --milestone4-review and --dispatcher cancel/feedback."""
    batch=(ROOT/'cpp/gui_batch.hpp').read_text(encoding='utf8')
    assert 'auto [start,cancel]=take_requests(blocked);' in batch
    assert 'if(cancel) job_.cancel();' in batch
    take=batch[batch.index('std::pair<bool,bool> take_requests'):batch.index('std::pair<bool,bool> take_requests')+750]
    assert 'const bool run=!blocked && ui::chord_pressed(' in take
    assert 'const bool cancel=ui::batch_cancel_requested(frame);' in take
    assert 'frame.evaluated' not in take and 'frame.fired' not in take
    commands=(ROOT/'cpp/gui_commands.hpp').read_text(encoding='utf8')
    assert 'inline bool batch_cancel_requested(const CommandFrame& frame)' in commands
    gui=(ROOT/'cpp/gui.cpp').read_text(encoding='utf8')
    table=gui[gui.index('const auto tables_availability='):gui.index('auto export_image_cells=')]
    assert 'bool for_cells=false' in table and 'workspace.' not in table
    assert 'const auto available=tables_availability(table_kind,table_scope);' in gui
    for id in ['Action_Hash_file_Ctrl_Shift_F','Action_Review_for_project_Ctrl_Alt_F']:
        assert 'files_availability(ui::CommandId::'+id+')' in gui

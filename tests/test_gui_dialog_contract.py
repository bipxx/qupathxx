"""Enforce the native modal boundary; behavioral invariants live in native tests."""
from pathlib import Path
import re
import json
from gui_contract_calls import has_call, compact
from gui_command_inventory import calls

ROOT = Path(__file__).resolve().parents[1]


def inventory():
    text = (ROOT / 'cpp/gui_dialog_contract.hpp').read_text(encoding='utf8')
    rows=[]
    for args in calls(text,'X'):
        assert len(args)==5, ('Malformed dialog row',args)
        row=[compact(arg) for arg in args]
        rows.append((row[0],json.loads(row[1]),row[2],json.loads(row[3]),row[4]))
    return rows


def gui_sources():
    return {p.name: p.read_text(encoding='utf8') for p in (ROOT / 'cpp').glob('gui*')
            if p.suffix in ('.cpp', '.hpp')}


def bypasses(sources):
    findings = []
    for name, text in sources.items():
        if name == 'gui_dialogs.hpp':
            continue
        if re.search(r'ImGui::BeginPopupModal\s*\(|ui::BeginPopupModal\s*\(', text):
            findings.append((name, 'raw modal'))
        if re.search(r'ui::(?:dialog_detail::|BeginDialog\s*\(|DialogHeader\s*\(|DialogFooter\b|DialogBody\b|DialogCancelButton\s*\()', text):
            findings.append((name, 'frame bypass'))
        if name != 'gui_layout.hpp' and 'ImGui::CloseCurrentPopup(' in text:
            findings.append((name, 'unowned popup dismissal'))
    return findings


def test_all_modals_are_registered_at_their_owning_presenter():
    rows = inventory()
    assert rows and len({r[0] for r in rows}) == len(rows)
    assert len({r[1] for r in rows}) == len(rows), 'Stable modal identities must be unique'
    sources = gui_sources()
    for kind, stable, layout, owner, scope in rows:
        assert owner in sources and f'DialogKind::{kind}' in sources[owner], (kind, owner)
        assert has_call(sources[owner],'ui::RenderDialog') and has_call(sources[owner],'ui::OpenDialog')
        for name, text in sources.items():
            if name not in ('gui_dialogs.hpp', 'gui_dialog_contract.hpp', owner):
                assert f'DialogKind::{kind}' not in text, (kind, name, owner)


def test_no_modal_can_bypass_the_registered_renderer():
    assert bypasses(gui_sources()) == []


def test_guard_detects_raw_frames_and_unowned_dismissal():
    samples = {'gui_bad.hpp': 'ImGui::BeginPopupModal("New modal"); ui::DialogFooter actions({"Save"}); ImGui::CloseCurrentPopup();'}
    assert {reason for _, reason in bypasses(samples)} == {'raw modal', 'frame bypass', 'unowned popup dismissal'}


def test_action_definitions_have_one_production_owner():
    for name, text in gui_sources().items():
        if name in ('gui_dialog_contract.hpp', 'gui_dialogs.hpp'):
            continue
        assert not re.search(r'\b(?:DialogActionDefinition|DialogActionRole|DialogCompletion|DialogActionLayout)\b', text), name


def test_popup_menus_cannot_bypass_shared_spacing_and_bounds():
    for name, text in gui_sources().items():
        if name == 'gui_layout.hpp':
            continue
        assert not re.search(r'ImGui::(?:MenuItem|BeginMenu|BeginPopup|BeginPopupContextItem|BeginPopupContextWindow)\s*\(', text), name


def test_record_removal_has_registered_record_ownership():
    assert ('RemoveKeptResult', 'Remove kept result?', 'Confirmation',
            'gui_record_removal.hpp', 'Record') in inventory()


def test_folder_variants_belong_only_to_the_file_picker():
    text = (ROOT / 'cpp/gui_dialog_contract.hpp').read_text(encoding='utf8')
    allowed = text.split('inline bool dialog_variant_allowed', 1)[1].split('inline std::vector<DialogVariant>', 1)[0]
    picker = allowed.split('case DialogKind::ImportFile:', 1)[1].split('default:', 1)[0]
    for variant in ('Folder', 'NewFolder'):
        assert f'variant==DialogVariant::{variant}' in picker
        assert f'DialogVariant::{variant}' not in allowed.split('case DialogKind::ImportFile:', 1)[0]
        assert f'DialogVariant::{variant}' in text.split('inline std::vector<DialogVariant>', 1)[1].split('inline void validate_dialog_actions', 1)[0]
    assert '"Choose folder"' in text and '"Choose output folder"' in text

"""Guard command ownership while native input tests exercise header behavior."""
from pathlib import Path
import re
from gui_contract_calls import has_call
from gui_command_inventory import code_mask

ROOT = Path(__file__).resolve().parents[1]


def sources():
    return ((ROOT / "cpp/gui.cpp").read_text(encoding="utf8"),
            (ROOT / "cpp/gui_header.hpp").read_text(encoding="utf8"))


def test_header_commands_keep_their_existing_application_handlers():
    gui, header = sources()
    enum = re.search(r"enum class HeaderCommand\s*\{([^}]+)\}", header).group(1)
    commands = set(re.findall(r"\b[A-Z]\w*\b", enum))
    table = (ROOT / "cpp/gui_commands.hpp").read_text(encoding="utf8")
    expected = set(re.findall(r'CommandInfo\{CommandId::Header_(\w+),', table))
    assert commands == expected
    entries = set(re.findall(r'item\(HeaderCommand::(\w+),', header))
    assert entries == expected, "Every registered header command needs a menu entry"
    dispatch = gui[gui.index("const ui::HeaderDispatch header_dispatch="):gui.index("ui::draw_header(header,")]
    cases = re.findall(r"case Command::(\w+):([^;]+;[^\n]*)", dispatch)
    assert {name for name, _ in cases} == expected
    handlers = dict(cases)
    for name, call in {
        "Undo": "history_action(false)", "Redo": "history_action(true)",
        "CopyObjects": "copy_selection()", "PasteObjects": "paste_clipboard(false)",
        "ResetLayout": "workspace.reset()", "ReloadUi": "shell->reload()",
        "SaveProject": "save_project_requested=true", "ChooseLanguage": "language_requested=true",
    }.items():
        assert call in handlers[name], (name, handlers[name])
    assert has_call(gui,'ui::ToolMenus')
    tools = (ROOT / "cpp/gui_tool_catalog.hpp").read_text(encoding="utf8")
    assert re.search(r'ToolDefinition\{ToolId::Tma,[^\n]+ToolGroup::Project', tools)


def test_header_groups_and_shortcuts_have_one_private_owner():
    gui, header = sources()
    assert has_call(gui,'ui::draw_header')
    assert has_call(gui,'ui::draw_header_settings')
    assert has_call(gui,'ui::header_shortcuts')
    for caption in ["File", "Edit", "View", "Analyze", "Help", "Language", "TMA"]:
        assert f'ui::BeginMenu("{caption}"' not in gui
    assert 'BeginMenu("Language"' not in header
    assert 'BeginMenu("TMA"' not in header
    for caption in ["Panels", "Layout", "General", "Appearance", "Advanced"]:
        assert f'menu("{caption}"' in header
    assert 'if(!s.workspace)' in header and 'menu("Workspace settings"' in header
    assert "ui::chord_pressed(ui::CommandId::Header_ChooseLanguage,ui::frame_commands())" in header
    assert "ui::chord_pressed(ui::CommandId::Header_QuickReference,ui::frame_commands())" in header
    assert 'if(header_keys.language)language_requested=true' in gui
    assert 'if(header_keys.help)help_window.request(detail::HelpPage::QuickReference)' in gui
    assert 'shell->reload()' in gui and 'ui::CommandId::Header_ReloadUi' in gui


def test_help_is_one_read_only_registered_modal_with_explicit_links():
    gui, header = sources()
    registry = (ROOT / "cpp/gui_dialog_contract.hpp").read_text(encoding="utf8")
    assert registry.count('X(Help,"Help",Selector,"gui_header.hpp",Session)') == 1
    assert has_call(header,'ui::RenderDialog','ui::DialogKind::Help')
    help_owners=re.findall(r'\bGuiHelpWindow\s+(\w+)\s*;',code_mask(gui))
    assert len(help_owners)==1 and has_call(gui,help_owners[0]+'.draw','*build_environment()')
    assert 'BeginPopupModal' not in header
    help_body = (ROOT / "cpp/gui_help.hpp").read_text(encoding="utf8")
    assert "environment.app_version" in help_body and "environment.source_tree_sha256" in help_body
    assert "if(pressed)" in help_body
    assert help_body.index("if(pressed)") < help_body.index("Platform_OpenInShellFn")
    assert not re.search(r"\b(?:system|ShellExecute|CreateProcess)\s*\(", help_body)
    assert "document.set_" not in help_body and "project->" not in help_body

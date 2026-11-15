"""Notification routing is presentation-only and catalogue sources are enforced."""
from pathlib import Path
import re
from gui_contract_calls import has_call
ROOT=Path(__file__).resolve().parents[1]

def test_shared_failure_string_is_gone():
    gui=(ROOT/'cpp/gui.cpp').read_text(encoding='utf8')
    body=gui[gui.index('GuiReport run_gui('):]
    assert 'std::string error;' not in body
    assert not re.search(r'(?<![.\w])error\s*(?:=|\.clear\()',body)
    assert has_call(body,'observe_error','0','ui::workflow_task_labels[0]','mean.error')
    assert 'stain_model_io.operation()==detail::GuiStainModel::Operation::Save' in body
    tasks=(ROOT/'cpp/gui_analysis_tasks.hpp').read_text(encoding='utf8')
    assert 'std::string& error;' not in tasks and 'context.error' not in tasks
    assert not re.search(r'(?<![.\w])error\s*(?:=|\.clear\()',tasks)

def test_notice_sources_have_catalogue_rows():
    catalogue=(ROOT/'cpp/gui_i18n_catalog.inc').read_text(encoding='utf8')
    keys=set(re.findall(r'^\{"([^"]+)"',catalogue,re.M))
    commands=dict(re.findall(r'CommandInfo\{CommandId::(\w+),"([^"]+)"',(ROOT/'cpp/gui_commands.hpp').read_text(encoding='utf8')))
    for name in ['Tool_PixelClassifier','Tool_ObjectClassifier','Tool_RawPixelClassifier','Tool_Fluorescence','Tool_Spatial','Tool_SpatialImport','Tool_Batch','Tool_Tma','Tool_Density','Tool_Channels','Header_ProjectPanel']:
        assert commands[name] in keys,(name,commands[name])
    labels=re.search(r'workflow_task_labels=\{([^}]+)\}',(ROOT/'cpp/gui_workflow.hpp').read_text(encoding='utf8')).group(1)
    assert set(re.findall(r'"([^"]+)"',labels)) <= keys
    for p in (ROOT/'cpp').glob('gui*'):
        if p.suffix not in ['.cpp','.hpp']:continue
        source=p.read_text(encoding='utf8')
        for key in re.findall(r'post\(\{(?:ui::)?Severity::\w+,"([^"]+)"',source):assert key in keys,(p.name,key)
        for key in re.findall(r'exception_notice\("([^"]+)"',source):assert key in keys,(p.name,key)
        for key in re.findall(r'NoticeException<std::\w+>\("([^"]+)"',source):assert key in keys,(p.name,key)
    notices=(ROOT/'cpp/gui_notifications.hpp').read_text(encoding='utf8')
    assert 'notice.source==row.english' in notices
    assert 'if(source==std::end(i18n::catalogue))throw' in notices
    for key in ['Activity','Dismiss error','Information','Success','Warning','Error','Exported.','The operation completed.','The operation started.','The operation failed.']:
        assert key in keys

def test_bounded_session_history_and_passive_export():
    notices=(ROOT/'cpp/gui_notifications.hpp').read_text(encoding='utf8')
    # notifications_checks::model asserts newest 50 of 60 and every row's order.
    assert 'ImGuiListClipper' in notices and 'if(!*open)return;' in notices
    assert has_call(notices,'ImGui::SetClipboardText','entry.notice.path.c_str()')
    gate=(ROOT/'cpp/gui_output_gate.hpp').read_text(encoding='utf8')
    assert 'Export complete' not in gate and '"Exported.",ui::notice_path(path),true' in gate
    assert 'std::time' not in notices[notices.index('void draw_status_bar'):]
    assert 'post(' not in notices[notices.index('void draw_status_bar'):notices.index('struct NoticeJob')]

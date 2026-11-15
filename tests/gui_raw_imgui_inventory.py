"""Panel bypass inventory. Counts are parsed calls, independent of arguments."""
import re
from gui_command_inventory import calls, code_mask

# These implement shared presentation adapters rather than application panels.
ADAPTERS = {
    'gui_layout.hpp': 'Widget, field, text, menu and layout adapters own native ImGui calls.',
    'gui_design_system.hpp': 'Shared token/style application and diagnostic chrome.',
    'gui_icons.hpp': 'Shared icon rendering and button primitives.',
    'gui_dialogs.hpp': 'Registered modal renderer owns its shared frame and actions.',
    'gui_path_field.hpp': 'Shared path-input adapter owns native editing and picker integration.',
}
FUNCTION = re.compile(r'^(?:Button|SmallButton|Checkbox|CollapsingHeader|Combo|BeginCombo|Input\w*|TreeNode\w*|Selectable|BeginTabItem|TableSetupColumn|Text\w*|SetTooltip|MenuItem|BeginMenu)$')

def counts(source):
    names=set(re.findall(r'\bImGui\s*::\s*(\w+)\s*\(', code_mask(source)))
    return {name:sum(1 for _ in calls(source, 'ImGui::'+name))
            for name in sorted(names) if FUNCTION.fullmatch(name)}

def inventory(root):
    return {p.name: counts(p.read_text(encoding='utf8')) for p in sorted(root.glob('gui*'))
            if p.suffix in {'.cpp','.hpp'} and p.name not in ADAPTERS}

def check_ratchet(actual, recorded):
    for file in sorted(actual.keys() | recorded.keys()):
        for function in sorted(actual.get(file,{}).keys() | recorded.get(file,{}).keys()):
            now=actual.get(file,{}).get(function,0); before=recorded.get(file,{}).get(function,0)
            assert now <= before, f'{file}: ImGui::{function}: added raw call ({before} -> {now})'
            assert now >= before, f'{file}: ImGui::{function}: lower the file in gui-raw-imgui-allowlist.json ({before} -> {now})'

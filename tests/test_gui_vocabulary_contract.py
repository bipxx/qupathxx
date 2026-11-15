"""Visible source text shares the native catalogue/command vocabulary fixture."""
from pathlib import Path
import re
from gui_command_inventory import calls, literals, code_mask

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / 'tests/fixtures/gui-vocabulary.tsv'

def rules():
    rows = [line.split('\t', 2) for line in FIXTURE.read_text(encoding='utf8').splitlines()
            if line and not line.startswith('#')]
    assert all(len(row) == 3 and row[2] for row in rows)
    return [(name, re.compile(pattern, re.I)) for kind, name, pattern in rows if kind == 'R'], {
        text for kind, text, reason in rows if kind == 'A'}

def violations(text):
    text = text.split('##', 1)[0]
    patterns, allowed = rules()
    return [] if text in allowed else [name for name, pattern in patterns if pattern.search(text)]

def visible_literals(source):
    # Mask comments without altering strings, including URLs inside captions.
    source = re.sub(r'"(?:[^"\\]|\\.)*"|//[^\n]*|/\*.*?\*/',
                    lambda m: m[0] if m[0].startswith('"') else ' ', source, flags=re.S)
    # These adapters take a visible format/key first and a stable identifier
    # separately. Do not mistake the identifier for a caption in an outer call.
    source = re.sub(r'ui::presentation_label\([^,]+,\s*"(?:[^"\\]|\\.)*"\)', 'command_caption', source)
    source = re.sub(r'ui::unit_caption\(("(?:[^"\\]|\\.)*"),([^,]+),\s*"(?:[^"\\]|\\.)*"\)', r'ui::unit_text(\1,\2)', source)
    # Scope declarations use braces rather than a function-call parenthesis.
    # The shared balanced parser accepts their closing brace at the same depth;
    # only the opening token is synthesized, leaving all arguments unchanged.
    for match in re.finditer(r'\bui\s*::\s*ComboScope\s+\w+\s*\{', code_mask(source)):
        args = next(calls('scope('+source[match.end():], 'scope'), [])
        for expr in args[:2]:
            yield from literals(expr)
    # Local presentation adapters forward these arguments to the same sinks.
    # Workflow button adapters forward stable originals to the command table.
    for adapter in {'bound', 'value', 'area', 'count', 'heading', 'unit_label',
                    'vector_summary', 'memory', 'draw_record_list'}:
        for args in calls(source, adapter):
            if args:
                yield from literals(args[0])
    for args in calls(source, 'shape_measurement_row'):
        for arg in args:
            yield from literals(arg)
    # Named literal arrays used by text/column calls (phase labels and headers).
    for match in re.finditer(r'(?:const char\*|char const\*)\s+(\w+)\s*\[\]\s*=\s*\{([^}]+)\}', source):
        if re.search(r'(?:Text\w*|TableSetupColumn)\([^;]*\b'+match[1]+r'\b', source):
            yield from literals(match[2])
    for match in re.finditer(r'\b(ui|ImGui|i18n)::(\w+)\s*\(', source):
        namespace, name = match.groups()
        # Workflow originals are stable identifiers; the table owns their captions.
        if name in {'WorkflowAction', 'WorkflowIcon', 'icon_action', 'chord_pressed',
                    'WorkflowReason', 'mark', 'observe', 'PushID', 'GetID', 'field_input_id',
                    'trace_workflow_action', 'workflow_command', 'command_label', 'presentation_label'}:
            continue
        if namespace != 'ui' and not (name.startswith(('Text', 'Input', 'CompactInput', 'Slider', 'Drag')) or name in {
            'Button', 'AvailableButton', 'Checkbox', 'RadioButton', 'Combo', 'BeginCombo',
            'CollapsingHeader', 'Selectable', 'SeparatorText', 'TableSetupColumn', 'SetTooltip',
            'Begin', 'BeginTabItem', 'BeginMenu', 'MenuItem', 'text', 'label', 'unit_caption', 'unit_text', 'IconButton'}):
            continue
        args = next(calls(source[match.start():], name), [])
        if args:
            for value in literals(args[0]):
                yield value
            # Other direct string arguments can supply a visible conditional,
            # tooltip value or combo choice, as opposed to a printf conversion.
            if name.startswith('Text') or name in {'Combo', 'BeginCombo', 'SetTooltip'}:
                for expr in args[1:]:
                    yield from literals(expr)
            elif name == 'InputTextWithHint' and len(args)>1:
                yield from literals(args[1])

def test_frontend_visible_vocabulary():
    failures = []
    for path in sorted((ROOT/'cpp').glob('gui*')):
        if path.suffix not in {'.hpp', '.cpp'}:
            continue
        for value in visible_literals(path.read_text(encoding='utf8')):
            for rule in violations(value):
                failures.append(f'{path.name}: {rule}: {value}')
        # These operation gates feed their literals through the panels' caught
        # error channels rather than passing them directly to a text adapter.
        if path.name in {'gui_fluorescence.cpp', 'gui_raw.hpp', 'gui_pixel_classifier.hpp'}:
            source = path.read_text(encoding='utf8')
            for constructor in {'invalid_argument', 'logic_error', 'WorkflowInputChanged'}:
                for args in calls(source, constructor):
                    for value in literals(args[0]):
                        for rule in violations(value):
                            failures.append(f'{path.name}: error channel: {rule}: {value}')
    assert not failures, '\n'.join(failures)

def test_vocabulary_fixture_is_shared_with_native_check():
    source = (ROOT/'cpp/tests/gui_i18n_tests.cpp').read_text(encoding='utf8')
    assert 'gui-vocabulary.tsv' in source and '__FILE__' in source


def test_scoped_combo_captions_keep_vocabulary_coverage():
    values=list(visible_literals('if(ui::ComboScope picker {"Publish", "Choice"}; picker) {}'))
    assert 'Publish' in values
    assert violations('Publish')

def test_catalogue_vocabulary_and_printf_before_build():
    import json
    seen = set()
    conversions = lambda text: re.findall(r'%(?!%)[-+ #0-9.*hlzjtL]*[diuoxXfFeEgGaAcspn]', text)
    for line in (ROOT/'cpp/gui_i18n_catalog.inc').read_text(encoding='utf8').splitlines():
        if not line.startswith('{'):
            continue
        key, translated = json.loads(line.rstrip(',').replace('{', '[', 1).replace(', {', ', [', 1).removesuffix('}}')+']]')
        assert key not in seen, key
        seen.add(key)
        assert not violations(key), (key, violations(key))
        assert len(translated) == 15 and all(translated), key
        assert all(conversions(key) == conversions(value) for value in translated), key

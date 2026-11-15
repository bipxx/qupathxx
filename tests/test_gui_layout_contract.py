from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[1]


def test_icon_vocabulary_uses_current_enum_and_unique_command_pairs():
    source = (ROOT / 'cpp/gui_icons.hpp').read_text(encoding='utf8')
    names = [name.strip() for name in re.search(r'enum class Icon\s*\{([^}]+)', source, re.S)[1].split(',')]
    pairs = set()
    for line in (ROOT / 'tests/fixtures/gui-icon-vocabulary.tsv').read_text(encoding='utf8').splitlines():
        if not line or line.startswith('#'):
            continue
        ordinal, identity, name, meaning = line.split('\t')
        pair = (name, json.loads(identity))
        assert names[int(ordinal)] == name
        assert pair not in pairs and pair[1] and meaning
        pairs.add(pair)
    assert ('Trash', 'Remove metadata') in pairs
    assert ('Trash', 'Remove tag') in pairs
    assert ('Open', 'Recovery location') in pairs
    assert ('Stop', 'Cancel reading') in pairs
    assert not pairs.intersection({('Minus', 'Remove metadata'), ('Minus', 'Remove tag'),
                                   ('Settings', 'Recovery location'), ('Close', 'Cancel reading'),
                                   ('Save', 'Save class'), ('Edit', 'Rename')})

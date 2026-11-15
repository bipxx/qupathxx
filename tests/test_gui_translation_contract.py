from pathlib import Path
from gui_translation_coverage import compare, catalogue_keys, check_catalogue, load_fixture

ROOT = Path(__file__).resolve().parents[1]


def test_translation_fixture_has_only_missing_catalogue_keys():
    expected = load_fixture(ROOT)
    assert expected
    for surface, keys in expected.items():
        assert not surface.startswith('tool/')
        assert keys == sorted(set(keys))
        assert not keys, f'{surface}: in-scope captions must have catalogue rows'
    assert not check_catalogue(expected, catalogue_keys((ROOT/'cpp/gui_i18n_catalog.inc').read_text(encoding='utf8')))


def test_translation_ratchet_reports_new_caption():
    assert compare({'pane/Image': []}, {'pane/Image': ['Deliberate caption without a row']}) == [
        "pane/Image: new untranslated caption: ['Deliberate caption without a row']"]


def test_translation_ratchet_requires_lowering():
    assert compare({'pane/Image': ['Old caption']}, {'pane/Image': []}) == [
        "pane/Image: lower the fixture: ['Old caption']"]
    assert 'lower the fixture' in check_catalogue({'pane/Image': ['Old caption']}, {'Old caption'})[0]


def test_translation_ratchet_requires_every_surface():
    assert compare({'pane/Image': []}, {}) == ['pane/Image: localized surface was not measured']
    assert not compare({'pane/Image': []}, {'pane/Image': [], 'tool/Specialist': ['English caption']})

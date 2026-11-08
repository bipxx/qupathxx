"""Source/asset contracts for the native port of COLMAP WebView's system."""
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def test_palettes_match_captured_reference_tokens():
    reference = json.loads((ROOT/'tests/fixtures/colmap-design-system.json').read_text(encoding='utf8'))
    source = (ROOT/'cpp/gui_design_tokens.hpp').read_text(encoding='utf8')
    for mode, palette in reference['palettes'].items():
        body = re.search(r'constexpr Palette '+mode+r'\{([^}]+)', source).group(1)
        assert re.findall(r'0x([a-f0-9]{6})', body) == [color[1:] for color in palette.values()]


def test_pinned_sans_resources_and_license_are_shipped():
    source = (ROOT/'cmake/GuiFonts.cmake').read_text(encoding='utf8')
    provenance = json.loads((ROOT/'assets/fonts/font-source.json').read_text(encoding='utf8'))
    for prefix in ['SANS', 'SEMIBOLD']:
        name = re.search(r'set\(QPX_GUI_'+prefix+r'_NAME "([^"]+)"\)', source).group(1)
        size = int(re.search(r'set\(QPX_GUI_'+prefix+r'_BYTES (\d+)\)', source).group(1))
        digest = re.search(r'set\(QPX_GUI_'+prefix+r'_SHA256 "([^"]+)"\)', source).group(1)
        data = (ROOT/'assets/fonts'/name).read_bytes()
        assert len(data) == size
        assert hashlib.sha256(data).hexdigest() == digest == provenance['outputs'][name]['sha256']
        assert data[:4] == b'\x00\x01\x00\x00', 'Expected native SFNT/TTF'
    assert 'SIL OPEN FONT LICENSE Version 1.1' in (ROOT/'assets/fonts/IBMPlexSans-OFL.txt').read_text(encoding='utf8')


def test_reference_geometry_and_sans_roles_are_used_by_shared_components():
    tokens = (ROOT/'cpp/gui_design_tokens.hpp').read_text(encoding='utf8')
    assert 'font_size=14,panel_radius=8,control_radius=4,border=1' in tokens
    assert 'action_height=32,compact_height=24' in tokens
    fonts = (ROOT/'cpp/gui_fonts.cpp').read_text(encoding='utf8')
    assert 'add(assets.sans);add(assets.semibold)' in fonts
    assert 'AddFontDefaultBitmap' not in fonts
    assert 'assets.cjk' in fonts and 'assets.emoji' in fonts

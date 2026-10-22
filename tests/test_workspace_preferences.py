"""Config-root resolution is Unicode-safe and does not create preference files."""
import os
from pathlib import Path

import pytest
import qupathxx as q


def test_unicode_config_root_is_resolved_without_writing(tmp_path, monkeypatch):
    root = tmp_path/'\u7814\u7a76\u8bbe\u7f6e'
    monkeypatch.setenv('APPDATA' if os.name == 'nt' else 'XDG_CONFIG_HOME', str(root))
    result = Path(q.default_workspace_layout_path())
    assert result == root/'qupathxx'/'workspace-v1.ini'
    assert not root.exists()


def test_relative_config_root_does_not_redirect_preferences_into_working_directory(monkeypatch):
    monkeypatch.setenv('APPDATA' if os.name == 'nt' else 'XDG_CONFIG_HOME', 'relative-config')
    assert q.default_workspace_layout_path() == ''

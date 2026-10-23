"""Read original Windows QPS1 complete streams on every supported native backend."""
import hashlib
import json
from pathlib import Path
import pytest
import qupathxx as q

DATA=Path(__file__).parent/'data/pixel-streams'
MANIFEST=json.loads((DATA/'manifest.json').read_text(encoding='utf-8'))


@pytest.mark.parametrize('fixture',MANIFEST['fixtures'],ids=lambda f:f['name'])
def test_windows_stream_file_values_are_portable_without_training(fixture):
    if fixture['name']=='polygon-window' and not q.has_geos:pytest.skip('polygon topology requires GEOS')
    path=DATA/fixture['file'];assert path.stat().st_size==fixture['bytes'] and hashlib.sha256(path.read_bytes()).hexdigest()==fixture['file_sha256']
    file=q.PixelStreamFile(str(path));assert file.windowed==fixture['windowed'] and file.declared_summary.sha256==fixture['content_sha256']
    assert file.declared_summary.blocks==fixture['blocks'];file.verify();assert file.verified
    assert file.declared_summary.class_counts==fixture['class_counts']
    assert [file.read_block(i).sha256 for i in range(fixture['blocks'])]==fixture['numeric_sha256']

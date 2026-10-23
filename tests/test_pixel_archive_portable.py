"""Read and canonically re-encode Windows QPA1/QPA2 values on every platform."""
import hashlib
import json
from pathlib import Path
import pytest
import qupathxx as q

DATA=Path(__file__).resolve().parent/"data"/"pixel-archives"
MANIFEST=json.loads((DATA/"manifest.json").read_text(encoding="utf-8"))

@pytest.mark.parametrize("fixture",MANIFEST["fixtures"],ids=lambda f:f["name"])
def test_windows_pixel_archive_fixture_is_portable_without_training_backend(fixture):
    if fixture["name"]=="prediction" and not q.has_geos:
        pytest.skip("polygon topology validation requires GEOS")
    encoded=(DATA/fixture["file"]).read_bytes()
    assert len(encoded)==fixture["bytes"] and hashlib.sha256(encoded).hexdigest()==fixture["sha256"]
    artifact=q.PixelClassifierArchive.from_bytes(encoded)
    assert artifact.sha256==fixture["artifact_sha256"] and artifact.to_bytes()==encoded
    if artifact.table is not None:
        assert artifact.table.sha256==fixture["numeric_sha256"] and artifact.table.row_count==120
        rebuilt=q.archive_pixel_training(artifact.table,artifact.model)
    else:
        assert artifact.result is not None and artifact.result.sha256==fixture["numeric_sha256"]
        assert artifact.result.windowed==fixture.get("windowed",False)
        if "level_bounds" in fixture:
            bounds=artifact.result.level_bounds
            assert [bounds.x,bounds.y,artifact.result.width,artifact.result.height]==fixture["level_bounds"]
        rebuilt=q.archive_pixel_prediction(artifact.result)
    assert rebuilt.to_bytes()==encoded and rebuilt.sha256==artifact.sha256

"""Exercise the actual descriptor benchmark against the installed project format."""
import hashlib
import json
from pathlib import Path
import sqlite3
import subprocess
import sys

import pytest
import qupathxx as q
from test_pixel_stream_file import saved

pytestmark=pytest.mark.skipif(not q.has_project or not q.has_classifier_training,reason='project and classifier training required')


@pytest.mark.parametrize('records',[1,7])
def test_pixel_stream_record_benchmark_accepts_current_project_schema(tmp_path,records):
    _,_,_,stream,summary=saved(tmp_path)
    original=hashlib.sha256(stream.read_bytes()).hexdigest()
    project=tmp_path/'links.qpxproj';output=tmp_path/'report.json'
    script=Path(__file__).resolve().parents[1]/'tools/benchmark_pixel_stream_records.py'
    child=subprocess.run([sys.executable,'-I','-X','utf8',str(script),'--file',str(stream),'--records',str(records),'--project',str(project),'--output',str(output)],capture_output=True,text=True,encoding='utf-8',timeout=30)
    assert child.returncode==0,child.stdout+child.stderr
    result=json.loads(output.read_text(encoding='utf-8'))
    assert result['project_schema']==14 and result['records']==records
    assert result['environment']['git_revision']==q.build_environment().git_revision
    assert result['content_sha256']==summary.sha256
    assert result['max_encoded_descriptor_bytes']<4096 and result['total_encoded_descriptor_bytes']<records*4096
    with sqlite3.connect(project) as db:
        assert db.execute('PRAGMA user_version').fetchone()[0]==14
        assert db.execute('SELECT count(*) FROM pixel_stream_records').fetchone()[0]==records
    reopened=q.Project.open(str(project))
    assert sum(reopened.document(image.id).pixel_stream_record_count for image in reopened.images)==records
    assert hashlib.sha256(stream.read_bytes()).hexdigest()==original

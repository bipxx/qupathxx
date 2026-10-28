"""Owner serialization for older Document-free Python job controllers."""
from concurrent.futures import ThreadPoolExecutor

import pytest
import qupathxx as q


JOBS=[q.FileFingerprintJob,q.CellTableExportJob,q.MeasurementTableExportJob,
      q.ThumbnailJob,q.PixelStreamReadJob]


@pytest.mark.parametrize('job_type',JOBS,ids=lambda t:t.__name__)
@pytest.mark.parametrize('operation',['busy','poll','wait'])
def test_snapshot_controller_rejects_foreign_collection(job_type,operation):
    job=job_type()
    def foreign():
        value=getattr(job,operation)
        return value() if callable(value) else value
    with ThreadPoolExecutor(max_workers=1) as pool:
        with pytest.raises(RuntimeError,match='owner thread'):
            pool.submit(foreign).result(timeout=10)
    assert job.poll().state=='idle'


@pytest.mark.parametrize('job_type',JOBS,ids=lambda t:t.__name__)
def test_snapshot_controller_allows_foreign_cancel(job_type):
    job=job_type()
    with ThreadPoolExecutor(max_workers=1) as pool:
        pool.submit(job.cancel).result(timeout=10)
    assert job.poll().state=='idle'

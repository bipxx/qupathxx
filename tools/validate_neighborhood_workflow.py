"""Independent saved-neighborhood CSV count check (development oracle)."""
import csv
import json
from pathlib import Path
import sys

import qupathxx as q


def validate(output: Path):
    summary = json.loads((output / 'workflow.json').read_text(encoding='utf-8'))
    project = q.Project.open_without_journal_recovery(str(output / 'study.qpxproj'))
    result = project.document(summary['image_id']).neighborhood_record(summary['record_id']).result
    with (output / 'neighborhoods.csv').open(encoding='utf-8', newline='') as stream:
        rows = list(csv.DictReader(stream))
    assert len(rows) == result.row_count
    assert [int(row['count']) for row in rows] == [result.row(i).count for i in range(result.row_count)]
    assert summary['result_sha256'] == result.sha256
    return dict(csv_counts_equal=True, rows=len(rows))


if __name__ == '__main__':
    print(json.dumps(validate(Path(sys.argv[1]))))

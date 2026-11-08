"""Run saved-project density with the native workflow service.

python -I project_density.py selections.json new-output [--resume-from prior-output]
Requires a native-workflow-enabled qupathxx development install.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import signal
import threading
from typing import Callable

import qupathxx as q
from qupathxx._workflow_control import await_report

REPORT_SCHEMA = 'qpx.project-density-report/1'
MIB = 1024 * 1024
InputChanged = q.WorkflowInputChanged
CsvBudget = q.ProjectDensityCsvBudget
Cancelled = q.WorkflowCancelled


def run(manifest: Path, output: Path, *, resume_from: Path | None = None,
        cancelled: threading.Event | None = None, on_image: Callable | None = None,
        recompute: bool = False, stop_on_error: bool = False, csv_bytes: int = 64 * MIB,
        project_bytes: int = 1024 * MIB, checkpoint_bytes: int = 1024 * MIB) -> dict:
    options = q.ProjectDensityRunOptions()
    options.resume_from = '' if resume_from is None else str(resume_from)
    options.recompute, options.stop_on_error = recompute, stop_on_error
    options.csv_bytes, options.project_bytes = csv_bytes, project_bytes
    options.checkpoint_bytes = checkpoint_bytes
    options.pause_after_image = on_image is not None
    loaded = q.load_project_density_manifest(str(manifest))
    job = q.ProjectDensityJob()
    job.run(loaded, str(output), options)
    report = await_report(job, cancelled=cancelled, take=job.take_images,
                          acknowledge=job.continue_images, on_event=on_image)
    return json.loads(report.json)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path)
    parser.add_argument('output', type=Path, help='new directory; previous runs are never overwritten')
    parser.add_argument('--resume-from', type=Path)
    parser.add_argument('--recompute', action='store_true')
    parser.add_argument('--stop-on-error', action='store_true')
    parser.add_argument('--csv-bytes', type=int, default=64 * MIB)
    parser.add_argument('--project-bytes', type=int, default=1024 * MIB)
    parser.add_argument('--checkpoint-bytes', type=int, default=1024 * MIB)
    args = parser.parse_args(argv)
    cancelled = threading.Event()
    previous = signal.signal(signal.SIGINT, lambda *_: cancelled.set())
    try:
        report = run(args.manifest, args.output, resume_from=args.resume_from, cancelled=cancelled,
                     recompute=args.recompute, stop_on_error=args.stop_on_error,
                     csv_bytes=args.csv_bytes, project_bytes=args.project_bytes, checkpoint_bytes=args.checkpoint_bytes)
    except Cancelled as error:
        print(json.dumps({'schema': REPORT_SCHEMA, 'state': 'cancelled', 'error': str(error)}))
        return 130
    except Exception as error:
        print(json.dumps({'schema': REPORT_SCHEMA, 'state': 'failed', 'error': str(error)}))
        return 2
    finally:
        signal.signal(signal.SIGINT, previous)
    print(json.dumps({key: report[key] for key in ('schema', 'state', 'computed', 'reused', 'failed', 'not_run', 'rows', 'csv_path', 'report_path')}))
    return 130 if report['state'] == 'cancelled' else 0 if report['state'] == 'completed' else 1


if __name__ == '__main__':
    raise SystemExit(main())

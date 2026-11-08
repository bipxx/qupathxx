"""Owner-thread polling for native workflow jobs; no domain policy."""
from __future__ import annotations

import json
import time


def await_report(job, *, result="result", cancelled=None, take=None,
                 acknowledge=None, on_event=None):
    """Drain native events, deliver controls, and return the native report."""
    try:
        while True:
            job.poll()
            if take is not None:
                for event in take():
                    if on_event is not None:
                        try:
                            on_event(json.loads(event))
                        except Exception as error:
                            job.abort_control(str(error))
                    if cancelled is not None and cancelled.is_set():
                        job.cancel()
                    if acknowledge is not None:
                        acknowledge()
            if cancelled is not None and cancelled.is_set():
                job.cancel()
            if not job.busy:
                break
            time.sleep(0.01)
        job.rethrow_error()
        report = getattr(job, result)
        if report is None:
            raise RuntimeError("Native workflow returned no report")
        return report
    finally:
        if job.busy:
            job.cancel()
            job.finish()

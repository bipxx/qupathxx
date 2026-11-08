"""Predict an explicit saved-project ROI with a portable native pixel model.

python -I examples/pixel_classifier.py study.qpx model.qpc --image 1 --target 12
Synthetic raw-buffer training checks live in tools/demo_pixel_classifier.py.
"""
import argparse
import json
from pathlib import Path

import qupathxx as q


def run(document: q.Document, model: q.PixelClassifier, target_id: int,
        options: q.PixelPredictionOptions | None = None) -> q.PixelPredictionResult:
    request = q.PixelPredictionRequest(document, target_id, model,
                                       q.PixelPredictionOptions() if options is None else options)
    job = q.PixelPredictionJob()
    if not job.request(request):
        raise RuntimeError('Native pixel prediction was not accepted')
    status = job.wait(document)
    result = job.result
    if status.state != 'ready' or result is None:
        raise RuntimeError(status.error or status.state)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('project', type=Path)
    parser.add_argument('model', type=Path)
    parser.add_argument('--image', type=int, required=True)
    parser.add_argument('--target', type=int, required=True)
    args = parser.parse_args()
    project = q.Project.open_without_journal_recovery(str(args.project))
    document = project.document(args.image)
    try:
        result = run(document, q.load_pixel_classifier(str(args.model)), args.target)
        print(json.dumps(dict(classes=result.model.classes, counts=result.class_counts,
                              valid_pixels=result.valid_pixels, input_sha256=result.input_sha256,
                              model_sha256=result.model.sha256)))
    finally:
        document.server.close()


if __name__ == '__main__':
    main()

"""Train labelled project ROIs, save/reopen a native model and predict an ROI.

python -I examples/pixel_workflow.py study.qpx new.qpc --image 1 --label 10 --label 11 --target 12
Synthetic inputs and lifetime checks live in tools/demo_pixel_workflow.py.
"""
import argparse
import json
from pathlib import Path

import qupathxx as q


def run(document: q.Document, annotation_ids: list[int], target_id: int, model_path: Path,
        options: q.PixelTrainingOptions | None = None,
        parameters: q.ForestParameters | None = None) -> q.PixelPredictionResult:
    options = q.PixelTrainingOptions() if options is None else options
    parameters = q.pixel_forest_parameters() if parameters is None else parameters
    request = q.PixelTrainingRequest(document, annotation_ids, options=options)
    training = q.PixelTrainingJob()
    if not training.train(request, parameters):
        raise RuntimeError('Native pixel training was not accepted')
    status = training.wait(document)
    model = training.model
    if status.state != 'ready' or model is None:
        raise RuntimeError(status.error or status.state)
    q.save_pixel_classifier(model, str(model_path))
    restored = q.load_pixel_classifier(str(model_path))
    prediction = q.PixelPredictionJob()
    if not prediction.request(q.PixelPredictionRequest(document, target_id, restored)):
        raise RuntimeError('Native pixel prediction was not accepted')
    status = prediction.wait(document)
    result = prediction.result
    if status.state != 'ready' or result is None:
        raise RuntimeError(status.error or status.state)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('project', type=Path)
    parser.add_argument('model', type=Path, help='new native model; existing files are refused')
    parser.add_argument('--image', type=int, required=True)
    parser.add_argument('--label', type=int, action='append', required=True)
    parser.add_argument('--target', type=int, required=True)
    args = parser.parse_args()
    project = q.Project.open_without_journal_recovery(str(args.project))
    document = project.document(args.image)
    try:
        result = run(document, args.label, args.target, args.model)
        print(json.dumps(dict(classes=result.model.classes, counts=result.class_counts,
                              valid_pixels=result.valid_pixels, model=str(args.model),
                              model_sha256=result.model.sha256, input_sha256=result.input_sha256)))
    finally:
        document.server.close()


if __name__ == '__main__':
    main()

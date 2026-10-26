"""Train from existing labelled areas, resume native files, retain a new project.

Run with an installed training-enabled package:
  python examples/raw_classifier_workflow.py input.qpx output-directory --image-id 1
The output directory must be new. This uses explicit identity-normalized channel
intensities; it does not establish biological accuracy or stitched contour apply.
"""
import argparse
import json
from pathlib import Path

import qupathxx as q


def run(input_project: Path, output: Path, image_id: int, channels: int = 1) -> dict:
    output.mkdir(parents=True, exist_ok=False)
    project = q.Project.open(str(input_project))
    jobs = []
    try:
        doc = project.document(image_id)
        ids = [a.id for a in doc.annotations if a.class_name and a.geometry.is_area]
        width, height = doc.metadata.width, doc.metadata.height
        lattice = q.ScientificRawLattice(width, height, q.ScientificGridAxis(0, width, min(width, 256)), q.ScientificGridAxis(0, height, min(height, 256)))
        tiles = q.RawPixelTileOptions()
        tiles.width, tiles.height = min(64, max(1, lattice.x.samples // 2)), min(64, max(1, lattice.y.samples // 2))
        selection, features = q.ScientificRawOptions(), q.RawPixelFeatureOptions()
        selection.channels = channels
        features.offsets, features.scales = [0.] * channels, [1.] * channels
        request = q.RawPixelTiledTrainingRequest(doc, ids, lattice, selection, features, tiles=tiles)
        checkpoint = output / "training.qtc"
        sampling, save = q.RawPixelTrainingCheckpointJob(), q.RawPixelTrainingCheckpointOptions()
        jobs.append(sampling)
        save.max_new_windows = 1
        if not sampling.request(request, str(checkpoint), save) or sampling.wait(doc).state != "ready":
            raise RuntimeError("initial training checkpoint failed: " + sampling.poll(doc).error)
        assert sampling.take(doc) is not None
        save.resume_path, save.replace_existing, save.max_new_windows = str(checkpoint), True, 2**64 - 1
        if not sampling.request(request, str(checkpoint), save) or sampling.wait(doc).state != "ready":
            raise RuntimeError("training checkpoint resume failed: " + sampling.poll(doc).error)
        result = sampling.take(doc)
        if result is None or result.table is None:
            raise RuntimeError("training coverage did not complete")
        parameters = q.ForestParameters()
        parameters.trees, parameters.max_depth, parameters.work_bytes = 16, 8, 64 * 1024**2
        model = q.train_raw_pixel_tiled_classifier(result.table, parameters)
        q.save_raw_pixel_classifier(model, str(output / "model.qrm"))
        doc.keep_raw_pixel_tiled_training(request, q.archive_raw_pixel_tiled_training(result.table, model))
        prediction_options = q.RawPixelStreamOptions()
        prediction_options.tile_width, prediction_options.tile_height = tiles.width, tiles.height
        prediction = q.RawPixelTiledPredictionRequest(doc, model, lattice, prediction_options)
        stream, stream_options = q.RawPixelStreamSaveJob(), q.RawPixelStreamSaveOptions()
        jobs.append(stream)
        path = output / "prediction.qws"
        stream_options.max_new_windows = 1
        if not stream.request(prediction, str(path), stream_options) or stream.wait(doc).state != "ready":
            raise RuntimeError("initial prediction checkpoint failed: " + stream.poll(doc).error)
        assert stream.take(doc) is not None
        stream_options.resume_path, stream_options.replace_existing, stream_options.max_new_windows = str(path), True, 2**64 - 1
        if not stream.request(prediction, str(path), stream_options) or stream.wait(doc).state != "ready":
            raise RuntimeError("prediction resume failed: " + stream.poll(doc).error)
        link = stream.take_link(doc)
        if link is None or not link.summary.complete:
            raise RuntimeError("prediction coverage did not complete")
        doc.keep_raw_pixel_stream(link)
        saved = output / "analysis.qpx"
        project.save(str(saved))
        return dict(project=str(saved), rows=result.table.row_count, windows=link.summary.completed_windows,
                    model_sha256=model.sha256, stream_sha256=link.summary.sha256)
    finally:
        for job in jobs:
            job.cancel()
            job.await_completion()
        for image in project.images:
            project.document(image.id).server.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--image-id", type=int, required=True)
    parser.add_argument("--channels", type=int, default=1)
    args = parser.parse_args()
    print(json.dumps(run(args.project, args.output, args.image_id, args.channels), sort_keys=True))

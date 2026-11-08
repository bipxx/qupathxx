"""Control the native train/checkpoint/predict/retain project workflow.

Run with an installed training-enabled package:
  python examples/raw_classifier_workflow.py input.qpx output-directory --image-id 1
The output directory must be new. This uses explicit identity-normalized channel
intensities; it does not establish biological accuracy or stitched contour apply.
"""
import argparse
import json
from pathlib import Path

import qupathxx as q


def run(input_project: Path, output: Path, image_id: int, channels: int = 1) -> dict[str, object]:
    return json.loads(q.run_raw_classifier_workflow(str(input_project), str(output), image_id, channels))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--image-id", type=int, required=True)
    parser.add_argument("--channels", type=int, default=1)
    args = parser.parse_args()
    print(json.dumps(run(args.project, args.output, args.image_id, args.channels), sort_keys=True))

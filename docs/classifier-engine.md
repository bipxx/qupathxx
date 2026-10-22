# Native random forest engine

The C++ engine trains numeric classifiers, predicts tree votes, and round-trips
an immutable portable model through C++ and nanobind. This is the foundation
for object and pixel classification. Document feature snapshots, background
jobs, undoable publication, retained project provenance, and the classifier UI
are still required before either workflow is accepted.

## Backend and ownership

Training uses OpenCV **4.11.0** core/ML, the version used by the pinned
[QuPath v0.7.0 dependency catalog](https://github.com/qupath/qupath/blob/v0.7.0/gradle/libs.versions.toml).
The [RTrees public API](https://docs.opencv.org/4.x/d0/d65/classcv_1_1ml_1_1RTrees.html)
provides nodes, ordered splits and votes. No QuPath implementation code is
copied. `tools/build_opencv_sdk.py` verifies the source archive against a pinned
commit, length and SHA-256, and builds static core/ML with bundled zlib.
The installed SDK records configuration and hashes; CI validates cached bytes.
OpenCV, individual core/ML source, SoftFloat and zlib notices accompany enabled packages.

The trainer snapshots Python float32 matrices and uint32 labels while holding
the GIL, then releases it for native training. Arbitrary CPU array strides,
including negative strides, follow logical row/column order. Inputs are not
silently converted from other dtypes. The model owns feature descriptors,
classes, parameters and nodes; returned Python metadata cannot mutate it.

RTrees uses the calling thread's RNG. Training saves and restores that state
with an exception-safe guard, supplies an explicit nonzero 64-bit seed and a
fixed tree count, and leaves process-wide OpenCV thread settings alone.
Repeatability is checked within the same backend/build. Identical retraining
across compiler, platform or future backend versions is not promised.

## Numeric and scientific contract

The schema is an ordered list of unique UTF-8 feature names and explicit units
(`1` for dimensionless values). Prediction requires an exact match, including
units. Class indices follow the supplied unique class list; every training
class needs at least two rows. Labels need not be contiguous in the input row
order. Scaling, normalization, missing-value imputation, class balancing,
validation splits and hyperparameter search are not applied automatically.

Features must be finite float32 values with magnitude at most `FLT_MAX/4`.
This also excludes OpenCV's missing-value sentinel and avoids overflow when
the backend computes split midpoints. There are at most 100,000 rows, 128
features and 256 classes. Parameters bound depth to 16 and trees to 256.
The conservative work estimate includes a complete binary tree bound, backend
scratch, input copies and row storage; allocations are rejected before backend
training if they exceed the configured budget or the two-million-node bound.
The default budget is 256 MiB, with a 1 GiB ceiling. This is an allocation
estimate, not a hard process RSS limit.

Every tree contributes one class vote. The largest vote wins; ties choose the
first declared class. `vote_fraction` is winning votes divided by tree count,
**not a calibrated probability** or a measure of biological validity. All
training-row votes are checked against OpenCV's own traversal in bounded
1,024-row batches before a model is returned. Prediction output is limited to
eight million class votes per call; callers can split larger inputs into batches.

## Portable model

`to_bytes()` / `ForestModel::bytes()` encode a little-endian `QPXF` version-1
format containing parameters, training row count and input digest, backend
identifier, schema, class order, roots and flat nodes. Float thresholds retain
their exact IEEE-754 bit patterns. The model SHA-256 covers all encoded bytes;
the separate training digest covers domain/version, dimensions, schema,
classes, feature bits and labels. Neither digest is a signature.

Loading has a 64 MiB input ceiling. Counts and exact node payload length are
checked before node allocation; text is bounded UTF-8. Validation rejects bad
indices, cycles, shared nodes, unreachable nodes, excessive depth, invalid
classes, nonfinite thresholds, noncanonical leaves, truncation and trailing
bytes. Native inference and model loading work with OpenCV, Python, GUI,
project storage and interchange all disabled. Models are byte values at this
stage; atomic file saving and project associations belong to the upcoming
document workflow.

```python
import numpy as np
import qupathxx as q

features = [q.ClassifierFeature("area", "um2")]
x = np.array([[1], [2], [30], [40]], dtype=np.float32)
y = np.array([0, 0, 1, 1], dtype=np.uint32)
model = q.train_forest(x, y, features, ["small", "large"])
restored = q.ForestModel.from_bytes(model.to_bytes())
prediction = restored.predict(np.array([[35]], dtype=np.float32), features)[0]
print(restored.classes[prediction.class_index], prediction.vote_fraction)
```

Enable training with `QUPATHXX_BUILD_CLASSIFIER_TRAINING=ON`, set `OpenCV_DIR`
to the generated SDK (Linux: `lib/cmake/opencv4`), and set
`QUPATHXX_OPENCV_NOTICES` to its `share/licenses/opencv` directory. Training is
off by default; inference remains available. Full CI configurations enable it;
minimal CI verifies inference without the backend.

## Validation and performance scope

Native tests use an independently authored two-tree model to check the exact
threshold and adjacent float32 values, ties, schema mismatch and malformed
payloads. Training tests cover multiclass labels, repeatability, concurrent
trainers, backend vote equivalence and reloads. Python additionally checks
strict dtypes, array strides, metadata ownership and GIL-released calls.

`tools/benchmark_classifier.py` records a seeded numeric fixture: 10,000
training rows, eight features, three classes, 64 trees and depth ten, then five
80,000-row predictions. Timing includes input snapshots, native traversal and
Python result construction. It records every run, model/input hashes and build
identity. It excludes slide decoding, object feature extraction, publication
and GUI rendering. Synthetic label agreement is an engine sanity check; it
does not establish research accuracy, QuPath parity or a hardware performance
budget.

The [local validation record](benchmarks/classifier-engine-local-windows-linux.json)
checks the same wheel/native/source identities across the 1,327-test passing
installed Windows suite, ten classifier cases, 33 focused workspace/recovery/
classifier cases, native Windows/Linux configurations and strict typing. It
also retains the investigated fixed-duration GUI lock fixture failure and its
passing synchronized repeat. The [hosted artifact record](benchmarks/classifier-engine-ci-windows-linux.json)
verifies all five successful configurations and their package identities.
The local sample and all three hosted samples produce identical model bytes
for this seeded fixture, including Windows NumPy 1 and Linux/Windows NumPy 2.
This is bounded fixture evidence, not a general retraining guarantee.

The [local Windows sample](benchmarks/classifier-engine-windows.json) records
906 ms training and a 199 ms median for 80,000-row prediction, including Python
results, with a 544,863-byte model. All five timings and source/native/model
identities are retained. The measured synthetic threshold-label agreement is
97.50%; this is not biological accuracy.

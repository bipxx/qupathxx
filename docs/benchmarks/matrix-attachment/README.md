# Retry project attachment from a completed QMX

When an import creates a QMX and the subsequent project save fails, use the
completed QMX to finish the project. Supply its recorded file SHA-256, the same
input project and image, and a new project destination.

```text
qupathxx_format_import matrix-attach INPUT_PROJECT IMAGE_ID QMX FILE_SHA256 NEW_PROJECT
```

`matrix-info QMX` shows the file digest and stored frame for inspection. Use the
digest from the import receipt if you have one. If the selected image requires an
optional reader, set its manifest with `--reader-plugins ABS_MANIFEST` before
`matrix-attach`. The original matrix data, position table and matrix provider
are unnecessary.

The native service compares the complete QMX file digest and the stored
level-zero pixel frame to the current image. It uses existing project ownership
and atomic save, then reopens the saved project to verify the owned matrix.
The input project is a read-only template. The retry writes the managed QMX
beside the new project during Save As. A failed save can leave a complete
managed copy at the new location for a later retry. Existing destination
projects are refused. The project input guard also protects the managed copy
path, including paths reserved by offline linked stores.

Sixteen local Windows commands passed. Both counts and continuous imports first
encountered an actual save failure caused by an absent output parent. Retrying
with both original inputs absent retained exact QMX bytes, source, frame and
receipt in a fresh process without the provider. Invalid digests, a wrong
target frame, input aliases and existing project destinations were refused.
See the [local evidence](../../../build/gui-aggregation/catalogue-closure/matrix-attachment/results.json).

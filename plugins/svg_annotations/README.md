# SVG pixel annotation import

Use the optional `org.qupathxx.svg-annotations` provider to import a vector
overlay into a saved image. Version 1.1 requires a format host with annotation
ABI6. Root width and height must be whole pixels, written unitless or with a
`px` suffix. The host verifies that the declared canvas matches the saved
image's level-zero width and height before accepting the annotation preview.
The coordinates use the image's top-left origin.

```text
qupathxx_format_import --format-plugins ABS_FORMAT_MANIFEST annotations-import org.qupathxx.svg-annotations INPUT_PROJECT IMAGE_ID OVERLAY.svg NEW_PROJECT
```

Pass `--reader-plugins ABS_READER_MANIFEST` as a bootstrap option when the image
uses a reader plugin. The source project is a template and the output is a new
project. Shape IDs, titles, nearest named group and selected RGB paint are
retained with the native geometry.

The profile supports rectangles, circles, axis-aligned ellipses, lines,
polylines, polygons and straight paths using M, L, H, V and Z (including
relative forms). Closed even-odd paths can carry holes. Groups inherit
`transform`, `fill`, `stroke` and `fill-rule`. Transforms may use
`translate`, `scale` or a supplied affine `matrix`. A viewBox uses the
default centered meet mapping or explicit `none`. Hex and the basic named
RGB paints are accepted. Open polylines or paths require `fill="none"`
and visible stroke paint. [SVG basic shapes](https://www.w3.org/TR/SVG2/shapes.html) ·
[Viewports and transforms](https://www.w3.org/TR/SVG2/coords.html)

Curves, rounded rectangles, rotated or skewed ellipses, viewport clipping,
CSS, text geometry, and reference-based SVG documents are outside this
profile. Geometry is stored as a native ROI. Stroke width and general SVG
rendering have no representation here.

The AGPL-3.0-only module uses the pinned private Expat 2.10.0 source with
its original notices. Build Release with `QUPATHXX_FORMAT_PLUGIN_KIT_PREFIX`
and `QUPATHXX_SVG_EXPAT_SOURCE_DIR` set to the matching kit and admitted parser
source. The MIT-licensed svgwrite 1.4.3 writer is used only to generate the
development fixture.

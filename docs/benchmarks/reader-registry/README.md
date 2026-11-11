# Native reader catalog and consumer migration — 2026-11-10

The first delivery of the [reviewed plan](../../format-reader-plugin-plan.md)
is complete. A private fixed C++ catalog owns built-in reader selection/opening.
Project validation and lazy reads, desktop attachment/startup, region/RGB preview
and the admitted spatial/QuPath import routes use its existing-policy adapters.
The direct factories and public SDK/binding signatures remain intact.

`qupathxx_region --readers` emits `qpx.reader-catalog/1` JSON for `ppm`,
`openslide`, `tiff` and `ome-zarr`, including build availability and unavailable
reasons. Prefix an existing display command with `--reader ID` for explicit
selection, for example:

```text
qupathxx_region --reader tiff image.tif 0 0 0 512 512 region.pam
```

The raw CLI keeps its explicit TIFF contract. Catalog capability flags describe
provider upper bounds; they do not admit a source into an analysis workflow.
Legacy path policies intentionally differ between callers. Known disabled saved
readers still validate without probing files; source-dependent work refuses at
the existing factory boundary. RGB-nominal PPM retains its memory adapter and
other PPM routes retain file observations.

Validation used the existing Windows Release cache with Python OFF and no new
backend, binding, kernel or package build. Three workers owned tests/CI, CLI/import
routes and desktop routes; the parent owned the registry, Project and integration.
Compilation was serialized with an independent panel task. Development evidence
in the primary workspace's `build/reader-registry` records exact scoped inputs,
commands/logs, manifests and results:

- Thirteen core/CLI CTests pass: `reader_registry`, `reader_routes_cli`,
  `image_server`, `raw_image`, `source_identity`, `project`, `project_save`,
  `recovery`, `rgb_preview`, `qupath_project`, `ome_zarr_server`,
  `ome_zarr_project` and `backed_spatial_import`. The new tests exercise five
  frozen policies under all eight feature combinations; direct-factory RGBA/raw,
  validity, calibration and stable source digests; and CLI no-fallback/refusal
  behavior. The new SQLite payload checks were guarded by a private core macro
  missing from this test target, so they did **not** run in this historical
  build. The [plugin continuation](../reader-plugin-pilot/README.md) explicitly
  enables and passes them under its final source identity; the existing
  Project/save/recovery suites did run here. The cached CC0
  SVS fixture matches its admitted SHA-256. Core build source identity:
  `fc931523e918078dcbc14cc760bf328e5054c5ac05cad120bf2ae4d1dbb2aef4`.
- The coordinated GUI/app build succeeds; `application_entry` and `gui_zarr`
  pass, including native channel/plane/measurement/keep/save/reopen controls.
  Four app `--smoke` runs load tiles: explicit PPM, automatic OME-TIFF,
  automatic OME-Zarr and `--tiff-series 0` overriding explicit PPM. Fixture bytes
  remain exact. This intermediate frontend snapshot includes two independent
  panel edits and has source identity
  `eca08717811a19446ccffac222e11f1b31d59e43684bf9cbab6305455ca1c677`.
  It does not qualify the panel task's later source or final UI workflow.
- All 38 existing binding object hashes remain unchanged. The first CLI check
  failed because Windows CMake text output translated the expected PAM header
  to CRLF. A memory-hex oracle fixes the test while retaining exact product byte
  assertions; the corrected script passes without recompiling native code.
  Failed evidence is preserved.

No dynamic modules, plugin ABI, saved plugin-link schema, new formats, wheel,
release or platform campaign is delivered here. Candidate8 assets and their
qualification stay frozen. The next separate milestone is the verified standalone
binary plugin pilot, followed by saved plugin sources and deployment.

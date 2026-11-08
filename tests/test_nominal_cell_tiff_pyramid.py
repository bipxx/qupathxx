"""Level-zero nominal cells through the native openly licensed lossless TIFF reader."""
import os

import numpy as np
import pytest
import qupathxx as q
import tifffile

from test_nominal_cell_python import FIXTURE

pytestmark = pytest.mark.skipif(not q.has_tiff or not q.has_project, reason="requires native TIFF and project support")


def scene(tmp_path, layout):
    pixels = np.frombuffer((FIXTURE / "source.rgb").read_bytes(), dtype=np.uint8).reshape(192, 256, 3)
    source = tmp_path / (layout + "-pyramid.tif")
    def stored(values):
        return np.moveaxis(values, -1, 0) if layout == "separate" else values
    with tifffile.TiffWriter(source, bigtiff=True) as writer:
        writer.write(stored(pixels), tile=(16, 16), compression="deflate", subifds=1,
                     metadata=None, photometric="rgb", planarconfig=layout)
        writer.write(stored(np.full((96, 128, 3), 255, dtype=np.uint8)), tile=(16, 16),
                     compression="deflate", subfiletype=1, metadata=None,
                     photometric="rgb", planarconfig=layout)
    server = q.open_tiff(str(source))
    assert [level.downsample for level in server.metadata.levels] == [1, 2]
    assert np.all(server.read_region(q.RegionRequest(0, 0, 1, 128, 96)).numpy() == 255)
    np.testing.assert_array_equal(server.read_region(q.RegionRequest(0, 0, 0, 256, 192)).numpy()[..., :3], pixels)
    dense = q.Document(q.Image.from_numpy(pixels))
    linked = q.Document(server)
    for doc in (dense, linked):
        doc.set_calibration(.5, .5)
        doc.add_rectangle(q.Rect(0, 0, 256, 192))
    transform = q.StainTransform([[1, 0, 0], [0, 1, 0]])
    parameters = q.GridCellParameters()
    parameters.threshold = .4
    parameters.sigma = 1
    parameters.cell_expansion = 5
    parameters.min_nucleus_area = 16
    parameters.max_nucleus_area = 512
    parameters.contours.work_bytes = 1024 * 1024
    return dense, linked, transform, parameters


def compare(actual, expected):
    assert actual.cell_count == expected.cell_count == 3
    assert actual.nuclei.samples == expected.nuclei.samples
    assert actual.nuclei.foreground_pixels == expected.nuclei.foreground_pixels == 379
    assert actual.cells.foreground_pixels == expected.cells.foreground_pixels == 825
    assert actual.nuclei.labels == expected.nuclei.labels
    assert actual.cells.labels == expected.cells.labels
    for a, b in zip(actual.features, expected.features):
        assert a.nucleus_pixels == b.nucleus_pixels and a.cell_pixels == b.cell_pixels
        for field in ("nucleus_rgb", "cell_rgb", "nucleus_channels", "cell_channels", "cytoplasm_rgb", "cytoplasm_channels"):
            np.testing.assert_array_equal(getattr(a, field), getattr(b, field))
        assert actual.calibrated_feature(a.label).cell_sample_area_um2 == expected.calibrated_feature(b.label).cell_sample_area_um2


@pytest.mark.parametrize("layout", ["contig", "separate"])
@pytest.mark.parametrize("coordinates", [q.RgbNominalSampling.INTEGER_ORIGIN, q.RgbNominalSampling.AFFINE_INVERSE])
@pytest.mark.parametrize("context", [(0, 0), (3, 4)])
def test_lossless_pyramid_nominal_cells_match_dense_and_replay(tmp_path, layout, coordinates, context):
    dense, linked, transform, parameters = scene(tmp_path, layout)
    request = q.RgbNominalRequest(0, 0, 256, 192, 1.5, coordinates)
    np.testing.assert_array_equal(q.read_rgb_nominal_region(linked.server, request).numpy(),
                                  q.read_rgb_nominal_region(dense.server, request).numpy())
    descriptor = q.NominalCellSampling(request, *context)
    expected = q.detect_cells(q.CellRequest(dense, 1, transform, descriptor, parameters))
    captured = q.CellRequest(linked, 1, transform, descriptor, parameters)
    actual = q.detect_cells(captured)
    compare(actual, expected)
    assert actual.nuclei.input_sha256 == q.input_sha256(captured)
    assert actual.nuclei.input_format.endswith("/2")
    record = linked.keep_cells(captured, actual)
    project = q.Project()
    image = project.add_document(linked)
    assert project.images[0].reader == "tiff" and not project.images[0].embedded
    saved = tmp_path / "linked.qpxproj"
    project.save(str(saved))
    reopened = q.Project.open(str(saved)).document(image)
    assert reopened.server.tiff_view is not None and len(reopened.server.metadata.levels) == 2
    replay = q.CellRequest.replay(reopened, record)
    restored = q.detect_cells(replay)
    compare(restored, expected)
    assert restored.nuclei.input_sha256 == actual.nuclei.input_sha256
    assert reopened.verify_cells(record, replay, restored)


@pytest.mark.parametrize("layout", ["contig", "separate"])
def test_lossless_pyramid_nominal_cli_checkpoint_and_resume(tmp_path, layout):
    from test_nominal_cell_batches import invoke, flags

    dense, linked, transform, parameters = scene(tmp_path, layout)
    expected = q.detect_cells(q.CellRequest.for_nominal_roi(dense, 1, transform, 1.5, 0, 0, parameters))
    project = q.Project()
    image = project.add_document(linked)
    saved = tmp_path / "input.qpxproj"
    project.save(str(saved))
    destination = tmp_path / "batch"
    import json
    first = invoke(saved, destination, flags())
    assert first.returncode == 0, first.stderr
    report = json.loads(first.stdout.splitlines()[-1])
    assert (report["saved"], report["reused"]) == (1, 0)
    output = q.Project.open(report["images"][0]["path"]).document(image)
    compare(output.cell_records[0].result, expected)
    second = invoke(saved, destination, flags())
    assert second.returncode == 0, second.stderr
    resumed = json.loads(second.stdout.splitlines()[-1])
    assert (resumed["saved"], resumed["reused"]) == (0, 1)
    assert resumed["images"][0]["pixel_sha256"] == report["images"][0]["pixel_sha256"]


@pytest.mark.skipif(not q.has_gui or os.environ.get("QUPATHXX_TEST_GUI") != "1", reason="opt-in TIFF nominal desktop")
def test_lossless_pyramid_nominal_desktop_detect_keep_and_recheck(tmp_path):
    from test_gui_replay import event

    dense, linked, transform, parameters = scene(tmp_path, "separate")
    expected = q.detect_cells(q.CellRequest.for_nominal_roi(dense, 1, transform, 1.5, 3, 4, parameters))
    options = q.GuiOptions()
    nominal = q.NominalCellRoiSampling()
    nominal.context_x, nominal.context_y = 3, 4
    options.cell_nominal = nominal
    options.grid_cell_parameters = parameters
    options.inputs = [event(4, q.GuiInputKind.SELECT_ALL_KEY), event(8, q.GuiInputKind.DETECT_CELLS_KEY),
                      event(10, q.GuiInputKind.KEEP_CELL_RUN_KEY, wait=q.GuiInputWait.CELLS_READY),
                      event(12, q.GuiInputKind.RECHECK_CELL_RUN_KEY, wait=q.GuiInputWait.CELLS_ARCHIVED),
                      event(14, q.GuiInputKind.MOVE, wait=q.GuiInputWait.CELLS_VERIFIED)]
    report = q.show(linked, frame_limit=40, options=options)
    assert report.saved_cell_status == "current" and report.cell_preview_ready
    compare(report.cell_result, expected)

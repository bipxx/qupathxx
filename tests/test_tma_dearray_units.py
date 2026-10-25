"""Explicit physical size, anisotropic calibration, owned capture and recipe identity."""
import gc
import numpy as np
import pytest
import qupathxx as q
from test_tma_dearray import CONTRACT, scene


def physical_scene():
    case = CONTRACT['cases'][4]
    doc, options = scene(case)
    options.core_units = q.TmaCoreUnits.Micrometers
    options.core_width, options.core_height = 16, 12
    return case, doc, options


def test_existing_pixel_recipe_bytes_remain_unchanged():
    _, doc, _ = physical_scene()
    options = q.TmaDearrayOptions()
    options.core_width, options.core_height = 32, 48
    pixels = doc.server.read_region(q.RegionRequest(0, 0, 0, 480, 352))
    preview = q.detect_tma_grid(pixels, 480, 352, options)
    server = q.detect_tma_grid(doc.server, options)
    assert preview.recipe_sha256 == 'df11f43f560dc664cf69db984bbef2dc68d0a9f3f465aaa2728cf5df9ed3ea81'
    assert server.recipe_sha256 == 'd4ddb80af1dee623a90dfee3f9dfac3e0bbb4673abb7692bba8ef13988edcfe1'
    assert preview.core_size.units == q.TmaCoreUnits.Pixels
    assert preview.core_size.width_pixels == 32 and preview.core_size.height_pixels == 48
    assert preview.core_size.pixel_width_um is None and preview.core_size.pixel_height_um is None


def test_physical_capture_uses_document_override_and_owns_values():
    case, doc, options = physical_scene()
    doc.set_calibration(.5, .25)
    assert doc.server.metadata.pixel_width_um is None
    captured = q.tma_dearray_options_for_document(doc, options)
    assert captured.pixel_width_um == .5 and captured.pixel_height_um == .25
    assert options.pixel_width_um is None and options.pixel_height_um is None
    pixels = doc.server.read_region(q.RegionRequest(0, 0, 0, 480, 352))
    first = q.detect_tma_grid(doc.server, captured)
    doc.clear_calibration()
    del doc
    gc.collect()
    result = q.detect_tma_grid(pixels, 480, 352, captured)
    captured.pixel_width_um = 1
    assert result.core_size.width == 16 and result.core_size.height == 12
    assert result.core_size.width_pixels == 32 and result.core_size.height_pixels == 48
    assert result.core_size.pixel_width_um == .5 and result.core_size.pixel_height_um == .25
    assert not result.core_size.calibration_from_reader
    assert result.input_sha256 == first.input_sha256
    for actual, wire, expected in zip(result.cores, result.grid.cores, case['cores'], strict=True):
        assert np.hypot(actual.center.x-expected['center'][0], actual.center.y-expected['center'][1]) <= 2
        assert wire.missing != expected['present']
    with pytest.raises(AttributeError):
        result.core_size.width_pixels = 1


@pytest.mark.parametrize('pair', [(None, None), (.5, None), (None, .25), (0, .25), (-1, .25), (float('nan'), .25), (.5, float('inf')), (1e-300, .25), (1e300, .25)])
def test_unknown_invalid_or_unrepresentable_calibration_refuses(pair):
    _, doc, options = physical_scene()
    options.pixel_width_um, options.pixel_height_um = pair
    with pytest.raises(ValueError):
        q.detect_tma_grid(doc.server, options)
    assert doc.tma_core_count == 0


def test_document_calibration_is_authoritative_and_never_guessed():
    _, doc, options = physical_scene()
    with pytest.raises(ValueError, match='calibration is unknown'):
        q.tma_dearray_options_for_document(doc, options)
    doc.set_calibration(.5, .25)
    options.pixel_width_um, options.pixel_height_um = .75, .25
    with pytest.raises(ValueError, match='differs from document'):
        q.tma_dearray_options_for_document(doc, options)
    options.core_units = q.TmaCoreUnits.Pixels
    with pytest.raises(ValueError, match='micrometer'):
        options.validate()


def test_physical_recipe_binds_units_requested_size_and_calibration():
    _, doc, options = physical_scene()
    options.pixel_width_um, options.pixel_height_um = .5, .25
    pixels = doc.server.read_region(q.RegionRequest(0, 0, 0, 480, 352))
    original = q.detect_tma_grid(pixels, 480, 352, options)
    assert original.recipe_sha256 == q.detect_tma_grid(pixels, 480, 352, options).recipe_sha256
    options.pixel_width_um, options.pixel_height_um = 1, .5
    options.core_width, options.core_height = 32, 24
    equivalent = q.detect_tma_grid(pixels, 480, 352, options)
    assert equivalent.core_size.width_pixels == original.core_size.width_pixels
    assert equivalent.core_size.height_pixels == original.core_size.height_pixels
    assert equivalent.input_sha256 == original.input_sha256 and equivalent.recipe_sha256 != original.recipe_sha256

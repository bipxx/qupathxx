import math

import pytest

import qupathxx as q


def test_display_channels_and_raw_measurements():
    doc = q.Document(q.Image(1, 1, [30, 120, 210]))
    pixels = doc.server.read_region(q.RegionRequest(0, 0, 0, 1, 1))
    settings = q.DisplaySettings()
    assert q.display_pixels(pixels, settings) == bytes([30, 120, 210, 255])
    for mode, expected in [(q.DisplayMode.RED, 30), (q.DisplayMode.GREEN, 120), (q.DisplayMode.BLUE, 210), (q.DisplayMode.GRAY, 103)]:
        settings.mode = mode
        assert q.display_pixels(pixels, settings) == bytes([expected] * 3 + [255])
    assert pixels.data == bytes([30, 120, 210, 255])
    assert doc.mean_intensity(doc.add_rectangle(q.Rect(0, 0, 1, 1))) == 120


def test_display_window_gamma_alpha_and_validation():
    pixels = q.dense_image_server(q.Image(1, 1, [10, 110, 210])).read_region(q.RegionRequest(-1, 0, 0, 2, 1))
    settings = q.DisplaySettings()
    settings.black, settings.white, settings.gamma = 10, 210, 2
    assert q.display_pixels(pixels, settings) == bytes([0, 0, 0, 0, 0, 180, 255, 255])
    for name, value in [("black", -1), ("white", math.nan), ("gamma", 0), ("gamma", 21)]:
        invalid = q.DisplaySettings()
        setattr(invalid, name, value)
        with pytest.raises(ValueError):
            q.display_pixels(pixels, invalid)


def test_overview_center_preserves_zoom():
    view = q.Viewport()
    view.fit(10000, 5000, 1000, 700)
    zoom = view.zoom
    view.center_on(q.Point(8500, 750), 1000, 700)
    center = view.screen_to_image(q.Point(500, 350))
    assert (center.x, center.y) == pytest.approx((8500, 750))
    assert view.zoom == zoom

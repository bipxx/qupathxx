import qupathxx as q


def invalid(rgb: q.PixelClassifier, model: q.RawPixelClassifier, window: q.ScientificRawWindow, event: q.ViewerEvent, session: q.ViewerSession) -> None:
    raw = q.GuiRawPixelOptions()
    raw.model = rgb
    raw.contours = q.PixelPredictionOptions()
    raw.preview = q.ScientificGrid(8, 6, q.ScientificGridAxis(0, 8, 7), q.ScientificGridAxis(0, 6, 5))
    session.contour_raw_pixels(1, model, window)
    geometry: q.RawPixelContours = event.raw_pixel_contours

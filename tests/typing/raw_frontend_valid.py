import qupathxx as q


def desktop(settings: q.RawPixelTrainingSettings, model: q.RawPixelClassifier, window: q.ScientificRawWindow) -> q.GuiOptions:
    raw = q.GuiRawPixelOptions()
    raw.training, raw.model, raw.preview = settings, model, window
    raw.contours = q.TissueContourOptions()
    raw.contour_name_prefix, raw.contour_work_bytes = "Cells", 16 * 1024**2
    options = q.GuiOptions()
    options.raw_pixel = raw
    return options


def commands(session: q.ViewerSession, model: q.RawPixelClassifier, grid: q.ScientificGrid, window: q.ScientificRawWindow, settings: q.RawPixelTrainingSettings) -> list[int]:
    return [session.train_raw_pixels(1, [1, 2], settings),
        session.predict_raw_pixels(1, model, window),
        session.contour_raw_pixels(1, model, grid),
        session.contour_raw_pixels(1, model, grid, apply=True, expected_revision=3),
        session.read_raw_pixel_stream(1, "history.qws")]


def geometry_bytes(event: q.ViewerEvent, report: q.GuiReport) -> int:
    geometry: q.RawPixelContours | None = event.raw_pixel_contours
    raw: q.GuiRawPixelReport | None = report.raw_pixel
    if geometry is None or raw is None:
        return 0
    result: q.RawPixelContours | None = raw.contours
    return geometry.payload_bytes + (result.contours.vertex_count if result else 0)

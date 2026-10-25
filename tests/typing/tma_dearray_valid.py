import qupathxx as q


def preview(server: q.ImageServer, pixels: q.PixelBuffer) -> q.TmaDearrayResult:
    options = q.TmaDearrayOptions()
    options.core_width = 32
    options.core_height = 48
    options.polarity = q.TmaPolarity.DarkOnLight
    options.work_bytes = q.tma_dearray_work_bytes(pixels.width, pixels.height, options)
    result: q.TmaDearrayResult = q.detect_tma_grid(pixels, 480, 352, options)
    core: q.TmaDearrayCore = result.cores[0]
    error: float | None = core.fit_error
    plan: q.ThumbnailPlan | None = result.thumbnail_plan
    grid: q.QptmaData = result.grid
    q.qptma_text(grid)
    if plan is None and error is not None:
        options.work_bytes += 4 * 480 * 352
        return q.detect_tma_grid(server, options)
    return result

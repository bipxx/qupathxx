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


def managed(session: q.ViewerSession, image: int, options: q.TmaDearrayOptions) -> int:
    ticket = session.detect_tma_grid(image, options)
    event = session.wait(ticket)
    proposal = event.tma_dearray
    if proposal is not None:
        return session.execute(image, q.qptma_grid_edits(proposal.grid), expected_revision=event.document_revision)
    return ticket


def desktop(options: q.TmaDearrayOptions, report: q.GuiReport) -> q.GuiOptions:
    gui = q.GuiOptions()
    gui.tma_dearray_options = options
    held: q.TmaDearrayResult | None = report.tma_dearray_result
    if held is not None:
        print(held.payload_bytes)
    return gui

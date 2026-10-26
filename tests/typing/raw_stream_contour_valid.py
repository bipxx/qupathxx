import qupathxx as q

def streamed(doc: q.Document, request: q.RawPixelTiledPredictionRequest) -> list[int]:
    options=q.RawPixelStreamContourOptions()
    result: q.RawPixelStreamContours=q.trace_raw_pixel_stream(request,options)
    region: q.RawPixelStreamRegion=result.region
    counts: list[int]=result.class_counts
    job=q.RawPixelStreamContourJob()
    job.request(request,options)
    job.wait(doc)
    delivered: q.RawPixelStreamContours | None=job.result
    if delivered is not None:
        return q.publish_raw_pixel_stream_contours(doc,request,delivered)
    return []


def consumers(session: q.ViewerSession, image: int, model: q.RawPixelClassifier, lattice: q.ScientificRawLattice, report: q.GuiRawPixelReport, event: q.ViewerEvent) -> int:
    gui=q.GuiRawPixelOptions();gui.stream_contours=True
    gui.stream_contour_options=q.RawPixelStreamContourOptions()
    preview: q.RawPixelStreamContours | None=report.stream_contours
    delivered: q.RawPixelStreamContours | None=event.raw_pixel_stream_contours
    return session.contour_raw_pixel_stream(image,model,lattice,apply=True)


def contour_batch(model: q.RawPixelClassifier) -> q.ProjectBatchOptions:
    options=q.ProjectBatchOptions()
    options.kind=q.ProjectBatchKind.RAW_PIXEL_CONTOURS
    options.raw_pixel_model=model
    options.raw_contours=q.RawPixelStreamContourOptions()
    options.raw_contour_prefix="Contours"
    options.raw_contour_publication_bytes=64*1024**2
    return options

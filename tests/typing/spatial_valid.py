import qupathxx as q

def distances(doc: q.Document, sources: list[int], targets: list[int]) -> int:
    options=q.SpatialDistanceOptions();options.units=q.SpatialDistanceUnits.MICROMETRES
    request=q.SpatialDistanceRequest(doc,sources,targets,options)
    job=q.SpatialDistanceJob();job.request(request);job.wait(doc)
    result: q.SpatialDistanceResult | None=job.result
    if result is None:return 0
    for row in result.rows(limit=128):
        distance: float | None=row.distance
        target: int | None=row.target_id
        point: q.Point | None=row.target_centroid
    snapshot: q.SpatialObjectSnapshot=request.source_object(0)
    record=doc.keep_spatial(request,result)
    retained: q.SpatialDistanceRecord=doc.spatial_record(record)
    text: str=q.spatial_distance_csv(retained.result)
    written: int=q.save_spatial_distance_csv(result,'distances.csv',replace=False)
    replay=q.SpatialDistanceRequest.replay(doc,record)
    doc.verify_spatial(record,replay,q.measure_spatial_distances(replay))
    return written+len(text)+snapshot.id


def spatial_viewer(session: q.ViewerSession, gui: q.GuiOptions, report: q.GuiReport) -> int:
    options=q.GuiSpatialOptions();options.sources=[1];options.targets=[2]
    gui.spatial=options
    gui.spatial=None
    ticket: int=session.spatial_distances(1,[1],[2],keep=True,expected_revision=2)
    ticket=session.inspect_spatial(1,3)
    ticket=session.verify_spatial(1,3)
    ticket=session.export_spatial(1,3,'distances.csv',byte_limit=65536)
    event: q.ViewerEvent=session.poll(ticket)
    result: q.SpatialDistanceResult | None=event.spatial
    panel: q.GuiSpatialReport | None=report.spatial
    if panel is not None:
        result=panel.result
        rows: int=panel.export_rows
    return result.row_count if result is not None else 0

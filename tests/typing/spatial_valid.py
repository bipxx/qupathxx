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

import qupathxx as q

forged=q.SpatialDistanceResult()
job=q.SpatialDistanceJob()
distance: float=job.result.row(0).distance

def wrong(doc: q.Document, request: q.AnalysisRequest, result: q.AnalysisResult) -> None:
    doc.keep_spatial(request,result)

def nullable(row: q.SpatialDistanceRow) -> float:
    value: int=row.target_id
    point: q.Point=row.target_centroid
    return row.distance

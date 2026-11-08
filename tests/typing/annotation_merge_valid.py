import qupathxx as q

def merge(document: q.Document, viewer: q.ViewerSession, image: int, ids: list[int]) -> tuple[int, int]:
    object_id: int = document.merge_annotations(ids)
    ticket: int = viewer.merge_annotations(image, ids, discard_points=False, expected_revision=document.revision)
    return object_id, ticket

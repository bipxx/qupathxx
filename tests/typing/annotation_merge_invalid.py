import qupathxx as q

def invalid(document: q.Document, viewer: q.ViewerSession) -> None:
    document.merge_annotations(["a", "b"])
    document.merge_annotations([1, 2], discard_points="yes")
    viewer.merge_annotations(1, [1, 2], expected_revision="current")

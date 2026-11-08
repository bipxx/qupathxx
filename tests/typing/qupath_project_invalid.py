import qupathxx as q

def invalid(preview: q.QuPathProjectPreview) -> None:
    q.preview_qupath_project("source", source_overrides={1: 2})
    preview.import_new("out", allow_losses="yes")

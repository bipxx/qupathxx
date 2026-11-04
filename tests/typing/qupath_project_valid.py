import qupathxx as q

def migrate(path: str, output: str) -> q.Project:
    preview: q.QuPathProjectPreview = q.preview_qupath_project(path, source_overrides={"1": "slide.ome.tif"})
    images: list[q.QuPathProjectImage] = preview.images
    source_id: str = images[0].source_id
    assert source_id and not preview.consumed
    return preview.import_new(output, allow_losses=False)

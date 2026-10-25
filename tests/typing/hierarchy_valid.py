import qupathxx as q

def nested_export(doc: q.Document, path: str) -> q.InterchangeReport:
    options = q.GuiOptions()
    options.interchange_nested_hierarchy = True
    text: q.InterchangeReport = q.export_geojson(doc, nested_hierarchy=True)
    q.save_geojson(doc, path, allow_losses=False, nested_hierarchy=True)
    return text

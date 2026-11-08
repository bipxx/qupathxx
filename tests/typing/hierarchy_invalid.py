import qupathxx as q

def rejected(doc: q.Document) -> None:
    q.export_geojson(doc, nested_hierarchy="yes")
    q.save_geojson(doc, "output.geojson", nested_hierarchy=[True])
    options = q.GuiOptions()
    options.interchange_nested_hierarchy = "yes"
